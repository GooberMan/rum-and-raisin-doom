//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020 Ethan Watson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//  Sky rendering. The DOOM sky is a texture map like any
//  wall, wrapping around. A 1024 columns equal 360 degrees.
//  The default sky map is 256 columns and repeats 4 times
//  on a 320 screen?
//  
//


#include "doomdef.h"

#include "p_local.h"

#include "deh_str.h"

#include "r_sky.h"

// Needed for FRACUNIT.
#include "m_fixed.h"
#include "m_misc.h"

// Needed for Flat retrieval.
#include "r_local.h"

#include "m_container.h"
#include "m_jsonlump.h"
#include "m_profile.h"

#include "z_zone.h"

// Used to do SCREENHEIGHT/2 to get the mid point for sky rendering
// Turns out 100 is the magic number for any screen resolution
// Makes sense really, it is meant to be sky texture relative not screen relative
// So let's leave the ratio here to support arbitrary sky heights later
#define SKYHEIGHT 128
#define SKYMID ( SKYHEIGHT * 781250 / 1000000 )

//
// sky mapping
//
int											skyflatnum = 0;
skyflat_t*									skyflat = nullptr;

sideinstance_t								skyfakeline = {};
std::unordered_map< std::string, sky_t* >	skylookup;
std::unordered_map< int32_t, skyflat_t* >	skyflatlookup;

constexpr auto Lines( rasterregion_t* region )
{
	return std::span( region->lines, region->maxx - region->minx + 1 );
}

DOOM_C_API void R_InitSkyDefs()
{
	skyflatnum = R_FlatNumForName( DEH_String( SKYFLATNAME ) );
	texturecomposite_t* skyflatcomposite = flatlookup[ skyflatnum ];

	skyflat = (skyflat_t*)Z_Malloc( sizeof( skyflat_t ), PU_STATIC, nullptr );
	*skyflat = { skyflatcomposite, nullptr };

	skyflatlookup[ skyflatnum ] = skyflat;
	skyflatcomposite->skyflat = skyflat;

	auto ParseSkydef = []( const JSONElement& elem, const JSONLumpVersion& version ) -> jsonlumpresult_t
	{
		const JSONElement& skyarray = elem[ "skies" ];
		const JSONElement& flatmappings = elem[ "flatmapping" ];
		if( !skyarray.IsArray() ) return jl_parseerror;
		if( !( flatmappings.IsArray() || flatmappings.IsNull() ) ) return jl_parseerror;

		for( const JSONElement& skyelem : skyarray.Children() )
		{
			const JSONElement& skytex = skyelem[ "texturename" ];
			const JSONElement& mid = skyelem[ "texturemid" ];
			const JSONElement& type = skyelem[ "type" ];
			const JSONElement& palette = skyelem[ "firepalette" ];

			std::string skytexname = to< std::string >( skytex );
			int32_t tex = R_TextureNumForName( skytexname.c_str() );
			if( tex < 0 ) return jl_parseerror;

			sky_t* sky = (sky_t*)Z_Malloc( sizeof( sky_t ), PU_STATIC, nullptr );
			sky->texture = texturelookup[ tex ];
			sky->texnum = tex;
			sky->texturemid = DoubleToRendFixed( to< double_t >( mid ) );
			sky->type = to< skytype_t >( type );
			if( sky->type == st_fire )
			{
				if( !palette.IsArray() || palette.Children().size() != NumFirePaletteEntries ) return jl_parseerror;
				byte* output = sky->firepalette;
				for( const JSONElement& palentry : palette.Children() )
				{
					*output++ = to< byte >( palentry );
				}
			}

			skylookup[ skytexname ] = sky;
		}
		
		for( const JSONElement& flatentry : flatmappings.Children() )
		{
			const JSONElement& flatelem = flatentry[ "flat" ];
			const JSONElement& skyelem = flatentry[ "sky" ];

			std::string flatname = to< std::string >( flatelem );
			int32_t flatnum = R_FlatNumForName( flatname.c_str() );
			if( flatnum < 0 || flatnum >= R_GetNumFlats() ) return jl_parseerror;

			std::string skyname = to< std::string >( skyelem );
			sky_t* sky = R_GetSky( skyname.c_str() );

			texturecomposite_t* flatcomposite = flatlookup[ flatnum ];
			skyflat_t* flatentry = (skyflat_t*)Z_Malloc( sizeof( skyflat_t ), PU_STATIC, nullptr );
			*flatentry = { flatcomposite, sky };

			skyflatlookup[ flatnum ] = flatentry;
			flatcomposite->skyflat = flatentry;
		}

		return jl_success;
	};

	M_ParseJSONLump( "SKYDEFS", "skydefs", { 1, 0, 0 }, ParseSkydef );
}

DOOM_C_API sky_t* R_GetSky(const char* name)
{
	std::string skytexname = name;
	auto found = skylookup.find( name );
	if( found != skylookup.end() )
	{
		return found->second;
	}

	int32_t tex = R_TextureNumForName( skytexname.c_str() );
	if( tex < 0 ) return nullptr;

	sky_t* sky = (sky_t*)Z_Malloc( sizeof( sky_t ), PU_STATIC, nullptr );
	sky->texture = texturelookup[ texturetranslation[ tex ] ];
	sky->texnum = tex;
	if( comp.tall_skies )
	{
		sky->texturemid = sky->texture->renderheight - constants::skytexturemidoffset;
	}
	else
	{
		sky->texturemid = constants::skytexturemid;
	}
	sky->type = st_texture;

	skylookup[ name ] = sky;
	return sky;
}

DOOM_C_API void R_SetSky( const char* sky )
{
	sky_t* skydef = R_GetSky( sky );
	skyflat->sky = skydef;
	skyfakeline.toptex = skydef->texture;
	skyfakeline.sky = skydef;
}

void R_DrawSky( rendercontext_t& rendercontext, rasterregion_t* thisregion, sideinstance_t* skytextureline )
{
	M_PROFILE_FUNC();

	viewpoint_t& viewpoint = rendercontext.viewpoint;
	vbuffer_t& dest = rendercontext.viewbuffer;

	if( !skytextureline )
	{
		skytextureline = &skyfakeline;
	}
	sky_t* sky = skytextureline->sky;
	texturecomposite_t* texture = texturelookup[ texturetranslation[ sky->texnum ] ];

	constexpr rend_fixed_t skyoneunit = RendFixedDiv( IntToRendFixed( 1 ), IntToRendFixed( 256 ) );
	constexpr rend_fixed_t ninetydegree = IntToRendFixed( ANG90 );
	rend_fixed_t skyoffsetfixed = RendFixedMul( RendFixedMul( skytextureline->coloffset, skyoneunit ), ninetydegree );
	angle_t skyoffsetangle = RendFixedToInt( skyoffsetfixed );

	colcontext_t	skycontext = {};

	// Originally this would setup the column renderer for every instance of a sky found.
	// But we have our own context for it now. These are constants too, so you could cook
	// this once and forget all about it.
	skycontext.iscale = drs_current->skyiscaley;
	skycontext.colfunc = texture->wallrender;
	skycontext.colormap = rendercontext.viewpoint.colormaps;
	skycontext.texturemid = sky->texturemid;
	skycontext.texturemid +=  + skytextureline->rowoffset;

	// This isn't a constant though...
	skycontext.output = dest;
	skycontext.sourceheight = texture->renderheight;

	int32_t x = thisregion->minx;

	for( rasterline_t& line : Lines( thisregion ) )
	{
		if ( line.top <= line.bottom )
		{
			skycontext.yl = line.top;
			skycontext.yh = line.bottom;
			skycontext.x = x;

			int32_t angle = ( viewpoint.angle + skyoffsetangle + drs_current->xtoviewangle[x] ) >> ANGLETOSKYSHIFT;
			// Sky is allways drawn full bright,
			//  i.e. colormaps[0] is used.
			// Because of this hack, sky is not affected
			//  by INVUL inverse mapping.
			skycontext.source = R_GetColumnComposite( texture, angle );
			skycontext.colfunc( &skycontext );
		}
		++x;
	}
}

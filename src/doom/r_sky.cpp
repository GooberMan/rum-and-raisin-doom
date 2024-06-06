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

#include <array>

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
skyflat_t*									defaultskyflat = nullptr;

std::unordered_map< std::string, sky_t* >	skylookup;
std::unordered_map< int32_t, skyflat_t* >	skyflatlookup;

constexpr auto Lines( rasterregion_t* region )
{
	return std::span( region->lines, region->maxx - region->minx + 1 );
}

constexpr std::array< byte, 256*256 > GenerateSkyMixMap()
{
	std::array< byte, 256*256 > data;

	int32_t dest = 0;

	// Every entry starts off with a 100% blend
	for( [[maybe_unused]] int32_t fromindex : iota( 0, 256 ) )
	{
		for( int32_t toindex : iota( 0, 256 ) )
		{
			data[ dest++ ] = toindex;
		}
	}

	// But entry 0 is effectively a 0% blend.
	dest = 0;
	for( int32_t toindex : iota( 0, 256 ) )
	{
		data[ dest ] = toindex;
		dest += 256;
	}

	return data;
}

constexpr std::array< byte, 256*256 > skymixmap = GenerateSkyMixMap();

DOOM_C_API void R_InitSkyDefs()
{
	skyflatnum = R_FlatNumForName( DEH_String( SKYFLATNAME ) );
	texturecomposite_t* skyflatcomposite = flatlookup[ skyflatnum ];

	defaultskyflat = (skyflat_t*)Z_Malloc( sizeof( skyflat_t ), PU_STATIC, nullptr );
	*defaultskyflat = { skyflatcomposite, nullptr };

	skyflatlookup[ skyflatnum ] = defaultskyflat;
	skyflatcomposite->skyflat = defaultskyflat;

	auto ParseSkydef = []( const JSONElement& elem, const JSONLumpVersion& version ) -> jsonlumpresult_t
	{
		const JSONElement& skyarray = elem[ "skies" ];
		const JSONElement& flatmappings = elem[ "flatmapping" ];

		if( !( skyarray.IsArray() || skyarray.IsNull() ) ) return jl_parseerror;
		if( !( flatmappings.IsArray() || flatmappings.IsNull() ) ) return jl_parseerror;

		for( const JSONElement& skyelem : skyarray.Children() )
		{
			const JSONElement& type			= skyelem[ "type" ];

			const JSONElement& skytex		= skyelem[ "name" ];
			const JSONElement& mid			= skyelem[ "mid" ];
			const JSONElement& scrollx		= skyelem[ "scrollx" ];
			const JSONElement& scrolly		= skyelem[ "scrolly" ];

			const JSONElement& fireelem		= skyelem[ "fire" ];
			const JSONElement& foreelem		= skyelem[ "foregroundtex" ];

			skytype_t skytype = to< skytype_t >( type );
			if( skytype < sky_texture || skytype > sky_backandforeground ) return jl_parseerror;

			std::string skytexname = to< std::string >( skytex );
			int32_t tex = R_TextureNumForName( skytexname.c_str() );
			if( tex < 0 ) return jl_parseerror;

			sky_t* sky = (sky_t*)Z_MallocZero( sizeof( sky_t ), PU_STATIC, nullptr );

			sky->type = skytype;

			sky->background.texture = texturelookup[ tex ];
			sky->background.texnum = tex;
			sky->background.mid = DoubleToRendFixed( to< double_t >( mid ) );
			sky->background.scrollx = DoubleToRendFixed( to< double_t >( scrollx ) );
			sky->background.scrolly = DoubleToRendFixed( to< double_t >( scrolly ) );

			if( sky->type == sky_fire )
			{
				if( !fireelem.IsElement() ) return jl_parseerror;

				const JSONElement& firepalette	= fireelem[ "palette" ];
				const JSONElement& fireticrate	= fireelem[ "ticrate" ];

				if( !firepalette.IsArray() ) return jl_parseerror;
				sky->numfireentries = (int32_t)firepalette.Children().size();
				byte* output = sky->firepalette = (byte*)Z_Malloc( sizeof( byte ) * sky->numfireentries, PU_STATIC, nullptr );
				for( const JSONElement& palentry : firepalette.Children() )
				{
					*output++ = to< byte >( palentry );
				}
			}
			else if( sky->type = sky_backandforeground )
			{
				if( !foreelem.IsElement() ) return jl_parseerror;

				const JSONElement& foreskytex	= foreelem[ "name" ];
				const JSONElement& foremid		= foreelem[ "mid" ];
				const JSONElement& forescrollx	= foreelem[ "scrollx" ];
				const JSONElement& forescrolly	= foreelem[ "scrolly" ];

				std::string foreskytexname = to< std::string >( foreskytex );
				int32_t foretex = R_TextureNumForName( foreskytexname.c_str() );
				if( foretex < 0 ) return jl_parseerror;

				sky->foreground.texture = texturelookup[ foretex ];
				sky->foreground.texnum = foretex;
				sky->foreground.mid = DoubleToRendFixed( to< double_t >( foremid ) );
				sky->foreground.scrollx = DoubleToRendFixed( to< double_t >( forescrollx ) );
				sky->foreground.scrolly = DoubleToRendFixed( to< double_t >( forescrolly ) );
			}
			else
			{
				if( !fireelem.IsNull() || !foreelem.IsNull() ) return jl_parseerror;
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
			sky_t* sky = R_GetSky( skyname.c_str(), true );

			texturecomposite_t* flatcomposite = flatlookup[ flatnum ];
			skyflat_t* flatentry = (skyflat_t*)Z_MallocZero( sizeof( skyflat_t ), PU_STATIC, nullptr );
			*flatentry = { flatcomposite, sky };

			skyflatlookup[ flatnum ] = flatentry;
			flatcomposite->skyflat = flatentry;
		}

		return jl_success;
	};

	M_ParseJSONLump( "SKYDEFS", "skydefs", { 1, 0, 0 }, ParseSkydef );
}

DOOM_C_API void R_InitSkiesForLevel()
{
	for( auto& skypair : skylookup )
	{
		skypair.second->active = false;
		skypair.second->foreground.currx = 0;
		skypair.second->foreground.curry = 0;
		skypair.second->background.currx = 0;
		skypair.second->background.curry = 0;
	}
}

DOOM_C_API sky_t* R_GetSky( const char* name, doombool create )
{
	std::string skytexname = name;
	auto found = skylookup.find( name );
	if( found != skylookup.end() )
	{
		return found->second;
	}

	if( !create )
	{
		return nullptr;
	}

	int32_t tex = R_TextureNumForName( skytexname.c_str() );
	if( tex < 0 ) return nullptr;

	sky_t* sky = (sky_t*)Z_MallocZero( sizeof( sky_t ), PU_STATIC, nullptr );
	sky->background.texture = texturelookup[ texturetranslation[ tex ] ];
	sky->background.texnum = tex;
	if( comp.tall_skies )
	{
		sky->background.mid = sky->background.texture->renderheight - constants::skytexturemidoffset;
	}
	else
	{
		sky->background.mid = constants::skytexturemid;
	}
	sky->type = sky_texture;

	skylookup[ name ] = sky;
	return sky;
}

DOOM_C_API void R_SetDefaultSky( const char* sky )
{
	sky_t* skydef = R_GetSky( sky, true );
	defaultskyflat->sky = skydef;
}

DOOM_C_API void R_ActivateSky( sky_t* sky )
{
	sky->active = true;
}

DOOM_C_API void R_ActivateSkyAndAnims( int32_t texnum )
{
	if( sky_t* sky = R_GetSky( texturelookup[ texnum ]->name, false ) )
	{
		R_ActivateSky( sky );
	}
	else
	{
		return;
	}

	int32_t numtextures = R_GetNumTextures();
	if( texnum < numtextures )
	{
		int32_t animstart = P_GetPicAnimStart( true, texnum );
		if( animstart >= 0 )
		{
			int32_t animend = animstart + P_GetPicAnimLength( true, animstart );
			for( int32_t thisanim : iota( animstart, animend ) )
			{
				if( sky_t* sky = R_GetSky( texturelookup[ thisanim ]->name, false ) )
				{
					R_ActivateSky( sky );
				}
			}
		}
	}
	else
	{
		int32_t flatnum = texnum - numtextures;
		int32_t animstart = P_GetPicAnimStart( false, flatnum );
		if( animstart >= 0 )
		{
			int32_t animend = animstart + P_GetPicAnimLength( false, animstart );
			for( int32_t thisanim : iota( animstart, animend ) )
			{
				if( sky_t* sky = R_GetSky( flatlookup[ thisanim ]->name, false ) )
				{
					R_ActivateSky( sky );
				}
			}
		}
	}
}

static void R_UpdateFireSky( sky_t* sky )
{
}

static void R_UpdateSky( sky_t* sky )
{
	sky->foreground.currx += sky->foreground.scrollx;
	sky->foreground.curry += sky->foreground.scrolly;

	sky->background.currx += sky->background.scrollx;
	sky->background.curry += sky->background.scrolly;

	if( sky->type == sky_fire )
	{
		R_UpdateFireSky( sky );
	}
}

DOOM_C_API void R_UpdateSky()
{
	for( auto& skypair : skylookup )
	{
		if( skypair.second->active )
		{
			R_UpdateSky( skypair.second );
		}
	}
}

void R_DrawSky( rendercontext_t& rendercontext, rasterregion_t* thisregion, sky_t* basesky, sideinstance_t* skytextureline )
{
	constexpr rend_fixed_t	skyoneunit = RendFixedDiv( IntToRendFixed( 1 ), IntToRendFixed( 256 ) );
	constexpr int64_t		hackfidelityshift = 16ll;
	constexpr rend_fixed_t	ninetydegree = IntToRendFixed( ANG90 ) >> hackfidelityshift;

	M_PROFILE_FUNC();

	sideinstance_t skyfakeline = {};

	if( !skytextureline )
	{
		skytextureline = &skyfakeline;
		skytextureline->toptex = basesky->background.texture;
		skytextureline->sky = basesky;
	}

	constexpr auto RenderSkyTex = []( rendercontext_t& rendercontext, rasterregion_t* region
									, sideinstance_t* line, skytex_t& tex, bool foreground )
	{
		viewpoint_t& viewpoint = rendercontext.viewpoint;
		vbuffer_t& dest = rendercontext.viewbuffer;

		sky_t* sky = line->sky;
		texturecomposite_t* texture = texturelookup[ texturetranslation[ tex.texnum ] ];

		rend_fixed_t xoffset = line->coloffset + tex.currx;
		rend_fixed_t xskyunit = RendFixedMul( xoffset, skyoneunit );
		rend_fixed_t skyoffsetfixed = RendFixedMul( xskyunit, ninetydegree ) << hackfidelityshift;
		angle_t skyoffsetangle = RendFixedToInt( skyoffsetfixed );

		colcontext_t	skycontext = {};

		skycontext.iscale = drs_current->skyiscaley;
		skycontext.colfunc = foreground ? texture->transparentwallrender : texture->wallrender;
		skycontext.transparency = foreground ? (byte*)skymixmap.data() : nullptr;
		skycontext.colormap = rendercontext.viewpoint.colormaps;
		skycontext.texturemid = tex.mid + line->rowoffset + tex.curry;
		skycontext.output = dest;
		skycontext.sourceheight = texture->renderheight;

		int32_t x = region->minx;

		for( rasterline_t& line : Lines( region ) )
		{
			if ( line.top <= line.bottom )
			{
				skycontext.yl = line.top;
				skycontext.yh = line.bottom;
				skycontext.x = x;

				int32_t angle = ( viewpoint.angle + skyoffsetangle + drs_current->xtoviewangle[ x ] ) >> ANGLETOSKYSHIFT;
				// Sky is allways drawn full bright,
				//  i.e. colormaps[0] is used.
				// Because of this hack, sky is not affected
				//  by INVUL inverse mapping.
				skycontext.source = R_GetColumnComposite( texture, angle );
				skycontext.colfunc( &skycontext );
			}
			++x;
		}
	};

	RenderSkyTex( rendercontext, thisregion, skytextureline, skytextureline->sky->background, false );
	if( skytextureline->sky->type == sky_backandforeground )
	{
		RenderSkyTex( rendercontext, thisregion, skytextureline, skytextureline->sky->foreground, true );
	}
}

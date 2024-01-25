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
#include "r_sky.h"

// Needed for FRACUNIT.
#include "m_fixed.h"
#include "m_misc.h"

// Needed for Flat retrieval.
#include "r_local.h"

#include "m_container.h"
#include "m_profile.h"

// Used to do SCREENHEIGHT/2 to get the mid point for sky rendering
// Turns out 100 is the magic number for any screen resolution
// Makes sense really, it is meant to be sky texture relative not screen relative
// So let's leave the ratio here to support arbitrary sky heights later
#define SKYHEIGHT 128
#define SKYMID ( SKYHEIGHT * 781250 / 1000000 )

//
// sky mapping
//
int						skyflatnum;
int						skytexture;
sideinstance_t			skyfakeline = {};

constexpr auto Lines( rasterregion_t* region )
{
	return std::span( region->lines, region->maxx - region->minx + 1 );
}

DOOM_C_API void R_SetSkyTexture( int32_t texnum )
{
	skytexture = texnum;
	skyfakeline.toptex = texturelookup[ skytexture ];
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
	texturecomposite_t* sky = skytextureline->toptex;

	constexpr rend_fixed_t skyoneunit = RendFixedDiv( IntToRendFixed( 1 ), IntToRendFixed( 256 ) );
	constexpr rend_fixed_t ninetydegree = IntToRendFixed( ANG90 );
	rend_fixed_t skyoffsetfixed = RendFixedMul( RendFixedMul( skytextureline->coloffset, skyoneunit ), ninetydegree );
	angle_t skyoffsetangle = RendFixedToInt( skyoffsetfixed );

	colcontext_t	skycontext;

	skycontext.colfunc = &R_DrawColumn;

	// Originally this would setup the column renderer for every instance of a sky found.
	// But we have our own context for it now. These are constants too, so you could cook
	// this once and forget all about it.
	skycontext.iscale = drs_current->skyiscaley;
	skycontext.texturemid = constants::skytexturemid + skytextureline->rowoffset;

	// This isn't a constant though...
	skycontext.output = dest;
	skycontext.sourceheight = sky->renderheight;

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
			skycontext.source = R_GetColumnComposite( sky, angle );
			skycontext.colfunc( &skycontext );
		}
		++x;
	}
}

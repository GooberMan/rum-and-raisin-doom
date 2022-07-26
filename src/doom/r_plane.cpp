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
//	Here is a core component: drawing the floors and ceilings,
//	 while maintaining a per column clipping list only.
//	Moreover, the sky areas have to be determined.
//

#include "doomdef.h"
#include "doomtype.h"
#include "m_fixed.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_raster.h"
#include "r_sky.h"

extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>

	#include "i_system.h"
	#include "z_zone.h"
	#include "w_wad.h"

	#include "m_misc.h"

	//
	// Constants. Don't need to be in a context. Will get them off the stack at some point though.
	//
	#define MAXWIDTH	( MAXSCREENWIDTH + ( MAXSCREENWIDTH >> 1) )
	#define MAXHEIGHT	( MAXSCREENHEIGHT + ( MAXSCREENHEIGHT >> 1) )

	rend_fixed_t		yslope[ MAXSCREENHEIGHT ];
	rend_fixed_t		distscale[ MAXSCREENWIDTH ];

	int32_t				span_override = Span_None;

	extern size_t		xlookup[MAXWIDTH];
	extern size_t		rowofs[MAXHEIGHT];

	extern int			numflats;
	extern int			numtextures;
}

#include "m_container.h"
#include "m_profile.h"

//
// R_InitPlanes
// Only at game startup.
//
DOOM_C_API void R_InitPlanes (void)
{
  // Doh!
}

//
// R_ClearPlanes
// At begining of frame.
//
DOOM_C_API void R_ClearPlanes ( planecontext_t* context, int32_t width, int32_t height, angle_t thisangle )
{
	M_PROFILE_FUNC();

	int32_t	i;
	angle_t	angle;

	{
		M_PROFILE_NAMED( "Clip clear" );
		context->floorclip = R_AllocateScratch< vertclip_t >( viewwidth, viewheight );
		context->ceilingclip = R_AllocateScratch< vertclip_t >( viewwidth, -1 );
	}

	context->lastopening = context->openings;

	memset( context->rasterregions, 0, sizeof( rasterregion_t* ) * ( numflats + numtextures ) );

	context->raster = R_AllocateScratch< rastercache_t >( viewheight );

	// left to right mapping
	angle = (thisangle-ANG90)>>RENDERANGLETOFINESHIFT;
}

//
// R_FindPlane
//

DOOM_C_API rasterregion_t* R_AddNewRasterRegion( planecontext_t* context, int32_t picnum, fixed_t height, int32_t lightlevel, int32_t start, int32_t stop )
{
	constexpr rasterline_t defaultline = { VPINDEX_INVALID, 0 };
	int16_t width = stop - start + 1;

	rasterregion_t* region = R_AllocateScratch< rasterregion_t >( 1 );
	region->height = FixedToRendFixed( height );
	region->lightlevel = lightlevel;
	region->minx = start;
	region->maxx = stop;
	region->miny = render_height;
	region->maxy = -1;

	if( width > 0 )
	{
		region->lines = R_AllocateScratch< rasterline_t >( width, defaultline );
	}
	else
	{
		region->lines = nullptr;
	}

	if( picnum != skyflatnum )
	{
		region->nextregion = NULL;
	}
	else
	{
		region->nextregion = context->rasterregions[ picnum ];
		context->rasterregions[ picnum ] = region;
	}

	return region;
}

#ifdef RANGECHECK
DOOM_C_API void R_ErrorCheckPlanes( rendercontext_t* context )
{
	M_PROFILE_FUNC();

	if ( context->bspcontext.thisdrawseg - context->bspcontext.drawsegs > MAXDRAWSEGS)
	{
		I_Error ("R_DrawPlanes: drawsegs overflow (%" PRIiPTR ")",
				context->bspcontext.thisdrawseg - context->bspcontext.drawsegs);
	}

	if (context->planecontext.lastopening - context->planecontext.openings > MAXOPENINGS)
	{
		I_Error ("R_DrawPlanes: opening overflow (%" PRIiPTR ")",
				context->planecontext.lastopening - context->planecontext.openings);
	}
}
#endif

//
// R_DrawPlanes
// At the end of each frame.
//

constexpr auto Lines( rasterregion_t* region )
{
	return std::span( region->lines, region->maxx - region->minx + 1 );
}

auto Surfaces( planecontext_t* context )
{
	return std::span( context->rasterregions, numflats + numtextures );
}

// This now only draws the sky plane, need to make this way nicer
DOOM_C_API void R_DrawPlanes( vbuffer_t* dest, planecontext_t* planecontext )
{
	M_PROFILE_FUNC();

	colcontext_t	skycontext;

	skycontext.colfunc = colfuncs[ M_MIN( ( ( pspriteiscale >> detailshift ) >> 12 ), 15 ) ];

	// Originally this would setup the column renderer for every instance of a sky found.
	// But we have our own context for it now. These are constants too, so you could cook
	// this once and forget all about it.
	skycontext.iscale = FixedToRendFixed( pspriteiscale>>detailshift );
	skycontext.texturemid = skytexturemid;

	// This isn't a constant though...
	skycontext.output = *dest;
	skycontext.sourceheight = texturelookup[ skytexture ]->renderheight;

	// TODO: Sort visplanes by height
	rasterregion_t* region = Surfaces( planecontext )[ skyflatnum ];
	for( rasterregion_t* thisregion : RegionRange( region ) )
	{
		int32_t x = thisregion->minx;

		for( rasterline_t& line : Lines( thisregion ) )
		{
			if ( line.top <= line.bottom )
			{
				skycontext.yl = line.top;
				skycontext.yh = line.bottom;
				skycontext.x = x;

				int32_t angle = ( viewangle + xtoviewangle[x] ) >> ANGLETOSKYSHIFT;
				// Sky is allways drawn full bright,
				//  i.e. colormaps[0] is used.
				// Because of this hack, sky is not affected
				//  by INVUL inverse mapping.
				skycontext.source = R_GetColumn( skytexture, angle, 0 );
				skycontext.colfunc( &skycontext );
			}
			++x;
		}
	}
}

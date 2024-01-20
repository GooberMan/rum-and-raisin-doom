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

	extern int			numflats;
	extern int			numtextures;
}

#include "m_container.h"
#include "m_profile.h"

//
// R_ClearPlanes
// At begining of frame.
//
DOOM_C_API void R_ClearPlanes( planecontext_t* context, int32_t width, int32_t height )
{
	M_PROFILE_FUNC();

	{
		M_PROFILE_NAMED( "Clip clear" );
		context->floorclip = R_AllocateScratch< vertclip_t >( drs_current->viewwidth, drs_current->viewheight );
		context->ceilingclip = R_AllocateScratch< vertclip_t >( drs_current->viewwidth, -1 );
	}

	context->lastopening = context->openings;

	memset( context->rasterregions, 0, sizeof( rasterregion_t* ) * ( numflats + numtextures ) );

	context->raster = R_AllocateScratch< rastercache_t >( drs_current->viewheight );
}

//
// R_FindPlane
//

DOOM_C_API rasterregion_t* R_AddNewRasterRegion( planecontext_t* context, int32_t picnum, rend_fixed_t height, int32_t lightlevel, int32_t start, int32_t stop )
{
	constexpr rasterline_t defaultline = { VPINDEX_INVALID, 0 };
	int16_t width = stop - start + 1;

	rasterregion_t* region = R_AllocateScratch< rasterregion_t >( 1 );
	region->height = height;
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

#if RANGECHECK
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

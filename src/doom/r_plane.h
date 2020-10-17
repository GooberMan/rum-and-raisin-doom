//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
//	Refresh, visplane stuff (floor, ceilings).
//


#ifndef __R_PLANE__
#define __R_PLANE__


#include "r_data.h"



// Visplane related.
extern  vertclip_t*		lastopening;

extern vertclip_t	floorclip[SCREENWIDTH];
extern vertclip_t	ceilingclip[SCREENWIDTH];

extern fixed_t		yslope[SCREENHEIGHT];
extern fixed_t		distscale[SCREENWIDTH];

void R_InitPlanes (void);
void R_ClearPlanes ( planecontext_t* context, int32_t width, int32_t height, int32_t thisangle );

void R_MapPlane( spancontext_t* context, int y, int x1, int x2 );
void R_MakeSpans( spancontext_t* context, int x, int t1, int b1, int t2, int b2 );

#ifdef RANGECHECK
void R_ErrorCheckPlanes( rendercontext_t* context );
#else
#define R_ErrorCheckPlanes( context )
#endif // RANGECHECK

void R_DrawPlanes ( );

visplane_t*
R_FindPlane
( fixed_t	height,
  int		picnum,
  int		lightlevel );

visplane_t*
R_CheckPlane
( visplane_t*	pl,
  int		start,
  int		stop );



#endif

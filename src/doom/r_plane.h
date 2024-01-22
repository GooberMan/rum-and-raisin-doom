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
//	Refresh, visplane stuff (floor, ceilings).
//


#ifndef __R_PLANE__
#define __R_PLANE__


#include "r_data.h"

#if defined(__cplusplus)

void R_ClearPlanes( planecontext_t* context, int32_t width, int32_t height );

#if RANGECHECK
void R_ErrorCheckPlanes( rendercontext_t& context );
#else
#define R_ErrorCheckPlanes( context )
#endif // RANGECHECK

rasterregion_t* R_AddNewRasterRegion( planecontext_t& context, int32_t picnum, rend_fixed_t height, int32_t lightlevel, int32_t start, int32_t stop );

#endif // defined(__cplusplus)

#endif

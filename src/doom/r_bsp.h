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
//	Refresh module, BSP traversal and handling.
//


#ifndef __R_BSP__
#define __R_BSP__

#include "r_defs.h"

#if defined(__cplusplus)
// BSP?
void R_ClearClipSegs( bspcontext_t* context, int32_t mincol, int32_t maxcol );
void R_ClearDrawSegs( bspcontext_t* context );

void R_RenderBSPNode( rendercontext_t& rendercontext, int32_t bspnum );
#endif // defined(__cplusplus)

#endif

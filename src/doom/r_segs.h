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
//	Refresh module, drawing LineSegs from BSP.
//


#ifndef __R_SEGS__
#define __R_SEGS__

#if defined(__cplusplus)

void R_StoreWallRange( rendercontext_t& rendercontext, wallcontext_t& wallcontext, int32_t start, int32_t stop );
void R_RenderMaskedSegRange( rendercontext_t& rendercontext, drawseg_t* ds, int x1, int x2 );

#endif // defined(__cplusplus)

#endif

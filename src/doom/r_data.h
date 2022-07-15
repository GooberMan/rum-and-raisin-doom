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
//  Refresh module, data I/O, caching, retrieval of graphics
//  by name.
//


#ifndef __R_DATA__
#define __R_DATA__

#include "r_defs.h"
#include "r_state.h"

DOOM_C_API void R_CacheComposite( int32_t tex );

// Retrieve column data for span blitting.
DOOM_C_API byte* R_GetColumn ( int32_t tex, int32_t col, int32_t colormapindex );

// A raw column is a non-composited column as you'd find in your WAD
DOOM_C_API byte* R_GetRawColumn ( int32_t tex, int32_t col );


// I/O, setting up the stuff.
DOOM_C_API void R_InitData (void);
DOOM_C_API void R_PrecacheLevel (void);


// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name. For animation?
DOOM_C_API int R_FlatNumForName(const char *name);


// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
DOOM_C_API int R_TextureNumForName(const char *name);
DOOM_C_API int R_CheckTextureNumForName(const char *name);

DOOM_C_API const char* R_TextureNameForNum( int32_t tex );

#endif

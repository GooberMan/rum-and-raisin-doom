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
//	Sky rendering.
//


#ifndef __R_SKY__
#define __R_SKY__

#include "r_defs.h"

#if defined(__cplusplus)
namespace constants
{
	static constexpr rend_fixed_t	skytexturemid = IntToRendFixed( 100 );
	static constexpr rend_fixed_t	skytexturemidoffset = IntToRendFixed( 28 );
}
#endif //defined(__cplusplus)

// SKY, store the number for name.
#define			SKYFLATNAME  "F_SKY1"

// The sky map is 256*128*4 maps.
#define ANGLETOSKYSHIFT		22

DOOM_C_API extern int				skyflatnum;

DOOM_C_API void R_InitSkyDefs();
DOOM_C_API void R_InitSkiesForLevel();
DOOM_C_API sky_t* R_GetSky( const char* name, doombool create );
DOOM_C_API void R_SetDefaultSky( const char* sky );
DOOM_C_API void R_ActivateSky( sky_t* sky );
DOOM_C_API void R_ActivateSkyAndAnims( int32_t texnum );
DOOM_C_API void R_UpdateSky();

#if defined(__cplusplus)
void R_DrawSky( rendercontext_t& rendercontext, rasterregion_t* thisregion, sky_t* basesky, sideinstance_t* skytexture );
#endif // defined(__cplusplus)

#endif

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
//   Setup a game, startup stuff.
//


#ifndef __P_SETUP__
#define __P_SETUP__

#ifdef __cplusplus
extern "C" {
#endif

	#include "w_wad.h"

	extern lumpinfo_t *maplumpinfo;

#ifdef __cplusplus
}
#endif


// NOT called by W_Ticker. Fixme.
DOOM_C_API void
P_SetupLevel
( int		episode,
  int		map,
  int		playermask,
  skill_t	skill);

// Called by startup code.
DOOM_C_API void P_Init (void);

#endif

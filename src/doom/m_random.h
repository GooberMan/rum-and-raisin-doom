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
//
//    


#ifndef __M_RANDOM__
#define __M_RANDOM__


#include "doomtype.h"


// Returns a number from 0 to 255,
// from a lookup table.
DOOM_C_API int M_Random (void);

// As M_Random, but used only by the play simulation.
DOOM_C_API int P_Random (void);

// Fix randoms for demos.
DOOM_C_API void M_ClearRandom (void);

// Defined version of P_Random() - P_Random()
DOOM_C_API int P_SubRandom (void);

#endif

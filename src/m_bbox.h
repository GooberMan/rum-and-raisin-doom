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
//    Nil.
//    


#ifndef __M_BBOX__
#define __M_BBOX__

#include <limits.h>

#include "m_fixed.h"

// Bounding box coordinate storage.
DOOM_C_API enum
{
    BOXTOP,
    BOXBOTTOM,
    BOXLEFT,
    BOXRIGHT
};	// bbox coordinates

// Bounding box functions.
DOOM_C_API void M_ClearBox(fixed_t* box);

DOOM_C_API void M_AddToBox( fixed_t* box, fixed_t x, fixed_t y );

#endif

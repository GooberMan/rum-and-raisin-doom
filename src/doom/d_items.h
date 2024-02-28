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
//	Items: key cards, artifacts, weapon, ammunition.
//


#ifndef __D_ITEMS__
#define __D_ITEMS__

#include "doomdef.h"

// Weapon info: sprite frames, ammunition use.
DOOM_C_API typedef struct
{
	ammotype_t		ammo;
	int32_t			upstate;
	int32_t			downstate;
	int32_t			readystate;
	int32_t			atkstate;
	int32_t			flashstate;

	// MBF extensions
	int32_t			ammopershot;
	int32_t			mbf21flags;
} weaponinfo_t;

DOOM_C_API extern  weaponinfo_t    weaponinfo[NUMWEAPONS];

#endif

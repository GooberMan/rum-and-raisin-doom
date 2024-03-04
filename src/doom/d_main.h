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
//	System specific interface stuff.
//


#ifndef __D_MAIN__
#define __D_MAIN__

#include "doomdef.h"




// Read events from all input devices

DOOM_C_API void D_ProcessEvents (void); 
	

//
// BASE LEVEL
//
DOOM_C_API void D_PageTicker (void);
DOOM_C_API void D_PageDrawer (void);
DOOM_C_API void D_AdvanceDemo (void);
DOOM_C_API void D_DoAdvanceDemo (void);
DOOM_C_API void D_StartTitle (void);

DOOM_C_API void D_SetupLoadingDisk( int32_t disk_icon_style );
//
// GLOBAL VARIABLES
//

DOOM_C_API extern  gameaction_t    gameaction;


#endif


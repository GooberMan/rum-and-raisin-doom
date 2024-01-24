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
//   Duh.
// 


#ifndef __G_GAME__
#define __G_GAME__

#include "doomdef.h"
#include "d_event.h"
#include "d_ticcmd.h"
#include "d_gameflow.h"


//
// GAME
//
DOOM_C_API void G_DeathMatchSpawnPlayer (int playernum);

DOOM_C_API void G_InitNew (skill_t skill, mapinfo_t* mapinfo, gameflags_t flags);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
DOOM_C_API void G_DeferedInitNew (skill_t skill, mapinfo_t* mapinfo, gameflags_t flags);

DOOM_C_API void G_DeferedPlayDemo (const char* demo);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
DOOM_C_API void G_LoadGame (char* name);

DOOM_C_API void G_DoLoadGame (void);

// Called by M_Responder.
DOOM_C_API void G_SaveGame (int slot, char* description);

// Only called by startup code.
DOOM_C_API void G_RecordDemo (const char* name);

DOOM_C_API void G_BeginRecording (void);

DOOM_C_API void G_PlayDemo (char* name);
DOOM_C_API void G_TimeDemo (char* name);
DOOM_C_API doombool G_CheckDemoStatus (void);

DOOM_C_API void G_ExitLevel (void);
DOOM_C_API void G_SecretExitLevel (void);

DOOM_C_API void G_WorldDone (void);

// Read current data from inputs and build a player movement command.

DOOM_C_API void G_BuildTiccmd (ticcmd_t *cmd, uint64_t maketic); 

DOOM_C_API void G_Ticker (void);
DOOM_C_API doombool G_Responder (event_t*	ev);

DOOM_C_API void G_ScreenShot (void);

DOOM_C_API void G_DrawMouseSpeedBox(void);
DOOM_C_API int G_VanillaVersionCode(void);

DOOM_C_API extern int vanilla_savegame_limit;
DOOM_C_API extern int vanilla_demo_limit;
#endif


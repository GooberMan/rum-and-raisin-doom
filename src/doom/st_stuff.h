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
//	Status bar code.
//	Does the face/direction indicator animatin.
//	Does palette indicators as well (red pain/berserk, bright pickup)
//

#ifndef __STSTUFF_H__
#define __STSTUFF_H__

#include "doomtype.h"
#include "d_event.h"
#include "m_cheat.h"
#include "v_video.h"

// Size of statusbar.
// Now sensitive for scaling.
#define ST_HEIGHT	32
#define ST_WIDTH	V_VIRTUALWIDTH
#define ST_Y		(V_VIRTUALHEIGHT - ST_HEIGHT)

#define ST_BUFFERWIDTH ( ( ( (int64_t)( ST_WIDTH << FRACBITS ) * (int64_t)V_WIDTHMULTIPLIER ) >> FRACBITS ) >> FRACBITS )
#define ST_BUFFERHEIGHT ( ( ( (int64_t)( ST_HEIGHT << FRACBITS ) * (int64_t)V_HEIGHTMULTIPLIER ) >> FRACBITS ) >> FRACBITS )

// States for status bar code.
typedef enum
{
    AutomapState,
    FirstPersonState
    
} st_stateenum_t;


// States for the chat code.
typedef enum
{
    StartChatState,
    WaitDestState,
    GetChatState
    
} st_chatstateenum_t;

typedef enum st_bordertile_e
{
	STB_WADDefined, // Defaults to view window border
	STB_Flat5_4,
	STB_Custom,
} st_bordertile_t;

//
// STATUS BAR
//

// Called by main loop.
doombool ST_Responder (event_t* ev);

// Called by main loop.
void ST_Ticker (void);

// Called by main loop.
void ST_Drawer (doombool fullscreen, doombool refresh);

// Called when the console player is spawned on each level.
void ST_Start (void);

// Called by startup code.
void ST_Init (void);

st_bordertile_t ST_GetBorderTileStyle();
void ST_SetBorderTileStyle( st_bordertile_t mode, const char* flatname );

extern cheatseq_t cheat_mus;
extern cheatseq_t cheat_god;
extern cheatseq_t cheat_ammo;
extern cheatseq_t cheat_ammonokey;
extern cheatseq_t cheat_noclip;
extern cheatseq_t cheat_commercial_noclip;
extern cheatseq_t cheat_powerup[7];
extern cheatseq_t cheat_choppers;
extern cheatseq_t cheat_clev;
extern cheatseq_t cheat_mypos;


#endif

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
//      System-specific timer interface
//


#ifndef __I_TIMER__
#define __I_TIMER__

#include "doomtype.h"

#define TICRATE 35

// Called by D_DoomLoop,
// returns current time in tics.
uint64_t I_GetTimeTicks (void);

// returns current time in ms
uint64_t I_GetTimeMS (void);

// returns current time in microseconds
uint64_t I_GetTimeUS (void);

// Pause for a specified number of ms
void I_Sleep(uint64_t ms);

// Initialize timer
void I_InitTimer(void);

// Wait for vertical retrace or pause a bit.
void I_WaitVBL(int count);

void I_InitPerfFrames( uint64_t count );

boolean I_IsPerfFramesRunning(void);

// If -perf is specified on command line, this will fill that buffer
void I_LogPerfFrame( uint64_t microseconds, const char* description );

#endif


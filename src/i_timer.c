//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020-2022 Ethan Watson
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
//      Timer functions.
//

#include <SDL2/SDL.h>

#include "i_timer.h"
#include "z_zone.h"
#include "doomtype.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined( _WIN32 )

#define TIMER_USE_SDL 0

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define GetPerfCounter( val ) QueryPerformanceCounter( (LARGE_INTEGER*)&val )
#define GetPerfFreq( val ) QueryPerformanceFrequency( (LARGE_INTEGER*)&val )
#define DoSleep( val ) Sleep( (Uint32)val )

#else // Non-Windows platforms

#define TIMER_USE_SDL 1

#define GetPerfCounter( val ) val = SDL_GetPerformanceCounter()
#define GetPerfFreq( val ) val = SDL_GetPerformanceFrequency()
#define DoSleep( val ) SDL_Delay( val )

#endif // Platform checks

//
// I_GetTime
// returns time in 1/35th second tics
//
static uint64_t basecounter = 0;
static uint64_t basefreq = 0;

// COUNTER_OFFSET is used for testing 64-bit timer support.
// Signed int32 values for millisecond counters means that
// wrap-around happens at tic 61,356,676. In real time, less
// than a day. A large value will ensure that all base ticks
// are already outside of that range, and thus you can catch
// any time errors immediately. Might need to tweak yourself
// according to what the performance counter returns.

//#define COUNTER_OFFSET 0x100000000000ull
#define COUNTER_OFFSET 0ull

uint64_t I_GetTimeTicks( void )
{
	uint64_t counter;
	GetPerfCounter( counter );

	return ( ( counter - basecounter ) * TICRATE ) / basefreq;
}

uint64_t I_GetTimeMS( void )
{
	uint64_t counter;
	GetPerfCounter( counter );

	return ( ( counter - basecounter ) * 1000ull ) / basefreq;
}

uint64_t I_GetTimeUS( void )
{
	uint64_t counter;
	GetPerfCounter( counter );

	return ( ( counter - basecounter ) * 1000000ull ) / basefreq;
}

// Sleep for a specified number of ms

void I_Sleep( uint64_t ms )
{
	DoSleep( ms );
}

void I_WaitVBL( uint64_t count )
{
	I_Sleep( ( count * 1000 ) / 70 );
}

void I_InitTimer( void )
{
	// initialize timer
	uint64_t counter;

#if TIMER_USE_SDL

#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
#endif
    SDL_Init(SDL_INIT_TIMER);

#endif // TIMER_USE_SDL

	GetPerfFreq( basefreq );
	GetPerfCounter( counter );
	basecounter = counter - COUNTER_OFFSET;
}

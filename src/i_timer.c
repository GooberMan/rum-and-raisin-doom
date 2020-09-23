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
//      Timer functions.
//

#include "SDL.h"

#include "i_timer.h"
#include "z_zone.h"
#include "doomtype.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct
{
	uint64_t		microseconds;
	const char*		description;
} perfframe_t;

static perfframe_t* perfframes = NULL;
static uint64_t numperfframes = 0;
static uint64_t currperfframe = 0;

//
// I_GetTime
// returns time in 1/35th second tics
//

static Uint32 basetime = 0;
static Uint64 basecounter = 0;
static Uint64 basefreq = 0;

int  I_GetTime (void)
{
    Uint32 ticks;

    ticks = SDL_GetTicks();

    if (basetime == 0)
        basetime = ticks;

    ticks -= basetime;

    return (ticks * TICRATE) / 1000;    
}

//
// Same as I_GetTime, but returns time in milliseconds
//

int I_GetTimeMS(void)
{
    Uint32 ticks;

    ticks = SDL_GetTicks();

    if (basetime == 0)
        basetime = ticks;

    return ticks - basetime;
}

uint64_t I_GetTimeUS(void)
{
	Uint64 counter;
	Uint64 ret;

	counter = SDL_GetPerformanceCounter();

	ret = ( ( counter - basecounter ) * 1000000 ) / basefreq;
	return ret;
}

// Sleep for a specified number of ms

void I_Sleep(int ms)
{
    SDL_Delay(ms);
}

void I_WaitVBL(int count)
{
    I_Sleep((count * 1000) / 70);
}


void I_InitTimer(void)
{
    // initialize timer

#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
#endif
    SDL_Init(SDL_INIT_TIMER);
	basefreq = SDL_GetPerformanceFrequency();
	basecounter = SDL_GetPerformanceCounter();
}

void I_InitPerfFrames( uint64_t count )
{
	if( count > 0 )
	{
		perfframes = (perfframe_t*)Z_Malloc( sizeof( perfframe_t ) * count, PU_STATIC, NULL );
		numperfframes = count;
		currperfframe = 0;
	}
}

void I_LogPerfFrame( uint64_t microseconds, const char* reason )
{
	uint64_t thisperf;
	FILE* of;

	if( numperfframes > 0 )
	{
		perfframes[ currperfframe ].microseconds = microseconds;
		perfframes[ currperfframe ].description = reason;

		if( ++currperfframe >= numperfframes )
		{
			time_t currtime = time( NULL );
			struct tm* local = localtime( &currtime );

			char filename[ 512 ];
			sprintf( filename, "%d%02d%02d_%02d.%02d.%02d_perfframe.csv", 1900 + local->tm_year, local->tm_mon + 1, local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec );

			of = fopen( filename, "w" );
			if( of != NULL )
			{
				fprintf( of, "frame,microseconds,description\n" );
				for( thisperf = 0; thisperf < numperfframes; ++thisperf )
				{
					fprintf( of, "%d,%d,%s\n", (int)thisperf, (int)perfframes[ thisperf ].microseconds, perfframes[ thisperf ].description );
				}
				fclose( of );
			}

			Z_Free( perfframes );
			perfframes = NULL;
			numperfframes = currperfframe = 0;

			exit(0);
		}
	}
}

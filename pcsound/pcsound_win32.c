//
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
//    PC speaker interface.
//

#ifdef _WIN32

#include "SDL.h"
#include "SDL_thread.h"
#include <windows.h>

#include "pcsound.h"
#include "pcsound_internal.h"

#pragma optimize( "", off )

static SDL_Thread *sound_thread_handle;
static int sound_thread_running;
static pcsound_callback_func callback;

static int SoundThread(void *unused)
{
    int frequency;
    int duration;

	uint64_t startticks;
	uint64_t nowms;
	uint64_t freq = SDL_GetPerformanceFrequency();

	SDL_SetThreadPriority( SDL_THREAD_PRIORITY_TIME_CRITICAL );
    
    while (sound_thread_running)
    {
        callback(&duration, &frequency);

        if (frequency != 0) 
        {
            Beep(frequency, duration);
        }
        else
        {
			startticks = SDL_GetPerformanceCounter();
			do
			{
				nowms = ( ( SDL_GetPerformanceCounter() - startticks ) * 1000ull ) / freq;
			} while( nowms <= duration );
        }
    }
    
    return 0;    
}

static int PCSound_Win32_Init(pcsound_callback_func callback_func)
{
    OSVERSIONINFO osvi;
    BOOL result;

    // Temporarily disabled - the Windows scheduler is strange and 
    // stupid.
   
    //return 0;

    // Find the OS version

    osvi.dwOSVersionInfoSize = sizeof(osvi);

    result = GetVersionEx(&osvi);

    if (!result)
    {
        return 0;
    }

    // Beep() ignores its arguments on win9x, so this driver will
    // not work there.
    if (osvi.dwPlatformId != VER_PLATFORM_WIN32_NT)
    {
        return 0;
    }

    // Start a thread to play sound.

    callback = callback_func;
    sound_thread_running = 1;

    sound_thread_handle =
        SDL_CreateThread(SoundThread, "PC speaker thread", NULL);

    return 1;
}

static void PCSound_Win32_Shutdown(void)
{
    sound_thread_running = 0;
    SDL_WaitThread(sound_thread_handle, NULL);
}

pcsound_driver_t pcsound_win32_driver = 
{
    "Windows",
    PCSound_Win32_Init,
    PCSound_Win32_Shutdown,
};

#endif /* #ifdef _WIN32 */


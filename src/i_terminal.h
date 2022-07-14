//
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
// DESCRIPTION: Simple terminal emulator, output only. Little more
//              than a wrapper in to the txt_io module, but I want
//              that render texture nice and self-contained here.
//              It also calls the standard printf functions.

#ifndef __I_TERMINAL__
#define __I_TERMINAL__

#include "doomtype.h"
#include "i_log.h"

#ifdef __cplusplus
extern "C" {
#endif

// Solely to get access to the text mode colours...
#include "txt_main.h"

typedef enum terminalmode_e
{
	TM_None,
	TM_StandardRender,
	TM_ImmediateRender,
} terminalmode_t;

#ifdef __cplusplus
}
#endif

DOOM_C_API void I_TerminalInit( void );
DOOM_C_API void I_TerminalSetMode( terminalmode_t mode );
DOOM_C_API void I_TerminalClear( void );
DOOM_C_API void I_TerminalPrintBanner( int32_t logtype, const char* banner, int32_t foreground, int32_t background );
DOOM_C_API void I_TerminalVPrintf( int32_t logtype, const char* format, va_list args );
DOOM_C_API void I_TerminalPrintf( int32_t logtype, const char* format, ... );
DOOM_C_API void I_TerminalRender( void );

#endif // __I_TERMINAL__

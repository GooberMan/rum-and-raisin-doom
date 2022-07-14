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

#include "i_terminal.h"

#include "i_log.h"
#include "m_container.h"
#include <cstdarg>

extern "C"
{
	#include "txt_io.h"

	#include "m_misc.h"

	#include "i_video.h"
}

static vbuffer_t terminalbuffer;
static terminalmode_t terminalmode = TM_None;

DOOM_C_API void I_TerminalInit( void )
{
	TXT_InitForBuffer( &terminalbuffer, I_GetWindow(), I_GetRenderer() );
	terminalmode = TM_StandardRender;
}

DOOM_C_API void I_TerminalSetMode( terminalmode_t mode )
{
	terminalmode = mode;
}

DOOM_C_API void I_TerminalClear( void )
{
	TXT_ClearScreen();

	if( terminalmode == TM_ImmediateRender )
	{
		I_TerminalRender();
	}
}

DOOM_C_API void I_TerminalPrintBanner( int32_t logtype, const char* banner, int32_t foreground, int32_t background )
{
	txt_saved_colors_t saved;
	TXT_SaveColors( &saved );

	TXT_FGColor( ( txt_color_t )foreground );
	TXT_BGColor( ( txt_color_t )background, 0 );

	size_t bannerlen = strlen( banner );
	size_t leftover = TXT_SCREEN_W - bannerlen;
	size_t startmessage = leftover / 2;
	size_t endmessage = startmessage + bannerlen;
	const char* currchar = banner;

	for( size_t index : iota( 0, TXT_SCREEN_W ) )
	{
		if( index < startmessage || index >= endmessage )
		{
			if( terminalmode != TM_None ) TXT_PutChar( ' ' );
			putchar( ' ' );
		}
		else
		{
			if( terminalmode != TM_None ) TXT_PutChar( *currchar );
			putchar( *currchar );
			++currchar;
		}
	}

	TXT_RestoreColors( &saved );

	I_LogAddEntry( logtype, banner );

	if( terminalmode == TM_ImmediateRender )
	{
		I_TerminalRender();
	}
}

DOOM_C_API void I_TerminalVPrintf( int32_t logtype, const char* format, va_list args )
{
	size_t len = M_vsnprintf( nullptr, 0, format, args );
	DoomString formatted( len, '\0' );
	M_vsnprintf( formatted.data(), len + 1, format, args );

	printf( formatted.c_str() );
	I_LogAddEntry( logtype, formatted.c_str() );


	if( terminalmode != TM_None )
	{
		for( char c : formatted )
		{
			TXT_PutChar( c );
		}

		if( terminalmode == TM_ImmediateRender )
		{
			I_TerminalRender();
		}
	}
}

DOOM_C_API void I_TerminalPrintf( int32_t logtype, const char* format, ... )
{
	va_list args;
	va_start( args, format );
	I_TerminalVPrintf( logtype, format, args );
	va_end( args );
}

DOOM_C_API void I_TerminalRender( void )
{
	TXT_UpdateScreenForBuffer( &terminalbuffer );
	if( terminalmode == TM_ImmediateRender )
	{
		I_StartFrame();
		//I_FinishUpdate( &terminalbuffer );
		TXT_UpdateScreen();
	}
}

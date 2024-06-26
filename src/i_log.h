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
// DESCRIPTION: Log library. Wraps some STL containers.
//


#ifndef __I_LOG__
#define __I_LOG__

#include "doomtype.h"
#include <stdarg.h>

DOOM_C_API typedef enum logtype_e
{
	Log_None,
	Log_Normal,
	Log_System,
	Log_Startup,
	Log_InGameMessage,
	Log_Chat,
	Log_Warning,
	Log_Error,

	Log_Max
} logtype_t;

DOOM_C_API void			I_LogDebug( const char* message );

DOOM_C_API void			I_LogAddEntry( int32_t type, const char* message );
DOOM_C_API void			I_LogAddEntryVAList( int32_t type, const char* message, va_list args );
DOOM_C_API void			I_LogAddEntryVar( int32_t type, const char* message, ... );

DOOM_C_API size_t			I_LogNumEntries();
DOOM_C_API const char*		I_LogGetEntryText( size_t index );
DOOM_C_API int32_t			I_LogGetEntryType( size_t index );
DOOM_C_API const char*		I_LogGetTimestamp( size_t index );

#endif // __I_LOG__


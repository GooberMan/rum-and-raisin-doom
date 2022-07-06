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

#ifdef __cplusplus
extern "C" {
#endif

#include "doomtype.h"

enum LogType
{
	Log_Normal,
	Log_System,
	Log_Startup,
	Log_InGameMessage,
	Log_Chat,
	Log_Warning,
	Log_Error,

	Log_Max
};

void			I_LogAddEntry( int32_t type, const char* message );
void			I_LogAddEntryVAList( int32_t type, const char* message, va_list args );
void			I_LogAddEntryVar( int32_t type, const char* message, ... );

size_t			I_LogNumEntries();
const char*		I_LogGetEntryText( size_t index );
int32_t			I_LogGetEntryType( size_t index );
const char*		I_LogGetTimestamp( size_t index );

#ifdef __cplusplus
}
#endif

#endif // __I_LOG__


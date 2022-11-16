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

#include "i_log.h"

#include "z_zone.h"

#include "m_misc.h"

#include <memory>
#include <limits>
#include <utility>
#include <type_traits>
#include <algorithm>

#include <ctime>

#include "m_container.h"

template< typename _ty > using LogVector = std::vector< _ty >;
using LogString = std::string;

typedef struct logentry_s
{
	logentry_s()
		: timestamp( 0 )
		, type( 0 ) { }

	logentry_s( int32_t t, const char* m )
		: timestamp( std::time( nullptr ) )
		, type( t )
		, message( m )
	{
		message.erase( std::remove( message.begin(), message.end(), '\n' ), message.end() );
		std::replace( message.begin(), message.end(), '\t', ' ' );

		char output[ 24 ];

		std::tm local = *std::localtime( &timestamp );
		std::strftime( output, 24, "%H:%M:%S", &local );
		timestring = output;
	}

	std::time_t				timestamp;
	int32_t					type;
	LogString				message;
	LogString				timestring;
} logentry_t;

static LogVector< logentry_t >	logentries;

#define BUFFER_LENGTH 1024

#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

INLINE void PrintEntryToConsole( logentry_t& entry )
{
	OutputDebugString( entry.message.c_str() );
	OutputDebugString( "\n" );
}
#else // Non-windows platforms dump to console
#include <cstdio>

INLINE void PrintEntryToConsole( logentry_t& entry )
{
	printf( "%s\n", entry.message.c_str() );
}
#endif // _WIN32

DOOM_C_API void I_LogDebug( const char* message )
{
#if defined( _WIN32 )
	static const char* lastmessage = nullptr;
	if( lastmessage == message )
	{
		OutputDebugString( "DUPLICATE: " );
	}
	lastmessage = message;

	OutputDebugString( message );
	OutputDebugString( "\n" );
#else // Non-windows platforms dump to console
	printf( "%s\n", message );
#endif // _WIN32
}

DOOM_C_API void I_LogAddEntry( int32_t type, const char* message )
{
	if( type == Log_None )
	{
		return;
	}

	if( logentries.capacity() == logentries.size() )
	{
		logentries.reserve( logentries.capacity() + 1024 );
	}

	auto entry = logentries.insert( logentries.end(), logentry_t( type, message ) );
	PrintEntryToConsole( *entry );
}

DOOM_C_API void I_LogAddEntryVAList( int32_t type, const char* message, va_list args )
{
	char buffer[ 1024 ];
	buffer[ 0 ] = 0;
	M_vsnprintf( buffer, BUFFER_LENGTH, message, args );
	I_LogAddEntry( type, buffer );
}

DOOM_C_API void I_LogAddEntryVar( int32_t type, const char* message, ... )
{
	va_list args;
	va_start( args, message );
	I_LogAddEntryVAList( type, message, args );
	va_end( args );
}

DOOM_C_API size_t I_LogNumEntries()
{
	return logentries.size();
}

DOOM_C_API const char* I_LogGetEntryText( size_t index )
{
	return logentries[ index ].message.c_str();
}

DOOM_C_API int32_t I_LogGetEntryType( size_t index )
{
	return logentries[ index ].type;
}

DOOM_C_API const char* I_LogGetTimestamp( size_t index )
{
	return logentries[ index ].timestring.c_str();
}

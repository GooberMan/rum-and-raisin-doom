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

extern "C"
{
	#include "z_zone.h"
	#include "m_misc.h"
}

#include <memory>
#include <limits>
#include <utility>
#include <type_traits>

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

extern "C" {

#define BUFFER_LENGTH 1024

void I_LogAddEntry( int32_t type, const char* message )
{
	if( logentries.capacity() == logentries.size() )
	{
		logentries.reserve( logentries.capacity() + 1024 );
	}

	logentries.push_back( logentry_t( type, message ) );
}

void I_LogAddEntryVAList( int32_t type, const char* message, va_list args )
{
	char buffer[ 1024 ];
	buffer[ 0 ] = 0;
	M_vsnprintf( buffer, BUFFER_LENGTH, message, args );
	I_LogAddEntry( type, buffer );
}

void I_LogAddEntryVar( int32_t type, const char* message, ... )
{
	va_list args;
	va_start( args, message );
	I_LogAddEntryVAList( type, message, args );
	va_end( args );
}

size_t I_LogNumEntries()
{
	return logentries.size();
}

const char* I_LogGetEntryText( size_t index )
{
	return logentries[ index ].message.c_str();
}

int32_t I_LogGetEntryType( size_t index )
{
	return logentries[ index ].type;
}

const char* I_LogGetTimestamp( size_t index )
{
	return logentries[ index ].timestring.c_str();
}

}
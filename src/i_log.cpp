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

#include <vector>

template< typename _ty, int32_t _tag = PU_STATIC >
struct DoomAllocator
{
	enum : int32_t										{ tag = _tag };
	typedef typename std::remove_cv< _ty >::type		value_type;
	typedef value_type*									pointer;
	typedef const pointer								const_pointer;
	typedef value_type&									reference;
	typedef const reference								const_reference;
	typedef size_t										size_type;
	typedef ptrdiff_t									difference_type;
	typedef std::true_type								propagate_on_container_move_assignment;
	template< typename _ty >
	struct rebind
	{
		typedef DoomAllocator< _ty, tag >				other;
	};
	typedef std::true_type								is_always_equal;

	DoomAllocator() = default;
	~DoomAllocator() = default;

	pointer address( reference x ) noexcept
	{
		return &x;
	}

	const_pointer address( const_reference x ) const noexcept
	{
		return &x;
	}

	pointer allocate( size_type n, const void * hint = 0 )
	{
		return ( pointer )Z_Malloc( sizeof( value_type ) * n, tag, NULL );
	}

	void deallocate( pointer p, std::size_t n )
	{
		Z_Free( p );
	}

	size_type max_size() const noexcept
	{
		return std::numeric_limits<size_type>::max() / sizeof(value_type);
	}

	template< class U, class... Args >
	void construct( U* p, Args&&... args )
	{
		::new( (void *)p ) U( std::forward< Args >( args )... );
	}

	template< class U >
	void destroy( U* p )
	{
		p->~U();
	}

	template< class _other >
	bool operator==( const DoomAllocator< _other >& rhs ) noexcept
	{
		return std::is_same< value_type, _other >::value;
	}

	template< class _other >
	bool operator!=( const DoomAllocator< _other >& rhs ) noexcept
	{
		return !std::is_same< value_type, _other >::value;
	}
};

// Can't use our zone allocator just yet, because we try to log messages before Z_Init
//template< typename _ty, int32_t _tag = PU_STATIC > using DoomVector = std::vector< _ty, DoomAllocator< _ty, _tag > >;
//using DoomString = std::basic_string< char, std::char_traits< char >, DoomAllocator< char, PU_STATIC > >;

template< typename _ty > using DoomVector = std::vector< _ty >;
using DoomString = std::string;

typedef struct logentry_s
{
	uint64_t		timestamp;
	int32_t			type;
	DoomString		message;
} logentry_t;

static DoomVector< logentry_t >	logentries;

#pragma optimize( "", off )

extern "C" {

#define BUFFER_LENGTH 1024

void I_LogAddEntry( int32_t type, const char* message )
{
	if( logentries.capacity() == logentries.size() )
	{
		logentries.reserve( logentries.capacity() + 1024 );
	}

	logentries.push_back( { 0, type, message } );
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

uint64_t I_LogGetTimestamp( size_t index )
{
	return logentries[ index ].timestamp;
}

}
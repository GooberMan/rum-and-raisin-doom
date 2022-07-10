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
// DESCRIPTION: Container library. Wraps some STL containers.
//


#ifndef __M_CONTAINER__
#define __M_CONTAINER__

#ifdef __cplusplus

extern "C"
{
	#include "doomtype.h"
	#include "z_zone.h"
}

#include <span>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>

template< typename _ty, int32_t _tag = PU_STATIC >
struct DoomAllocator
{
	enum : int32_t										{ tag = _tag };
	typedef typename std::remove_cv< _ty >::type		value_type;
	typedef value_type*									pointer;
	typedef const pointer								const_pointer;
	typedef value_type&									reference;
	typedef const value_type&							const_reference;
	typedef size_t										size_type;
	typedef ptrdiff_t									difference_type;
	typedef std::true_type								propagate_on_container_move_assignment;
	template< typename _newty >
	struct rebind
	{
		typedef DoomAllocator< _newty, tag >			other;
	};
	typedef std::true_type								is_always_equal;

	constexpr DoomAllocator() noexcept { }
	constexpr DoomAllocator( const DoomAllocator& other ) noexcept { }
	template< typename U, int32_t T = PU_STATIC >
	constexpr DoomAllocator( const DoomAllocator< U, T >& other ) noexcept { }


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
		return ( pointer )Z_Malloc( (int32_t)( sizeof( value_type ) * n ), tag, NULL );
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

template< typename _ty, int32_t _tag = PU_STATIC >
using DoomVector = std::vector< _ty >; //, DoomAllocator< _ty, _tag > >;

template< typename _key, typename _val, int32_t _tag = PU_STATIC >
using DoomUnorderedMap = std::unordered_map< _key, _val, std::hash< _key >, std::equal_to< _key > >; //, DoomAllocator< std::pair< const _key, _val >, _tag > >;

template< typename _key, typename _val, int32_t _tag = PU_STATIC >
using DoomMap = std::map< _key, _val, std::less< _key > >; //, DoomAllocator< std::pair< const _key, _val >, _tag > >;

using DoomString = std::basic_string< char, std::char_traits< char > >; //, DoomAllocator< char, PU_STATIC > >;

// Reformatted copypasta from https://ctrpeach.io/posts/cpp20-string-literal-template-parameters/
template< size_t _length >
struct StringLiteral
{
	constexpr StringLiteral( const char( &str )[ _length ] )
	{
		std::copy_n( str, _length, value );
	}

	constexpr const char* c_str() const { return value; }
	constexpr size_t size() const		{ return _length; }

	char value[ _length ];
};

#endif // __cplusplus

#endif // __M_CONTAINER__
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
// DESCRIPTION: Templated conversion using to< type >() functions.
//              As these are all templated, custom types with
//              conversions should define their own overloads.
//

#if !defined( __M_CONV_H__ )
#define __M_CONV_H__

#include "doomtype.h"
#include <type_traits>
#include <string>
#include <cstdlib>

template< typename _ty >
struct is_std_string : std::false_type { };

template< typename _val, typename _trait, typename _alloc >
struct is_std_string< typename std::basic_string< _val, _trait, _alloc > > : std::true_type { };

template< typename _ty >
constexpr bool is_std_string_v = is_std_string< _ty >::value;

template< typename _ty >
requires std::is_integral_v< _ty >
INLINE _ty stringto( const char* start, size_t* index = nullptr, int base = 10 )
{
	char* end;
	_ty output;
	if constexpr( std::is_unsigned_v< _ty > )
	{
		output = (_ty)strtoull( start, &end, base );
	}
	else
	{
		output = (_ty)strtoll( start, &end, base );
	}

	if( index != nullptr )
	{
		*index = (size_t)( end - start );
	}

	return output;
}

// Basic types from string
template< typename _ty, typename _str >
requires std::is_same_v< _ty, const char* > && std::is_integral_v< _ty >
INLINE auto to( _str val )
{
	return stringto< _ty >( val );
}

template< typename _ty, typename _str >
requires is_std_string_v< _str > && std::is_integral_v< _ty >
INLINE auto to( const _str& val )
{
	return stringto< _ty >( val.c_str() );
}

template< typename _ty, size_t _len >
requires std::is_integral_v< _ty >
INLINE auto to( const char( &val )[ _len ] )
{
	return stringto< _ty >( (const char*)val );
}


template< typename _ty, typename _str >
requires is_std_string_v< _str > && std::is_floating_point_v< _ty >
INLINE auto to( const _str& val )
{
	if constexpr( std::is_same_v< float_t, _ty > )
	{
		return (_ty)std::stof( val );
	}
	else if constexpr( std::is_same_v< double_t, _ty > )
	{
		return (_ty)std::stof( val );
	}
	else
	{
		return _ty();
	}
}

template< typename _ty, typename _str >
requires std::is_same_v< _ty, const char* > && std::is_floating_point_v< _ty >
INLINE auto to( const _str& val )
{
	if constexpr( std::is_same_v< float_t, _ty > )
	{
		return (_ty)std::stof( val );
	}
	else if constexpr( std::is_same_v< double_t, _ty > )
	{
		return (_ty)std::stof( val );
	}
	else
	{
		return _ty();
	}
}

template< typename _ty, size_t _len >
requires std::is_floating_point_v< _ty >
INLINE auto to( const char( &val )[ _len ] )
{
	if constexpr( std::is_same_v< float_t, _ty > )
	{
		return (_ty)std::stof( val );
	}
	else if constexpr( std::is_same_v< double_t, _ty > )
	{
		return (_ty)std::stof( val );
	}
	else
	{
		return _ty();
	}
}

// Basic types to string
template< typename _str, typename _ty >
requires is_std_string_v< _str > && ( std::is_integral_v< _ty > || std::is_floating_point_v< _ty > )
INLINE auto to( const _ty& val )
{
	return std::to_string( val );
}

#endif // __M_CONV_H__
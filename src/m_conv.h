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
#include <algorithm>
#include <cstdlib>

template< typename _ty >
struct is_std_string : std::false_type { };

template< typename _val, typename _trait, typename _alloc >
struct is_std_string< typename std::basic_string< _val, _trait, _alloc > > : std::true_type { };

template< typename _ty >
constexpr bool is_std_string_v = is_std_string< _ty >::value;

template< typename _ty >
requires std::is_integral_v< _ty >
INLINE _ty stringto( const char* start )
{
	char* end;
	_ty output;
	if constexpr( std::is_unsigned_v< _ty > )
	{
		output = (_ty)strtoull( start, &end, 10 );
	}
	else
	{
		output = (_ty)strtoll( start, &end, 10 );
	}

	return output;
}

template<>
INLINE bool stringto< bool >( const char* start )
{
	if( strcasecmp( start, "true" ) == 0 )
	{
		return true;
	}
	else if( strcasecmp( start, "false" ) == 0 )
	{
		return false;
	}

	return !!stringto< int64_t >( start );
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

template< typename _str >
requires is_std_string_v< _str > 
INLINE _str ToLower( const _str& str )
{
	_str copy = str;
	std::transform( copy.begin(), copy.end(), copy.begin(), []( auto c ){ return std::tolower( c ); } );
	return copy;
}

template< typename _str >
requires is_std_string_v< _str > 
INLINE _str& ToLower( _str& str )
{
	std::transform( str.begin(), str.end(), str.begin(), []( auto c ){ return std::tolower( c ); } );
	return str;
}

template< typename _str >
requires is_std_string_v< _str > 
INLINE _str&& ToLower( _str&& str )
{
	std::transform( str.begin(), str.end(), str.begin(), []( auto c ){ return std::tolower( c ); } );
	return std::move( str );
}

template< typename _output = std::string >
INLINE auto ToLower( const char* str )
{
	return ToLower( _output( str ) );
}

template< typename _str >
requires is_std_string_v< _str > 
INLINE _str ToUpper( const _str& str )
{
	_str copy = str;
	std::transform( copy.begin(), copy.end(), copy.begin(), []( auto c ){ return std::toupper( c ); } );
	return copy;
}

template< typename _str >
requires is_std_string_v< _str > 
INLINE _str& ToUpper( _str& str )
{
	std::transform( str.begin(), str.end(), str.begin(), []( auto c ){ return std::toupper( c ); } );
	return str;
}

template< typename _str >
requires is_std_string_v< _str > 
INLINE _str&& ToUpper( _str&& str )
{
	std::transform( str.begin(), str.end(), str.begin(), []( auto c ){ return std::toupper( c ); } );
	return std::move( str );
}

template< typename _output = std::string >
INLINE auto ToUpper( const char* str )
{
	return ToUpper( _output( str ) );
}

constexpr bool IsDigit( const char val )
{
	return val >= '0' && val <= '9';
}

constexpr bool IsNumber( const char* val )
{
	return IsDigit( val[ 0 ] )
		|| ( val[ 0 ] == '-' && IsDigit( val[ 1 ] ) );
}

template< typename _ty >
requires std::is_integral_v< _ty >
constexpr auto Negate( _ty val )
{
	return ~val + 1;
}

#endif // __M_CONV_H__
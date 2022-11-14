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

// Not currently a complete JSON implementation, doesn't fully implement arrays

#if !defined( __M_JSON_H__ )
#define __M_JSON_H__

#if defined( __cplusplus )

#include "doomtype.h"
#include "m_container.h"
#include "m_conv.h"

#include <sstream>
#include <ranges>

enum class JSONElementType : int32_t
{
	Invalid,
	Null,
	String,
	Number,
	Element,
	ElementArray,
};

// Necessary forward declarations
//class JSONElement;
//template< typename _ty > auto to( const JSONElement& source );
//template< typename _ty > auto& to( const JSONElement& source );

class JSONElement
{
public:
	static const JSONElement empty;

	INLINE JSONElement()
		: type( JSONElementType::Invalid )
	{
	}

	static INLINE JSONElement MakeRoot()
	{
		return JSONElement( JSONElementType::Element );
	}

	static JSONElement Deserialise( const std::string& jsondoc );

	std::string Serialise( size_t depth = 0 );

	const JSONElement& AddNull( const std::string& key )
	{
		if( !( type == JSONElementType::Element || type == JSONElementType::ElementArray ) )
		{
			return empty;
		}

		JSONElement& output = *children.insert( children.end(), JSONElement( JSONElementType::Null ) );
		if( type == JSONElementType::Element )
		{
			output.key = key;
			element_indices[ key ] = children.size() - 1;
		}

		return output;
	}

	const JSONElement& AddString( const std::string& key, const std::string& val )
	{
		if( !( type == JSONElementType::Element || type == JSONElementType::ElementArray ) )
		{
			return empty;
		}

		JSONElement& output = *children.insert( children.end(), JSONElement( JSONElementType::String ) );
		if( type == JSONElementType::Element )
		{
			output.key = key;
			output.value = val;
			element_indices[ key ] = children.size() - 1;
		}

		return output;
	}

	template< typename _ty >
	requires std::is_integral_v< _ty > || std::is_floating_point_v< _ty >
	const JSONElement& AddNumber( const std::string& key, const _ty val )
	{
		if( !( type == JSONElementType::Element || type == JSONElementType::ElementArray ) )
		{
			return empty;
		}

		JSONElement& output = *children.insert( children.end(), JSONElement( JSONElementType::Number ) );
		if( type == JSONElementType::Element )
		{
			output.key = key;
			output.value = ::to< std::string >( val );
			element_indices[ key ] = children.size() - 1;
		}

		return output;
	}

	JSONElement& AddElement( const std::string& key )
	{
		if( !( type == JSONElementType::Element || type == JSONElementType::ElementArray ) )
		{
			// THIS IS UGLY, FIX THIS
			return (JSONElement&)empty;
		}

		JSONElement& output = *children.insert( children.end(), JSONElement( JSONElementType::Element ) );
		if( type == JSONElementType::Element )
		{
			output.key = key;
			element_indices[ key ] = children.size() - 1;
		}

		return output;
	}

	JSONElement& AddArray( const std::string& key )
	{
		if( !( type == JSONElementType::Element || type == JSONElementType::ElementArray ) )
		{
			// THIS IS UGLY, FIX THIS
			return (JSONElement&)empty;
		}

		JSONElement& output = *children.insert( children.end(), JSONElement( JSONElementType::ElementArray ) );
		if( type == JSONElementType::Element )
		{
			output.key = key;
			element_indices[ key ] = children.size() - 1;
		}
		return output;
	}

	const JSONElement& operator[]( const std::string& key ) const
	{
		if( type == JSONElementType::Element && !children.empty() )
		{
			auto found = element_indices.find( key );
			if( found != element_indices.end() )
			{
				return children[ found->second ];
			}
		}

		return empty;
	}

	const JSONElement& operator[]( const size_t index ) const
	{
		if( type == JSONElementType::ElementArray && !children.empty() )
		{
			return children[ index ];
		}

		return empty;
	}

	auto Children() const
	{
		return std::ranges::views::take( children, type == JSONElementType::ElementArray ? children.size() : 0 );
	}

	auto& Key() const		{ return key; }
	auto& Value() const		{ return value; }

	template< typename _ty >
	requires std::is_integral_v< _ty > || std::is_floating_point_v< _ty >
	friend _ty to( const JSONElement& source );

	template< typename _ty >
	requires is_std_string_v< _ty >
	friend std::string to( const JSONElement& source );

private:
	JSONElement( JSONElementType t )
		: type( t )
	{
	}

	std::string DeserialiseElement( std::istringstream& jsonstream );

	const JSONElement& AddNumber( const std::string& key, const std::string& val )
	{
		if( !( type == JSONElementType::Element || type == JSONElementType::ElementArray ) )
		{
			return empty;
		}

		JSONElement& output = *children.insert( children.end(), JSONElement( JSONElementType::Number ) );
		if( type == JSONElementType::Element )
		{
			output.key = key;
			output.value = val;
			element_indices[ key ] = children.size() - 1;
		}

		return output;
	}


	std::string									key;
	std::string									value;
	std::vector< JSONElement >					children;
	std::unordered_map< std::string, size_t >	element_indices;
	JSONElementType								type;
};

template< typename _ty >
requires std::is_integral_v< _ty > || std::is_floating_point_v< _ty >
INLINE _ty to( const JSONElement& source )
{
	if( source.type == JSONElementType::Number )
	{
		return to< _ty >( source.value );
	}
	else
	{
		return _ty();
	}
}

template< typename _ty >
requires is_std_string_v< _ty >
INLINE std::string to( const JSONElement& source )
{
	std::string output;
	if( source.type == JSONElementType::Number || source.type == JSONElementType::String )
	{
		output = source.value;
		size_t foundpos = 0;
		while( ( foundpos = output.find( "\\n", foundpos ) ) != std::string::npos )
		{
			output.replace( foundpos, 2, "\n" );
			foundpos += 1;
		}
	}
	return output;
}

#endif //defined( __cplusplus )

#endif //!defined( __M_JSON_H__ )

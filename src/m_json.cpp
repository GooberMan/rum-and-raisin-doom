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

#include "m_json.h"

#include "i_system.h"

#include <iomanip>
#include <regex>

const JSONElement JSONElement::empty;

constexpr const char* AnyNumberRegexString = "^[0-9+\\-\\.eE]+$";
static std::regex AnyNumberRegex = std::regex( AnyNumberRegexString );

static INLINE bool IsAnyNumber( const std::string& val )
{
	return std::regex_match( val, AnyNumberRegex );
}

std::string JSONElement::DeserialiseElement( std::istringstream& jsonstream )
{
	std::string token;
	std::string key;
	std::string value;

	do
	{
		jsonstream >> std::quoted( key );
		if( key == "}" )
		{
			token = key;
			break;
		}
		jsonstream >> std::quoted( token );
		jsonstream >> std::quoted( value );
		if( value.ends_with( ',' ) )
		{
			value = value.substr( 0, value.size() - 1 );
			token = ",";
		}
		else
		{
			if( value != "{" )
			{
				jsonstream >> std::quoted( token );
			}
		}

		if( value == "{" )
		{
			JSONElement& newelem = AddElement( key );
			token = newelem.DeserialiseElement( jsonstream );
			if( token.ends_with( ',' ) )
			{
				token = ",";
			}
		}
		else if( value == "[" )
		{
			I_Error( "Don't do this yet" );
		}
		else if( IsAnyNumber( value ) )
		{
			AddNumber( key, value );
		}
		else
		{
			AddString( key, value );
		}

	} while( token == "," );

	return token;
}

JSONElement JSONElement::Deserialise( const std::string& jsondoc )
{
	std::istringstream jsonstream( jsondoc );

	std::string token;
	jsonstream >> std::quoted( token );
	if( token == "{" )
	{
		JSONElement root( JSONElementType::Element );
		root.DeserialiseElement( jsonstream );
		return root;
	}

	return JSONElement::empty;
}

std::string JSONElement::Serialise( size_t depth )
{
	std::string output;
	output.reserve( 1024 );

	std::string oldtabs( depth, '\t' );
	std::string newtabs( depth + 1, '\t' );

	bool didprev = false;

	switch( type )
	{
	case JSONElementType::String:
		output += "\"" + key + "\" : \"" + value + "\"";
		break;
	case JSONElementType::Number:
		output += "\"" + key + "\" : " + value;
		break;
	case JSONElementType::Element:
		if( depth > 0 ) output += "\"" + key + "\" : ";
		output += "{\n";
		for( auto& child : children )
		{
			if( didprev ) output += ",\n";
			didprev = true;
			output += newtabs + child.Serialise( depth + 1 );
		}
		output += "\n" + oldtabs + "}";
		break;
	default:
		break;
	}

	return output;
}

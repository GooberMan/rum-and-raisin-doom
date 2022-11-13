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

static void Extract( std::istringstream& input, std::string& output )
{
	output.clear();
	bool inquote = false;
	bool inescape = false;
	bool stillvalid = !input.eof();
	while( stillvalid )
	{
		char current = input.get();

		bool add = true;
		if( inquote && !inescape )
		{
			inquote = current != '\"';
			inescape = current == '\\';
		}
		else if( !inquote )
		{
			inquote = current == '\"';
		}
		else if( !inescape )
		{
			inescape = current == '\\';
		}
		else
		{
			inescape = false;
		}

		if( inquote )
		{
			add = inescape || current != '\"';
		}
		else
		{
			stillvalid = !( std::isspace( current ) || current == '{' || current == '}' || current == '[' || current == ']' || current == ':' || current == ',' );
			add = !std::isspace( current ) && current != '\"';
		}

		if( add ) output += current;

		if( input.eof() )
		{
			stillvalid = false;
		}
		else if( !inquote && ( input.peek() == ':' || input.peek() == ',' || input.peek() == '{' || input.peek() == '}' || input.peek() == '[' || input.peek() == ']' ) )
		{
			stillvalid = false;
		}
	}
}

static void StripWhitespace( std::string& val )
{
	std::string::iterator curr = val.begin();
	bool inquote = false;
	bool inescape = false;
	while( curr != val.end() )
	{
		if( inquote && !inescape )
		{
			inquote = *curr != '\"';
			inescape = *curr == '\\';
		}
		else if( !inquote )
		{
			inquote = *curr == '\"';
		}
		else if( !inescape )
		{
			inescape = *curr == '\\';
		}
		else
		{
			inescape = false;
		}

		if( !inquote && std::isspace( (unsigned char)*curr ) )
		{
			curr = val.erase( curr );
		}
		else
		{
			++curr;
		}
	}
}

std::string JSONElement::DeserialiseElement( std::istringstream& jsonstream )
{
	std::string token;
	std::string key;
	std::string value;

	do
	{
		Extract( jsonstream, key );
		if( type == JSONElementType::ElementArray )
		{
			if( key == "{" )
			{
				JSONElement& newelem = AddElement( "" );
				newelem.DeserialiseElement( jsonstream );
				Extract( jsonstream, token );
				continue;
			}
			else if( key == "]" )
			{
				Extract( jsonstream, token );
				continue;
			}
		}
		else if( key == "}"  )
		{
			Extract( jsonstream, token );
			continue;
		}
		Extract( jsonstream, token );
		Extract( jsonstream, value );

		if( value == "{" )
		{
			JSONElement& newelem = AddElement( key );
			newelem.DeserialiseElement( jsonstream );
			Extract( jsonstream, token );
		}
		else if( value == "[" )
		{
			JSONElement& newelem = AddArray( key );
			newelem.DeserialiseElement( jsonstream );
			Extract( jsonstream, token );
		}
		else if( value == "null" )
		{
			AddNull( key );
		}
		else if( IsAnyNumber( value ) )
		{
			AddNumber( key, value );
		}
		else
		{
			AddString( key, value );
		}

		Extract( jsonstream, token );
	} while( token == "," );

	return token;
}

JSONElement JSONElement::Deserialise( const std::string& jsondoc )
{
	std::string stripped = jsondoc;
	StripWhitespace( stripped );
	std::istringstream jsonstream( stripped );

	std::string token;
	Extract( jsonstream, token );
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

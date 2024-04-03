//
// Copyright(C) 2020-2024 Ethan Watson
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

#include "m_jsonlump.h"

#include <regex>

constexpr const char* TypeMatchRegexString = "^[a-z0-9_-]+$";
static std::regex TypeMatchRegex = std::regex( TypeMatchRegexString );

constexpr const char* VersionMatchRegexString = "^(\\d+)\\.(\\d+)\\.(\\d+)$";
static std::regex VersionMatchRegex = std::regex( VersionMatchRegexString );

bool M_ParseJSONLump( lumpindex_t lumpindex, const char* lumptype, JSONLumpFunc&& parsefunc )
{
	if( lumpindex < 0
		|| W_LumpLength( lumpindex ) <= 0 )
	{
		return false;
	}

	const char* jsondata = (const char*)W_CacheLumpNum( lumpindex, PU_STATIC );

	JSONElement root = JSONElement::Deserialise( jsondata );

	const JSONElement& type			= root[ "type" ];
	const JSONElement& version		= root[ "version" ];
	const JSONElement& metadata		= root[ "metadata" ];
	const JSONElement& data			= root[ "data" ];

	std::smatch versionmatch;
	std::string versionstr = to< std::string >( version );

	if( root.Children().size() != 4
		|| !type.Valid()
		|| !version.Valid()
		|| !metadata.Valid()
		|| !data.Valid()
		|| !std::regex_match( lumptype, TypeMatchRegex )
		|| to< std::string >( type ) != lumptype
		|| !std::regex_search( versionstr, versionmatch, VersionMatchRegex )
		)
	{
		return false;
	}

	JSONLumpVersion versiondata =
	{
		to< int32_t >( versionmatch[ 0 ].str() ),
		to< int32_t >( versionmatch[ 1 ].str() ),
		to< int32_t >( versionmatch[ 2 ].str() ),
	};

	return parsefunc( data, versiondata );
}

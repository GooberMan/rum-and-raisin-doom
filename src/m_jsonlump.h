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

#if !defined( __M_JSONLUMP_H__ )
#define __M_JSONLUMP_H__

#if defined( __cplusplus )

#include "doomtype.h"
#include "m_json.h"
#include "w_wad.h"

#include <functional>

struct JSONLumpVersion
{
	int32_t major;
	int32_t minor;
	int32_t revision;
	int32_t devversion;

	constexpr bool operator > ( const JSONLumpVersion& rhs ) const
	{
		return major > rhs.major
			|| minor > rhs.minor
			|| revision > rhs.revision
			|| devversion > rhs.devversion;
	}

	constexpr bool operator >= ( const JSONLumpVersion& rhs ) const
	{
		return major >= rhs.major
			|| minor >= rhs.minor
			|| revision >= rhs.revision
			|| devversion >= rhs.devversion;
	}

	constexpr bool operator < ( const JSONLumpVersion& rhs ) const
	{
		return major < rhs.major
			|| minor < rhs.minor
			|| revision < rhs.revision
			|| devversion < rhs.devversion;
	}

	constexpr bool operator <= ( const JSONLumpVersion& rhs ) const
	{
		return major <= rhs.major
			|| minor <= rhs.minor
			|| revision <= rhs.revision
			|| devversion <= rhs.devversion;
	}
};

typedef enum jsonlumpresult_e
{
	jl_success,
	jl_notfound,
	jl_malformedroot,
	jl_typemismatch,
	jl_badversionformatting,
	jl_versionmismatch,
	jl_parseerror,
} jsonlumpresult_t;

using JSONLumpFunc = std::function< jsonlumpresult_t( const JSONElement& elem, const JSONLumpVersion& version ) >;

jsonlumpresult_t M_ParseJSONLump( lumpindex_t lumpindex, const char* lumptype, const JSONLumpVersion& maxversion, const JSONLumpFunc& parsefunc );

INLINE jsonlumpresult_t M_ParseJSONLump( lumpindex_t lumpindex, const char* lumptype, const JSONLumpVersion& maxversion, JSONLumpFunc&& parsefunc )
{
	return M_ParseJSONLump( lumpindex, lumptype, maxversion, parsefunc );
}

INLINE jsonlumpresult_t M_ParseJSONLump( const char* lumpname, const char* lumptype, const JSONLumpVersion& maxversion, JSONLumpFunc&& parsefunc )
{
	return M_ParseJSONLump( W_CheckNumForName( lumpname ), lumptype, maxversion, parsefunc );
}

#endif //defined( __cplusplus )

#endif // !defined( __M_JSONLUMP_H__ )

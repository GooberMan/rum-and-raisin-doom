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

#if !defined( __M_URL_H__ )
#define __M_URL_H__

#if defined( __cplusplus )
#define URL_C_API extern "C"
#else
#define URL_C_API
#endif
#define URL_INLINE inline

// Must be initialised per-thread
URL_C_API void M_URLInit( void );
URL_C_API void M_URLDeinit( void );

#if defined( __cplusplus )
#include <string>
#include <functional>

using urlprogress_t = std::function< void( ptrdiff_t, ptrdiff_t ) >;

bool M_URLGetString( std::string& output, const char* url, const char* params, urlprogress_t* func = nullptr );
bool M_URLGetBytes( std::vector< unsigned char >& output, int64_t& outfiletime, std::string& outerror, const char* url, const char* params, urlprogress_t* func = nullptr );

URL_INLINE bool M_URLGetString( std::string& output, const char* url, const char* params, urlprogress_t& func )
{
	return M_URLGetString( output, url, params, &func );
}

URL_INLINE bool M_URLGetBytes( std::vector< unsigned char >& output, int64_t& outfiletime, std::string& outerror, const char* url, const char* params, urlprogress_t& func )
{
	return M_URLGetBytes( output, outfiletime, outerror, url, params, &func );
}

#endif // defined( __cplusplus );

#endif //!defined( __M_URL_H__ )

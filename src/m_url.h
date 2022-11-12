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

// Must be initialised per-thread
URL_C_API void M_URLInit( void );
URL_C_API void M_URLDeinit( void );

#if defined( __cplusplus )
#include <string>

bool M_URLGetString( std::string& output, const char* url, const char* params );
#endif // defined( __cplusplus );

#endif //!defined( __M_URL_H__ )

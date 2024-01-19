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
// DESCRIPTION: Instrumented profiling library. Might expand in
// the future to hook in to external profilers too.
//

#if !defined( __M_PROFILE_H__ )
#define __M_PROFILE_H__

#if defined( RNR_PROFILING )
	#define PROFILING_ENABLED 1
#else
	#define PROFILING_ENABLED 0
#endif // defined( RNR_PROFILING )

#if PROFILING_ENABLED
	#define M_PROFILE_PUSH( marker, file, line ) M_ProfilePushMarker( marker, file, line )
	#define M_PROFILE_POP( marker ) M_ProfilePopMarker( marker )
#else // !PROFILING_ENABLED
	#define M_PROFILE_PUSH( marker, file, line )
	#define M_PROFILE_POP( marker )
#endif //PROFILING_ENABLED

#include "doomtype.h"

DOOM_C_API void M_ProfileInit( void );
DOOM_C_API void M_ProfileThreadInit( const char* threadname );
DOOM_C_API void M_ProfileNewFrame( void );

DOOM_C_API void M_ProfilePushMarker( const char* markername, const char* file, size_t line );
DOOM_C_API void M_ProfilePopMarker( const char* markername );

#ifdef __cplusplus

#include "m_container.h"

template< StringLiteral function, StringLiteral file, size_t line >
struct ProfileFunctionRAII
{
	constexpr ProfileFunctionRAII()
	{
		if constexpr( PROFILING_ENABLED ) M_ProfilePushMarker( function.c_str(), file.c_str(), line );
	}

	constexpr ~ProfileFunctionRAII()
	{
		if constexpr( PROFILING_ENABLED ) M_ProfilePopMarker( function.c_str() );
	}
};

#define CONCAT_IMPL a##b
#define CONCAT( a, b ) CONCAT_IMPL( a, b )

#define STR_IMPL a
#define STR( a ) STR_IMPL( a )

#if PROFILING_ENABLED
	#define M_PROFILE_FUNC()		ProfileFunctionRAII< __FUNCTION__, __FILE__, __LINE__ > foofunc = {}
	#define M_PROFILE_NAMED( n )	ProfileFunctionRAII< n, __FILE__, __LINE__ > foonamed = {}
#else // !PROFILING_ENABLED
	#define M_PROFILE_FUNC()
	#define M_PROFILE_NAMED( n )
#endif //PROFILING_ENABLED

#endif // __cplusplus


#endif // __M_PROFILE_H__

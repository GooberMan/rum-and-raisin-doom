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

#ifdef __cplusplus
extern "C" {
#endif

#include "doomtype.h"

void M_ProfileInit( void );
void M_ProfileThreadInit( const char* threadname );
void M_ProfileNewFrame( void );

void M_ProfilePushMarker( const char* markername, const char* file, size_t line );
void M_ProfilePopMarker( const char* markername );

#ifdef __cplusplus
}

template< auto& function, auto& file, size_t line >
struct ProfileFunctionRAII
{
	ProfileFunctionRAII()
	{
		M_ProfilePushFunction( function, file, line );
	}

	~ProfileFunctionRAII()
	{
		M_ProfilePopFunction( function );
	}
};

#define CONCAT_IMPL a ## b
#define CONCAT( a, b ) CONCAT_IMPL( a, b )

#define M_PROFILE_FUNC()		ProfileFunctionRAII< __FUNCTION__, __FILE__, __LINE__ > CONCAT( funcprofile_, __COUNTER__ )
#define M_PROFILE_NAMED( n )	ProfileFunctionRAII< n, __FILE__, __LINE__ > CONCAT( namedprofile_, __COUNTER__ )

#endif


#endif // __M_PROFILE_H__

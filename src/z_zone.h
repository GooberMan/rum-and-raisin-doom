//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020 Ethan Watson
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
// DESCRIPTION:
//      Zone Memory Allocation, perhaps NeXT ObjectiveC inspired.
//	Remark: this was the only stuff that, according
//	 to John Carmack, might have been useful for
//	 Quake.
//



#ifndef __Z_ZONE__
#define __Z_ZONE__

#include <stdio.h>
#include "doomtype.h"

#if defined( __cplusplus )
extern "C" {
#endif // defined( __cplusplus )

//
// ZONE MEMORY
// PU - purge tags.

enum
{
    PU_STATIC = 1,                  // static entire execution time
    PU_SOUND,                       // static while playing
    PU_MUSIC,                       // static while playing
    PU_FREE,                        // a free block
    PU_LEVEL,                       // static until level exited
    PU_LEVSPEC,                     // a special thinker in a level
    
    // Tags >= PU_PURGELEVEL are purgable whenever needed.

    PU_PURGELEVEL,
    PU_CACHE,

    // Total number of different tag types

    PU_NUM_TAGS
};

#if defined( __cplusplus )
}
#endif // defined( __cplusplus )

DOOM_C_API void		Z_Init (void);
DOOM_C_API void*	Z_MallocTracked( const char* file, size_t line, size_t size, int32_t tag, void *ptr );
DOOM_C_API void		Z_Free (void *ptr);
DOOM_C_API void		Z_FreeTags (int lowtag, int hightag);
DOOM_C_API void		Z_DumpHeap (int lowtag, int hightag);
DOOM_C_API void		Z_FileDumpHeap (FILE *f);
DOOM_C_API void		Z_CheckHeap (void);
DOOM_C_API boolean	Z_ChangeTag2 (void *ptr, int tag, const char *file, int line);
DOOM_C_API void		Z_ChangeUser(void *ptr, void **user);
DOOM_C_API size_t	Z_FreeMemory (void);
DOOM_C_API size_t	Z_ZoneSize(void);

#define Z_Malloc( size, tag, ptr ) Z_MallocTracked( __FILE__, __LINE__, size, tag, ptr )

//
// This is used to get the local FILE:LINE info from CPP
// prior to really call the function in question.
//
#define Z_ChangeTag(p,t)                                       \
    Z_ChangeTag2((p), (t), __FILE__, __LINE__)

#if defined( __cplusplus )

#define Z_MallocAs( type, tag, ptr ) Z_MallocTracked< type >( __FILE__, __LINE__, tag, ptr )
#define Z_MallocArrayAs( type, count, tag, ptr ) Z_MallocArrayTracked< type >( __FILE__, __LINE__, count, tag, ptr )

template< typename _ty >
INLINE void Z_MallocInitEntry( _ty*& val )
{
	new( val ) _ty;
}

template< typename _ty >
INLINE _ty* Z_MallocTracked( const char* file, size_t line, int32_t tag, void* ptr )
{
	_ty* val = (_ty*)Z_MallocTracked( file, line, sizeof( _ty ), tag, ptr );
	Z_MallocInitEntry( val );
	return val;
}

template< typename _ty >
INLINE _ty* Z_MallocArrayTracked( const char* file, size_t line, size_t count, int32_t tag, void* ptr )
{
	_ty* val = (_ty*)Z_MallocTracked( file, line, sizeof( _ty ) * count, tag, ptr );
	for( _ty* curr = val; curr < val + count; ++curr )
	{
		Z_MallocInitEntry( curr );
	}
	return val;
}

#endif // defined( __cplusplus )

#endif // __Z_ZONE__

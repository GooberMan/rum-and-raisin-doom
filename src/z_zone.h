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

//
// ZONE MEMORY
// PU - purge tags.

DOOM_C_API typedef enum zonetag_e
{
    PU_STATIC = 1,                  // static entire execution time
    PU_SOUND,                       // static while playing
    PU_MUSIC,                       // static while playing

    PU_LEVEL = 50,                  // static until level exited
    PU_LEVSPEC,                     // a special thinker in a level

    // Tags >= PU_PURGELEVEL are purgable whenever needed.

    PU_PURGELEVEL = 100,
    PU_CACHE,

} zonetag_t;

DOOM_C_API typedef void( *memdestruct_t )( void*, size_t );

DOOM_C_API void		Z_Init (void);
DOOM_C_API void*	Z_MallocTracked( const char* file, size_t line, size_t size, int32_t tag, void **ptr, memdestruct_t destructor );
DOOM_C_API void		Z_Free (void *ptr);
DOOM_C_API void		Z_FreeTags (int lowtag, int hightag);
DOOM_C_API void		Z_DumpHeap (int lowtag, int hightag);
DOOM_C_API void		Z_FileDumpHeap (FILE *f);
DOOM_C_API void		Z_CheckHeap (void);
DOOM_C_API doombool	Z_ChangeTag2 (void *ptr, int tag, const char *file, int line);
DOOM_C_API void		Z_ChangeUser(void *ptr, void **user);

#define Z_Malloc( size, tag, ptr ) Z_MallocTracked( __FILE__, __LINE__, size, tag, (void**)ptr, NULL )

//
// This is used to get the local FILE:LINE info from CPP
// prior to really call the function in question.
//
#define Z_ChangeTag(p,t)                                       \
    Z_ChangeTag2((p), (t), __FILE__, __LINE__)

#if defined( __cplusplus )

#include <type_traits>
#include <new>

#define Z_MallocZero( size, tag, ptr ) Z_MallocTrackedZero( __FILE__, __LINE__, size, tag, (void**)ptr )

#define Z_MallocAs( type, tag, ptr ) Z_MallocTrackedAs< type >( __FILE__, __LINE__, tag, (void**)ptr )
#define Z_MallocAsArgs( type, tag, ptr, ... ) Z_MallocTrackedAs< type >( __FILE__, __LINE__, tag, (void**)ptr, __VA_ARGS__ )
#define Z_MallocArrayAs( type, count, tag, ptr ) Z_MallocArrayTrackedAs< type >( __FILE__, __LINE__, count, tag, (void**)ptr )

template< typename _ty, typename... _args >
INLINE void Z_MallocConstructEntry( _ty*& val, _args&&... args )
{
	new( val ) _ty( args... );
}

template< typename _ty >
INLINE _ty&& Z_MallocDefaultFor()
{
	return {};
}

template< typename _ty >
INLINE void Z_MallocDestructEntry( _ty* val, size_t /*datasize*/ )
{
	if constexpr( !std::is_destructible_v< _ty > )
	{
		val->~_ty();
	}
}

template< typename _ty >
INLINE void Z_MallocDestructEntryArray( _ty* val, size_t datasize )
{
	if constexpr( !std::is_destructible_v< _ty > )
	{
		_ty* curr = (_ty*)( (byte*)val + datasize ) - 1;
		while( curr >= val )
		{
			curr->~_ty();
			--curr;
		}
	}
}

template< typename _ty >
constexpr memdestruct_t Z_DestructorFor()
{
	if constexpr( !std::is_destructible_v< _ty > )
	{
		return (memdestruct_t)&Z_MallocDestructEntry< _ty >;
	}
	else
	{
		return nullptr;
	}
}

template< typename _ty >
constexpr memdestruct_t Z_DestructorForArray()
{
	if constexpr( !std::is_destructible_v< _ty > )
	{
		return (memdestruct_t)&Z_MallocDestructEntryArray< _ty >;
	}
	else
	{
		return nullptr;
	}
}


template< typename _ty, typename... _args >
INLINE _ty* Z_MallocTrackedAs( const char* file, size_t line, int32_t tag, void** ptr, _args&&... args )
{
	_ty* val = (_ty*)Z_MallocTracked( file, line, sizeof( _ty ), tag, ptr, Z_DestructorFor< _ty >() );
	Z_MallocConstructEntry( val, std::forward< _args >( args )... );
	//*val = Z_MallocDefaultFor< _ty >();
	return val;
}

template< typename _ty >
INLINE _ty* Z_MallocArrayTrackedAs( const char* file, size_t line, size_t count, int32_t tag, void** ptr )
{
	_ty* val = (_ty*)Z_MallocTracked( file, line, sizeof( _ty ) * count, tag, ptr, Z_DestructorForArray< _ty >() );
	for( _ty* curr = val; curr < val + count; ++curr )
	{
		Z_MallocConstructEntry( curr );
		//*curr = Z_MallocDefaultFor< _ty >();
	}
	return val;
}

INLINE void* Z_MallocTrackedZero( const char* file, size_t line, size_t size, int32_t tag, void** ptr )
{
	void* output = Z_MallocTracked( file, line, size, tag, ptr, nullptr );
	memset( output, 0, size );
	return output;
}


#endif // defined( __cplusplus )

#endif // __Z_ZONE__

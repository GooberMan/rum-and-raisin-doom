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

#if !defined( __D_DEMOLOOP_H__ )
#define __D_DEMOLOOP_H__

#include "doomtype.h"

DOOM_C_API typedef enum demowipetype_e
{
    dwt_none,
    dwt_melt,

    dwt_max,
} demowipetype_t;

DOOM_C_API typedef enum demolooptype_e
{
	dlt_invalid = -1,
	dlt_artscreen,
	dlt_demo,

	dlt_max,
} demolooptype_t;

typedef struct demoloopentry_s
{
	char					primarylumpname[10];
	int16_t					primarylumppadding;
	char					secondarylumpname[10];
	int16_t					secondarylumppadding;
	uint64_t				tics;
	demolooptype_t			type;
	int32_t					outrowipestyle;
} demoloopentry_t;

typedef struct demoloop_s
{
	const demoloopentry_t*	entries;
	size_t					numentries;
	
	const demoloopentry_t*	current;
	uint64_t				ticsleft;

#if defined( __cplusplus )
	INLINE void Reset()
	{
		current = entries + ( numentries - 1 );
		ticsleft = 0;
	}

	INLINE void SetEntries( const demoloopentry_t* e, size_t c )
	{
		entries = e;
		numentries = c;
		current = e + ( c - 1 );
		ticsleft = 0;
	}

	template< size_t count >
	INLINE void SetEntries( const demoloopentry_t( &e )[ count ] )
	{
		SetEntries( e, count );
	}

	INLINE void Advance()
	{
		if( ++current == entries + numentries )
		{
			current = entries;
		}
		ticsleft = current->tics;
	}
#endif // defined( __cplusplus )
} demoloop_t;

DOOM_C_API demoloop_t* D_CreateDemoLoop();

#endif // !defined( __D_DEMOLOOP_H__ )
//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
//  MapObj data. Map Objects or mobjs are actors, entities,
//  thinker, take-your-pick... anything that moves, acts, or
//  suffers state changes of more or less violent nature.
//


#ifndef __D_THINK__
#define __D_THINK__

#include "doomtype.h"

//
// Experimental stuff.
// To compile this as "ANSI C with classes"
//  we will need to handle the various
//  action functions cleanly.
//

DOOM_C_API typedef struct mobj_s mobj_t;
DOOM_C_API typedef struct player_s player_t;
DOOM_C_API typedef struct pspdef_s pspdef_t;
DOOM_C_API typedef struct thinker_s thinker_t;


DOOM_C_API typedef  void (*actionf_v)();
DOOM_C_API typedef  void (*actionf_p1)( thinker_t* );
DOOM_C_API typedef  void (*actionf_p2)( player_t*, pspdef_t* );

DOOM_C_API typedef enum actiontype_e
{
	at_none,
	at_void,
	at_p1,
	at_p2,
} actiontype_t;

DOOM_C_API typedef struct actionf_s
{
	union
	{
		actionf_v		acv;
		actionf_p1		acp1;
		actionf_p2		acp2;
	};
	actiontype_t type;

#if defined(__cplusplus)
	actionf_s()
		: acv( nullptr )
	{
	}

	actionf_s( actionf_v func )
		: acv( func )
		, type( at_void )
	{
	}
	actionf_s( actionf_p1 func )
		: acp1( func )
		, type( at_p1 )
	{
	}
	actionf_s( actionf_p2 func )
		: acp2( func )
		, type( at_p2 )
	{
	}
#endif // defined(__cplusplus)
} actionf_t;

// Historically, "think_t" is yet another
//  function pointer to a routine to handle
//  an actor.
DOOM_C_API typedef actionf_t  think_t;


// Doubly linked list of actors.
DOOM_C_API typedef struct thinker_s
{
	// Changing the order from vanilla for better type punning
	think_t				function;
	struct thinker_s*	prev;
	struct thinker_s*	next;
};

#if defined( __cplusplus )

#include <array>

template< typename _ty >
struct thinkfunclookup
{
protected:
	template< typename... _args >
	static constexpr std::array< void*, sizeof...( _args ) > toarray( _args&&... args)
	{
		return { (void*)args... };
	}
};

#define MakeThinkFuncLookup( type, ... ) template<> \
struct thinkfunclookup< type > : thinkfunclookup< void > \
{ \
	constexpr static auto funcs = toarray( __VA_ARGS__ ); \
}

template< typename _to, typename _from >
constexpr _to* thinker_cast( _from* from )
{
	auto found = std::find( thinkfunclookup< _to >::funcs.begin(), thinkfunclookup< _to >::funcs.end(), *(void**)from );
	return found == thinkfunclookup< _to >::funcs.end() ? nullptr : (_to*)from;
}
#endif


#endif

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
DOOM_C_API typedef  void (*actionf_v)();
DOOM_C_API typedef  void (*actionf_p1)( void* );
DOOM_C_API typedef  void (*actionf_p2)( void*, void* );

DOOM_C_API typedef union
{
  actionf_v		acv;
  actionf_p1	acp1;
  actionf_p2	acp2;
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
} thinker_t;

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

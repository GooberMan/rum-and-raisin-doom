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
#include "i_error.h"

#if defined(__cplusplus)
#include <type_traits>
#endif //defined(__cplusplus)

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

typedef struct actionf_s
{
#if defined(__cplusplus)
	actionf_s()
		: _acv( nullptr )
	{
	}

	actionf_s( actionf_v func )
		: _acv( func )
		, type( at_void )
	{
	}

	actionf_s( actionf_p1 func )
		: _acp1( func )
		, type( at_p1 )
	{
	}

	actionf_s( actionf_p2 func )
		: _acp2( func )
		, type( at_p2 )
	{
	}

	actionf_s(std::nullptr_t v)
		: _acv( v )
		, type( at_none )
	{
	}

	template< typename _param,
			std::enable_if_t< std::is_same_v< _param*, decltype( thinker_cast< _param >( (_param*)nullptr ) ) >, bool > = true >
	actionf_s( void(*func)(_param*) )
		: _acp1( (actionf_p1)func )
		, type( at_p1 )
	{
	}

	actionf_s& operator=(std::nullptr_t v)
	{
		_acv = v;
		type = at_none;
		return *this;
	}

	template< typename _param >
	actionf_s& operator=( void(*func)(_param*) )
	{
		if constexpr( !std::is_same_v< _param*, decltype( thinker_cast< _param >( (_param*)nullptr ) ) > )
		{
			I_Error( "Non-thinker assignment of actionf_t" );
		}

		_acp1 = (actionf_p1)func;
		type = at_p1;
		return *this;
	}

	void operator()()
	{
		if( type != at_void )
		{
			I_Error( "Invoking wrong actionf_t type" );
		}
		_acv();
	}

	template< typename _param >
	void operator()( _param* val )
	{
		if( type != at_p1 )
		{
			I_Error( "Invoking wrong actionf_t type" );
		}

		if constexpr( !std::is_same_v< _param*, decltype( thinker_cast< _param >( (_param*)nullptr ) ) > )
		{
			I_Error( "Non-thinker invocation of actionf_t" );
		}

		_acp1( (thinker_t*)val );
	}

	void operator()( player_t* player, pspdef_t* pspr )
	{
		if( type != at_p2 )
		{
			I_Error( "Invoking wrong actionf_t type" );
		}

		_acp2( player, pspr );
	}

	template< typename _param >
	bool operator==( void(*func)(_param*) )
	{
		if constexpr( !std::is_same_v< _param*, decltype( thinker_cast< _param >( (_param*)nullptr ) ) > )
		{
			I_Error( "Non-thinker comparison of actionf_t" );
		}

		return type == at_p1 && _acp1 == (actionf_p1)func;
	}

	inline bool Valid()
	{
		return type != at_none && _acv != nullptr;
	}
private:
#endif // defined(__cplusplus)
	union
	{
		actionf_v		_acv;
		actionf_p1		_acp1;
		actionf_p2		_acp2;
	};
	actiontype_t type;
} actionf_t;

// Doubly linked list of actors.
DOOM_C_API struct thinker_s
{
	// Changing the order from vanilla for better type punning
	actionf_t			function;
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

#define MakeThinkFuncLookup( type, ... ) \
template<> \
constexpr thinker_t* thinker_cast< thinker_t >( type* from ) \
{ \
	return (thinker_t*)from; \
} \
 \
template<> \
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

template<>
constexpr thinker_t* thinker_cast< thinker_t >( thinker_t* val )
{
	return val;
}
#endif


#endif

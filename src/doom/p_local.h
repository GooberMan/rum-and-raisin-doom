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
//	Play functions, animation, global header.
//


#ifndef __P_LOCAL__
#define __P_LOCAL__

#define FLOATSPEED		(FRACUNIT*4)

#define MAXHEALTH		100
#define VIEWHEIGHT		(41*FRACUNIT)

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS	128
#define MAPBLOCKSIZE	(MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT	(FRACBITS+7)
#define MAPBMASK		(MAPBLOCKSIZE-1)
#define MAPBTOFRAC		(MAPBLOCKSHIFT-FRACBITS)


// player radius for movement checking
#define PLAYERRADIUS		16 * FRACUNIT
#define RENDPLAYERRADIUS	16 * RENDFRACUNIT

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS		32*FRACUNIT

#define GRAVITY		FRACUNIT
#define MAXMOVE		(30*FRACUNIT)

#define USERANGE		(64*FRACUNIT)
#define MELEERANGE		(64*FRACUNIT)
#define MISSILERANGE	(32*64*FRACUNIT)

// follow a player exlusively for 3 seconds
#define	BASETHRESHOLD	 	100

// Movement constants
#define STOPSPEED		0x1000 // 0.0625
#define FRICTION		0xe800 // 0.90625

#include "r_local.h"

DOOM_C_API typedef enum damage_e
{
	damage_none					= 0x00000000,
	damage_totalkill			= 0x00000001,
	damage_alsononshootables	= 0x00000002,
	damage_nothrust				= 0x00000004,
	damage_ignorearmor			= 0x00000008,
	damage_melee				= 0x00000010,

	damage_theworks				= damage_totalkill | damage_alsononshootables | damage_ignorearmor | damage_nothrust,
} damage_t;

//
// P_TICK
//

// both the head and tail of the thinker list
DOOM_C_API extern	thinker_t	thinkercap;


DOOM_C_API void P_InitThinkers (void);
DOOM_C_API void P_AddThinker (thinker_t* thinker);
DOOM_C_API void P_RemoveThinker (thinker_t* thinker);


//
// P_PSPR
//
DOOM_C_API void P_SetupPsprites (player_t* curplayer);
DOOM_C_API void P_MovePsprites (player_t* curplayer);
DOOM_C_API void P_DropWeapon (player_t* player);


//
// P_USER
//
DOOM_C_API void	P_PlayerThink (player_t* player);


//
// P_MOBJ
//
#define ONFLOORZ		INT_MIN
#define ONCEILINGZ		INT_MAX

// Time interval for item respawning.
#define ITEMQUESIZE		128

DOOM_C_API extern mapthing_t	itemrespawnque[ITEMQUESIZE];
DOOM_C_API extern int		itemrespawntime[ITEMQUESIZE];
DOOM_C_API extern int		iquehead;
DOOM_C_API extern int		iquetail;


DOOM_C_API void P_RespawnSpecials (void);

DOOM_C_API mobj_t*		P_SpawnMobj( fixed_t x, fixed_t y, fixed_t z, int32_t type );
DOOM_C_API mobj_t*		P_SpawnMobjEx( const mobjinfo_t* typeinfo, angle_t angle,
										fixed_t x, fixed_t y, fixed_t z,
										fixed_t forwardvel, fixed_t rightvel, fixed_t upvel );
DOOM_C_API void			P_CheckMissileSpawn( mobj_t* th );

DOOM_C_API void			P_RemoveMobj (mobj_t* th);
DOOM_C_API mobj_t*		P_SubstNullMobj (mobj_t* th);
DOOM_C_API doombool		P_SetMobjState (mobj_t* mobj, int32_t state);
DOOM_C_API void			P_MobjThinker (mobj_t* mobj);

DOOM_C_API void			P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z);
DOOM_C_API void			P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage);
DOOM_C_API mobj_t*		P_SpawnMissile (mobj_t* source, mobj_t* dest, int32_t type);
DOOM_C_API void			P_SpawnPlayerMissile (mobj_t* source, int32_t type);


//
// P_ENEMY
//
DOOM_C_API void			P_NoiseAlert (mobj_t* target, mobj_t* emmiter);
DOOM_C_API doombool		P_CheckMeleeRange( mobj_t* actor );

//
// P_MAPUTL
//

DOOM_C_API typedef struct
{
    fixed_t	frac;		// along trace line
    doombool	isaline;
    union {
	mobj_t*	thing;
	line_t*	line;
    }			d;
} intercept_t;

// Extended MAXINTERCEPTS, to allow for intercepts overrun emulation.

#define MAXINTERCEPTS           128

DOOM_C_API extern intercept_t*	intercepts;
DOOM_C_API extern intercept_t*	intercept_p;
DOOM_C_API extern size_t		interceptscount;

typedef doombool (*traverser_t) (intercept_t *in);

DOOM_C_API fixed_t			P_AproxDistance (fixed_t dx, fixed_t dy);
DOOM_C_API int				P_PointOnLineSide (fixed_t x, fixed_t y, line_t* line);
DOOM_C_API int				P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t* line);
DOOM_C_API void				P_MakeDivline (line_t* li, divline_t* dl);
DOOM_C_API fixed_t			P_InterceptVector (divline_t* v2, divline_t* v1);
DOOM_C_API int				P_BoxOnLineSide (fixed_t* tmbox, line_t* ld);

DOOM_C_API extern fixed_t	opentop;
DOOM_C_API extern fixed_t 	openbottom;
DOOM_C_API extern fixed_t	openrange;
DOOM_C_API extern fixed_t	lowfloor;

DOOM_C_API void				P_LineOpening (line_t* linedef);

DOOM_C_API doombool			P_BlockLinesIterator (int x, int y, doombool(*func)(line_t*) );
DOOM_C_API doombool			P_BlockThingsIterator (int x, int y, doombool(*func)(mobj_t*) );

DOOM_C_API doombool			P_BBoxOverlapsSector( sector_t* sector, fixed_t* bbox );
DOOM_C_API doombool			P_MobjOverlapsSector( sector_t* sector, mobj_t* mobj );

#if defined( __cplusplus )
#include <functional>
#include "m_container.h"

using iteratemobjfunc_t = std::function< doombool(mobj_t*) >;

doombool P_BlockThingsIterator( int32_t x, int32_t y, iteratemobjfunc_t&& func );

// Ranged iterators with directional bias, will travel all the way in one direction before moving to the next line
doombool P_BlockThingsIteratorHorizontal( iota&& xrange, iota&& yrange, iteratemobjfunc_t&& func );
doombool P_BlockThingsIteratorVertical( iota&& xrange, iota&& yrange, iteratemobjfunc_t&& func );
doombool P_BlockThingsIteratorHorizontal( fixed_t x, fixed_t y, fixed_t radius, iteratemobjfunc_t&& func );
doombool P_BlockThingsIteratorVertical( fixed_t x, fixed_t y, fixed_t radius, iteratemobjfunc_t&& func );
#endif

#define PT_ADDLINES		1
#define PT_ADDTHINGS	2
#define PT_EARLYOUT		4

DOOM_C_API extern divline_t	trace;

DOOM_C_API doombool			P_PathTraverse( fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, doombool (*trav) (intercept_t *));

DOOM_C_API void				P_UnsetThingPosition (mobj_t* thing);
DOOM_C_API void				P_SetThingPosition (mobj_t* thing);


//
// P_MAP
//

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
DOOM_C_API extern doombool		floatok;
DOOM_C_API extern fixed_t		tmfloorz;
DOOM_C_API extern fixed_t		tmceilingz;


DOOM_C_API extern	line_t*		ceilingline;

// fraggle: I have increased the size of this buffer.  In the original Doom,
// overrunning past this limit caused other bits of memory to be overwritten,
// affecting demo playback.  However, in doing so, the limit was still
// exceeded.  So we have to support more than 8 specials.
//
// We keep the original limit, to detect what variables in memory were
// overwritten (see SpechitOverrun())

#define MAXSPECIALCROSS		8

DOOM_C_API extern	line_t** spechit;
DOOM_C_API extern	int32_t	numspechit;
DOOM_C_API extern	size_t	spechitcount;

DOOM_C_API typedef enum checkposflags_e
{
	CP_None						= 0x00000000,
	CP_CorpseChecksAlways		= 0x00000001,
} checkposflags_t;

DOOM_C_API doombool			P_CheckPosition (mobj_t *thing, fixed_t x, fixed_t y, checkposflags_t flags = CP_None);
DOOM_C_API mobj_t*			P_XYMovement (mobj_t* mo); // Returns the mobj if it still exists after movement
DOOM_C_API doombool			P_TryMove (mobj_t* thing, fixed_t x, fixed_t y);
DOOM_C_API doombool			P_TeleportMove (mobj_t* thing, fixed_t x, fixed_t y);
DOOM_C_API void				P_SlideMove (mobj_t* mo);
DOOM_C_API doombool			P_CheckSight (mobj_t* t1, mobj_t* t2);
DOOM_C_API void				P_UseLines (player_t* player);

DOOM_C_API doombool			P_ChangeSector (sector_t* sector, doombool crunch);

DOOM_C_API extern mobj_t*	linetarget;	// who got hit (or NULL)

DOOM_C_API fixed_t			P_AimLineAttack( mobj_t* t1, angle_t angle, fixed_t distance );

DOOM_C_API void				P_LineAttack( mobj_t* t1, angle_t angle, fixed_t distance, fixed_t slope, int damage, damage_t damageflags = damage_none );

DOOM_C_API void				P_RadiusAttack( mobj_t* spot, mobj_t* source, int32_t damage );
DOOM_C_API void				P_RadiusAttackDistance( mobj_t* spot, mobj_t* source, int32_t damage, fixed_t distance );
DOOM_C_API mobj_t*			P_FindTracerTarget( mobj_t* source, fixed_t distance, angle_t fov );

//
// P_SETUP
//
DOOM_C_API extern byte*				rejectmatrix;	// for fast sight rejection
DOOM_C_API extern blockmap_t*		blockmap;
DOOM_C_API extern blockmap_t*		blockmapbase;
DOOM_C_API extern int32_t			bmapwidth;
DOOM_C_API extern int32_t			bmapheight;	// in mapblocks
DOOM_C_API extern fixed_t			bmaporgx;
DOOM_C_API extern fixed_t			bmaporgy;	// origin of block map
DOOM_C_API extern mobj_t**			blocklinks;	// for thing chains



//
// P_INTER
//
DOOM_C_API extern int		maxammo[NUMAMMO];
DOOM_C_API extern int		clipammo[NUMAMMO];

DOOM_C_API void P_TouchSpecialThing( mobj_t* special,  mobj_t* toucher );

DOOM_C_API void P_DamageMobj( mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage, damage_t flags );
DOOM_C_API void P_DamageMobjEx( mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage, damage_t flags );
DOOM_C_API void P_KillMobj( mobj_t* source, mobj_t* target );


//
// P_SPEC
//
#include "p_spec.h"



#endif	// __P_LOCAL__

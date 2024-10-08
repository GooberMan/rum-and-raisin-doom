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
//
//


#ifndef __D_PLAYER__
#define __D_PLAYER__


// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "d_items.h"
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "p_mobj.h"

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "d_ticcmd.h"

#include "net_defs.h"

#include "z_zone.h"

#if defined( __cplusplus )
constexpr angle_t MaxViewPitchAngle = ANG60;
#endif //defined( __cplusplus )


//
// Player states.
//
DOOM_C_API typedef enum
{
    // Playing or camping.
    PST_LIVE,
    // Dead on the ground, view follows killer.
    PST_DEAD,
    // Ready to restart/respawn???
    PST_REBORN		

} playerstate_t;


//
// Player internal flags, for cheats and debug.
//
DOOM_C_API typedef enum
{
    // No clipping, walk through barriers.
    CF_NOCLIP		= 1,
    // No damage, no health loss.
    CF_GODMODE		= 2,
    // Not really a cheat, just a debug aid.
    CF_NOMOMENTUM	= 4

} cheat_t;

DOOM_C_API typedef struct owneditem_s
{
	int32_t			index;
	int32_t			amount;
} owneditem_t;

DOOM_C_API typedef struct owneddata_s
{
	owneditem_t*	data;
	size_t			length;

#if defined( __cplusplus )
	INLINE int32_t& operator[]( int32_t index )
	{
		owneditem_t* end = data + length;
		auto found = std::find_if( data, end, [&index]( const owneditem_t& owned ) { return owned.index == index; } );
		if( found == end )
		{
			I_Error( "owneditem: invalid index %d", index );
		}
		return found->amount;
	}

	INLINE void ResetWeapon()
	{
		length = sizeof( owneditem_t ) * weaponinfo.size();
		if( !data )
		{
			data = (owneditem_t*)Z_MallocZero( length, PU_STATIC, nullptr );
		}

		owneditem_t* curr = data;
		for( weaponinfo_t* weapon : weaponinfo.All() )
		{
			*curr = { weapon->index, 0 };
			++curr;
		}
	}

	INLINE void ResetAmmo()
	{
		length = sizeof( owneditem_t ) * ammoinfo.size();
		if( !data )
		{
			data = (owneditem_t*)Z_MallocZero( length, PU_STATIC, nullptr );
		}

		owneditem_t* curr = data;
		for( ammoinfo_t* ammo : ammoinfo.All() )
		{
			*curr = { ammo->index, 0 };
			++curr;
		}
	}
#endif // defined( __cplusplus )
} owneddata_t;

//
// Extended player object info: player_t
//
DOOM_C_API typedef struct player_s
{
    mobj_t*		mo;
    playerstate_t	playerstate;
    ticcmd_t		cmd;

    // Determine POV,
    //  including viewpoint bobbing during movement.
    // Focal origin above r.z
    fixed_t		viewz;
    // Base height above floor for viewz.
    fixed_t		viewheight;
    // Bob/squat speed.
    fixed_t         	deltaviewheight;
    // bounded/scaled total momentum.
    fixed_t         	bob;

	angle_t				viewpitch;

	rend_fixed_t prevviewz;
	rend_fixed_t currviewz;

    // This is only used between levels,
    // mo->health is used during levels.
    int			health;	
    int			armorpoints;
    // Armor type is 0-2.
    int			armortype;	

    // Power ups. invinc and invis are tic counters.
    int			powers[NUMPOWERS];
    doombool	cards[NUMCARDS];
    doombool	backpack;
    
    // Frags, kills of other players.
    int			frags[MAXPLAYERS];
    int32_t		readyweapon;
    
    // Is wp_nochange if not changing.
    int32_t		pendingweapon;

    owneddata_t	weaponowned;
    owneddata_t	ammo;
    owneddata_t	maxammo;

    // True if button down last tic.
    int			attackdown;
    int			usedown;

    // Bit flags, for cheats and debug.
    // See cheat_t, above.
    int			cheats;		

    // Refired shots are less accurate.
    int			refire;		

     // For intermission stats.
    int			killcount;
    int			itemcount;
    int			secretcount;

    // Hint messages.
    const char		*message;
    
    // For screen flashing (red or bright).
    int			damagecount;
    int			bonuscount;

    // Who did damage (NULL for floors/ceilings).
    mobj_t*		attacker;
    
    // So gun flashes light up areas.
    int			extralight;

    // Current PLAYPAL, ???
    //  can be set to REDCOLORMAP for pain, etc.
    int			fixedcolormap;

    // Player skin colorshift,
    //  0-3 for which color to draw player.
    int			colormap;	

    // Overlay view sprites (gun, etc).
    pspdef_t		psprites[NUMPSPRITES];

    // True if secret level has been done.
    doombool		didsecret;	

	size_t		anymomentumframes;

	doombool*	visitedlevels;

#if defined(__cplusplus)
	INLINE const weaponinfo_t& Weapon() const { return weaponinfo[ readyweapon ]; }
#endif // defined(__cplusplus)
} player_t;


//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
DOOM_C_API typedef struct
{
    doombool	in;	// whether the player is in game
    
    // Player stats, kills, collected items etc.
    int		skills;
    int		sitems;
    int		ssecret;
    int		stime; 
    int		frags[4];
    int		score;	// current score on entry, modified on return
    doombool* visited;
} wbplayerstruct_t;

DOOM_C_API typedef struct
{
	void*	currmap;
	void*	nextmap;

    // if true, splash the secret level
    doombool	didsecret;
    
    // previous and next levels, origin 0
	doombool	nextissecret;
    
    int		maxkills;
    int		maxitems;
    int		maxsecret;
    int		maxfrags;

    // the par time
    int		partime;
    
    // index of this player in game
    int		pnum;	

    wbplayerstruct_t	plyr[MAXPLAYERS];

} wbstartstruct_t;


#endif

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
//  Internally used data structures for virtually everything,
//   lots of other stuff.
//

#ifndef __DOOMDEF__
#define __DOOMDEF__

#include <stdio.h>
#include <string.h>

#include "doomtype.h"

#include "i_timer.h"
#include "d_mode.h"

//
// Global parameters/defines.
//
// DOOM version
#define DOOM_VERSION 109

// Version code for cph's longtics hack ("v1.91")
#define DOOM_191_VERSION 111


// If rangecheck is undefined,
// most parameter validation debugging code will not be compiled
#define RANGECHECK 1

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS 4

// The current state of the game: whether we are
// playing, gazing at the intermission screen,
// the game final animation, or a demo. 
DOOM_C_API typedef enum
{
	GS_INVALID = -1,
    GS_LEVEL,
    GS_INTERMISSION,
    GS_FINALE,
    GS_DEMOSCREEN,
} gamestate_t;

DOOM_C_API typedef enum
{
    ga_nothing,
    ga_loadlevel,
    ga_newgame,
    ga_loadgame,
    ga_savegame,
    ga_playdemo,
    ga_completed,
    ga_victory,
    ga_worlddone,
    ga_screenshot
} gameaction_t;

//
// Difficulty/skill settings/filters.
//

// Skill flags.
#define	MTF_EASY		1
#define	MTF_NORMAL		2
#define	MTF_HARD		4

// Deaf monsters/do not react to sound.
#define	MTF_AMBUSH		8

#define MTF_MULTIPLAYER_ONLY		16

#define MTF_BOOM_NOT_IN_DEATHMATCH	32
#define MTF_BOOM_NOT_IN_COOP		64

#define MTF_MBF_FRIENDLY			128
#define MTF_MBF_RUBBISHFLAGCHECK	256


//
// Key cards.
//
DOOM_C_API typedef enum
{
    it_bluecard,
    it_yellowcard,
    it_redcard,
    it_blueskull,
    it_yellowskull,
    it_redskull,
    
    NUMCARDS
    
} card_t;



// The defined weapons,
//  including a marker indicating
//  user has not changed weapon.
DOOM_C_API typedef enum
{
    // No pending weapon change.
    wp_nochange = -1,

    wp_fist,
    wp_pistol,
    wp_shotgun,
    wp_chaingun,
    wp_missile,
    wp_plasma,
    wp_bfg,
    wp_chainsaw,
    wp_supershotgun,

    NUMWEAPONS,
} weapontype_t;


// Ammunition types defined.
DOOM_C_API typedef enum
{
    am_noammo = -1,	// Unlimited for chainsaw / fist.

    am_clip,	// Pistol / chaingun ammo.
    am_shell,	// Shotgun / double barreled shotgun.
    am_cell,	// Plasma rifle, BFG.
    am_misl,	// Missile launcher.

    NUMAMMO,
} ammotype_t;

DOOM_C_API typedef enum ammocategory_e
{
	ac_nocategory			= -1,
	ac_clip					= 0x0000,
	ac_box					= 0x0001,
	ac_weapon				= 0x0002,
	ac_backpack				= 0x0003,
	ac_dropped				= 0x0004,
	ac_deathmatch			= 0x0008,

	ac_droppedclip			= ac_dropped | ac_clip,
	ac_droppedbox			= ac_dropped | ac_box,
	ac_droppedweapon		= ac_dropped | ac_weapon,
	ac_droppedbackpack		= ac_dropped | ac_backpack,

	ac_deathmatchweapon		= ac_deathmatch | ac_weapon,
} ammocategory_t;

// Power up artifacts.
DOOM_C_API typedef enum
{
	pw_nopower = -1,
    pw_invulnerability,
    pw_strength,
    pw_invisibility,
    pw_ironfeet,
    pw_allmap,
    pw_infrared,
    NUMPOWERS
    
} powertype_t;

DOOM_C_API typedef enum itemtype_e
{
	item_noitem = -1,
	item_messageonly,
	item_bluecard,
	item_yellowcard,
	item_redcard,
	item_blueskull,
	item_yellowskull,
	item_redskull,
	item_backpack,
	item_healthbonus,
	item_stimpack,
	item_medikit,
	item_soulsphere,
	item_megasphere,
	item_armorbonus,
	item_greenarmor,
	item_bluearmor,
	item_areamap,
	item_lightamp,
	item_berserk,
	item_invisibility,
	item_radsuit,
	item_invulnerability,
} itemtype_t;

//
// Power up durations,
//  how many seconds till expiration,
//  assuming TICRATE is 35 ticks/second.
//
DOOM_C_API typedef enum
{
    INVULNTICS	= (30*TICRATE),
    INVISTICS	= (60*TICRATE),
    INFRATICS	= (120*TICRATE),
    IRONTICS	= (60*TICRATE)
    
} powerduration_t;

DOOM_C_API typedef enum gameflags_e
{
	GF_None							= 0,

	GF_RespawnMonsters				= 0x00000001,
	GF_FastMonsters					= 0x00000002,

	GF_Pacifist						= 0x08000000,

	GF_PistolStarts					= 0x10000000,
	GF_LoopOneLevel					= 0x20000000,

	GF_VanillaIncompatibleFlags		= GF_Pacifist | GF_PistolStarts | GF_LoopOneLevel,

} gameflags_t;

typedef enum demoversion_e
{
	demo_invalid		= -1,
	demo_doom_1_666		= 106,
	demo_doom_1_7		= 107,
	demo_doom_1_8		= 108,
	demo_doom_1_9		= 109,
	demo_doom_1_91		= 111,
	demo_boom_2_02		= 202,
	demo_mbf			= 203,
	demo_complevel11	= 214,
	demo_mbf21			= 221,
} demoversion_t;

#endif          // __DOOMDEF__

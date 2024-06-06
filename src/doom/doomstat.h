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
//   All the global variables that store the internal state.
//   Theoretically speaking, the internal state of the engine
//    should be found by looking at the variables collected
//    here, and every relevant module will have to include
//    this header file.
//   In practice, things are a bit messy.
//


#ifndef __D_STATE__
#define __D_STATE__

// We need globally shared data structures,
//  for defining the global state variables.
#include "doomdata.h"
#include "m_fixed.h"

#include "d_loop.h"

// We need the playr data structure as well.
#include "d_player.h"

// Game mode/mission
#include "d_mode.h"

#include "net_defs.h"

DOOM_C_API typedef struct sessionstats_s
{
	int32_t		start_total_monsters;
	int32_t		start_total_secrets;
	int32_t		start_total_items;

	int32_t		resurrected_monsters;
	int32_t		total_resurrected_monster_kills[ MAXPLAYERS ];
	int32_t		total_resurrected_monster_kills_global;

	int32_t		total_monster_kills[ MAXPLAYERS ];
	int32_t		total_monster_kills_global;

	int32_t		total_found_secrets[ MAXPLAYERS ];
	int32_t		total_found_secrets_global;

	int32_t		total_found_items[ MAXPLAYERS ];
	int32_t		total_found_items_global;

	uint64_t	level_time;
	uint64_t	session_time;
} sessionstats_t;

DOOM_C_API extern sessionstats_t session;

// ------------------------
// Command line parameters.
//
DOOM_C_API extern  doombool	nomonsters;	// checkparm of -nomonsters
DOOM_C_API extern  doombool	respawnparm;	// checkparm of -respawn
DOOM_C_API extern  doombool	fastparm;	// checkparm of -fast

DOOM_C_API extern  doombool	devparm;	// DEBUG: launched with -devparm


// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//
DOOM_C_API extern GameMode_t	gamemode;
DOOM_C_API extern GameMission_t	gamemission;
DOOM_C_API extern GameVersion_t    gameversion;
DOOM_C_API extern GameVariant_t    gamevariant;

// Convenience macro.
// 'gamemission' can be equal to pack_chex or pack_hacx, but these are
// just modified versions of doom and doom2, and should be interpreted
// as the same most of the time.

#define logical_gamemission                             \
    (gamemission == pack_chex ? doom :                  \
     gamemission == pack_hacx ? doom2 : gamemission)

// Set if homebrew PWAD stuff has been added.
DOOM_C_API extern  doombool	modifiedgame;


// -------------------------------------------
// Selected skill type, map etc.
//

// Defaults for menu, methinks.
DOOM_C_API extern  skill_t		startskill;
DOOM_C_API extern  int             startepisode;
DOOM_C_API extern	int		startmap;

// Savegame slot to load on startup.  This is the value provided to
// the -loadgame option.  If this has not been provided, this is -1.

DOOM_C_API extern  int             startloadgame;

DOOM_C_API extern  doombool		autostart;

// Selected by user. 
DOOM_C_API extern  skill_t         gameskill;

// If non-zero, exit the level after this number of minutes
DOOM_C_API extern  int             timelimit;

// Nightmare mode flag, single player.
DOOM_C_API extern  doombool         respawnmonsters;
DOOM_C_API extern  doombool         fastmonsters;

// Netgame? Only true if >1 player.
DOOM_C_API extern  doombool	netgame;
DOOM_C_API extern doombool solonetgame;

// 0=Cooperative; 1=Deathmatch; 2=Altdeath
DOOM_C_API extern int deathmatch;

// -------------------------
// Internal parameters for sound rendering.
// These have been taken from the DOS version,
//  but are not (yet) supported with Linux
//  (e.g. no sound volume adjustment with menu.

// From m_menu.c:
//  Sound FX volume has default, 0 - 15
//  Music volume has default, 0 - 15
// These are multiplied by 8.
DOOM_C_API extern int sfxVolume;
DOOM_C_API extern int musicVolume;

// Current music/sfx card - index useless
//  w/o a reference LUT in a sound module.
// Ideally, this would use indices found
//  in: /usr/include/linux/soundcard.h
DOOM_C_API extern int snd_MusicDevice;
DOOM_C_API extern int snd_SfxDevice;
// Config file? Same disclaimer as above.
DOOM_C_API extern int snd_DesiredMusicDevice;
DOOM_C_API extern int snd_DesiredSfxDevice;


// -------------------------
// Status flags for refresh.
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//  status bar explicitely.
DOOM_C_API extern  doombool statusbaractive;

DOOM_C_API extern  doombool automapactive;	// In AutoMap mode?
DOOM_C_API extern  doombool	menuactive;	// Menu overlayed?
DOOM_C_API extern  int32_t dashboardactive; // R&R Dashboard, powered by Dear ImGui
DOOM_C_API extern  int32_t dashboardremappingtype;
DOOM_C_API extern  int32_t dashboardpausesplaysim;
DOOM_C_API extern  doombool	paused;		// Game Pause?
DOOM_C_API extern  doombool	renderpaused;


DOOM_C_API extern  doombool		viewactive;

DOOM_C_API extern  doombool		nodrawers;


DOOM_C_API extern  doombool         testcontrols;
DOOM_C_API extern  int             testcontrols_mousespeed;




// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
DOOM_C_API extern  int	viewangleoffset;

// Player taking events, and displaying.
DOOM_C_API extern  int	consoleplayer;	
DOOM_C_API extern  int	displayplayer;


// -------------------------------------
// Scores, rating.
// Statistics on a given map, for intermission.
//
DOOM_C_API extern  int	totalkills;
DOOM_C_API extern	int	totalitems;
DOOM_C_API extern	int	totalsecret;

// Timer, for scores.
DOOM_C_API extern  int	levelstarttic;	// gametic at level start
DOOM_C_API extern  int	leveltime;	// tics in game play for par



// --------------------------------------
// DEMO playback/recording related stuff.
// No demo, there is a human player in charge?
// Disable save/end game?
DOOM_C_API extern  doombool	usergame;

//?
DOOM_C_API extern  doombool	demoplayback;
DOOM_C_API extern  doombool	demorecording;

// Round angleturn in ticcmds to the nearest 256.  This is used when
// recording Vanilla demos in netgames.

DOOM_C_API extern doombool lowres_turn;

// Quit after playing a demo from cmdline.
DOOM_C_API extern  doombool		singledemo;	




//?
DOOM_C_API extern  gamestate_t     gamestate;






//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.



// Bookkeeping on players - state.
DOOM_C_API extern	player_t	players[MAXPLAYERS];

// Alive? Disconnected?
DOOM_C_API extern  doombool		playeringame[MAXPLAYERS];


// Player spawn spots for deathmatch.
#define MAX_DM_STARTS   10
DOOM_C_API extern  mapthing_t      deathmatchstarts[MAX_DM_STARTS];
DOOM_C_API extern  mapthing_t*	deathmatch_p;

// Player spawn spots.
DOOM_C_API extern  mapthing_t      playerstarts[MAXPLAYERS];
DOOM_C_API extern  doombool         playerstartsingame[MAXPLAYERS];
// Intermission stats.
// Parameters for world map / intermission.
DOOM_C_API extern  wbstartstruct_t		wminfo;	







//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
DOOM_C_API extern  char        *savegamedir;

// if true, load all graphics at level load
DOOM_C_API extern  doombool         precache;


// wipegamestate can be set to -1
//  to force a wipe on the next draw
DOOM_C_API extern  gamestate_t     wipegamestate;

DOOM_C_API extern  int             mouseSensitivity;

DOOM_C_API extern  int             bodyqueslot;

// Netgame stuff (buffers and pointers, i.e. indices).

DOOM_C_API extern	int		rndindex;

DOOM_C_API extern  ticcmd_t       *netcmds;

DOOM_C_API extern int32_t remove_limits;

#if defined(__cplusplus)
#include "i_thread.h"

extern std::unique_ptr< JobSystem > jobs;

#endif defined(__cplusplus)

#endif

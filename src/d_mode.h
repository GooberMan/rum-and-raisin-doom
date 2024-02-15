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
//   Functions and definitions relating to the game type and operational
//   mode.
//

#ifndef __D_MODE__
#define __D_MODE__

#include "doomtype.h"

// The "mission" controls what game we are playing.

DOOM_C_API typedef enum
{
	doom,				// Doom 1
	doom2,				// Doom 2
	pack_tnt,			// Final Doom: TNT: Evilution
	pack_plut,			// Final Doom: The Plutonia Experiment
	pack_chex,			// Chex Quest (modded doom)
	pack_hacx,			// Hacx (modded doom2)
	heretic,			// Heretic
	hexen,				// Hexen
	strife,				// Strife

	none
} GameMission_t;

// The "mode" allows more accurate specification of the game mode we are
// in: eg. shareware vs. registered.  So doom1.wad and doom.wad are the
// same mission, but a different mode.

DOOM_C_API typedef enum
{
    shareware,       // Doom/Heretic shareware
    registered,      // Doom/Heretic registered
    commercial,      // Doom II/Hexen
    retail,          // Ultimate Doom
    indetermined     // Unknown.
} GameMode_t;

// What version are we emulating?

DOOM_C_API typedef enum
{
	exe_invalid = -1,
    exe_doom_1_2,    // Doom 1.2: shareware and registered
    exe_doom_1_666,  // Doom 1.666: for shareware, registered and commercial
    exe_doom_1_7,    // Doom 1.7/1.7a: "
    exe_doom_1_8,    // Doom 1.8: "
    exe_doom_1_9,    // Doom 1.9: "
    exe_hacx,        // Hacx
    exe_ultimate,    // Ultimate Doom (retail)
    exe_final,       // Final Doom
    exe_final2,      // Final Doom (alternate exe)
    exe_chex,        // Chex Quest executable (based on Final Doom)

	// Rum and Raisin additions
	exe_limit_removing,
	exe_limit_removing_fixed,
	exe_boom_2_02,
	exe_complevel9,
	exe_mbf,
	exe_mbf21,
	exe_mbf21_extended, // DEHACKED extensions

    exe_heretic_1_3, // Heretic 1.3

    exe_hexen_1_1,   // Hexen 1.1
    exe_strife_1_2,  // Strife v1.2
    exe_strife_1_31  // Strife v1.31
} GameVersion_t;

// What IWAD variant are we using?

DOOM_C_API typedef enum
{
    vanilla,    // Vanilla Doom
    freedoom,   // FreeDoom: Phase 1 + 2
    freedm,     // FreeDM
    bfgedition, // Doom Classic (Doom 3: BFG Edition)
	unityport,	// IWADs from the Doom unity port, including add-on WADs
} GameVariant_t;

// Skill level.

DOOM_C_API typedef enum
{
    sk_noitems = -1,        // the "-skill 0" hack
    sk_baby = 0,
    sk_easy,
    sk_medium,
    sk_hard,
    sk_nightmare
} skill_t;

DOOM_C_API doombool D_ValidGameMode(GameMission_t mission, GameMode_t mode);
DOOM_C_API doombool D_ValidGameVersion(GameMission_t mission, GameVersion_t version);
DOOM_C_API doombool D_ValidEpisodeMap(GameMission_t mission, GameMode_t mode,
                          int episode, int map);
DOOM_C_API int D_GetNumEpisodes(GameMission_t mission, GameMode_t mode);
DOOM_C_API doombool D_IsEpisodeMap(GameMission_t mission);
DOOM_C_API const char *D_GameMissionString(GameMission_t mission);
DOOM_C_API const char *D_GameModeString(GameMode_t mode);

#endif /* #ifndef __D_MODE__ */


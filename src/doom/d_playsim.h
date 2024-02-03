//
// Copyright(C) 2020-2022 Ethan Watson
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
// DESCRIPTION: Defines the gameflow types for Doom, Doom 2, TNT
//              and Plutonia.
//

#ifndef __D_PLAYSIM_H__
#define __D_PLAYSIM_H__

#include "doomtype.h"
#include "d_gameflow.h"

DOOM_C_API typedef struct attractoptions_s
{
	doombool					demo4;
} attractoptions_t;

DOOM_C_API typedef struct dehackedoptions_s
{
	doombool					frame_966;
	doombool					lump_from_iwads;
	doombool					always_load_lump;
	doombool					allow_bex;
	doombool					allow_mbf;
	doombool					allow_mbf21;
	doombool					allow_dsdhacked;
} dehackedoptions_t;

DOOM_C_API typedef struct demoopitions_s
{
	doombool					large_size;
	doombool					longtics;
} demooptions_t;

DOOM_C_API typedef struct finaleoptions_s
{
	doombool					always_allow_text_skip;
	doombool					cast_allows_mouse_presses;
} finaleoptions_t;

DOOM_C_API typedef struct gameoptions_s
{
	doombool					fix_divlineside;						// Checks against X when it should check against Y
	doombool					fix_findnexthighestfloor;				// Uses the worst algorithm known to humankind to do a minmax
	doombool					fix_unusable_onesided_teleports;		// Vanilla would wipe W1 specials at all times, even if things like teleports didn't trigger
	doombool					fix_same_sky_texture;					// Notorious bug where sky doesn't change between Doom II episodes
	doombool					fix_blockthingsiterator;				// Iterating all items of a linked list famously doesn't work if the list changes
	doombool					fix_intercepts_overflow;				// Stomps over the end of a statically-sized array
	doombool					fix_donut_backsector;					// Donut don't give no hoots about invalid back sectors
	doombool					fix_sky_wall_projectiles;				// The sky checking code when shooting projectiles would cause them to just disappear in to normal walls
	doombool					fix_bad_secret_exit_loop;				// Secret exit on any other map than the hardcoded maps causes the current level to loop

	doombool					allow_weapon_recoil;					// Boom "feature"
	doombool					allow_line_passthrough;					// Boom feature, don't stop using lines when one succeeds if linedef flag is set
	doombool					allow_hud_combined_keys;				// Boom feature, dependent on if resources exist in WAD
	doombool					allow_unlimited_scrollers;				// Vanilla limit
	doombool					allow_unlimited_platforms;				// Vanilla limit
	doombool					allow_unlimited_ceilings;				// Vanilla limit
	doombool					allow_generic_specials_handling;		// Rewritten specials, based on Boom standards but considered limit removing
	doombool					allow_separate_floor_ceiling_lights;	// Boom sector specials are allowed to work independently of each other
	doombool					allow_boom_line_specials;				// Comes with Boom fixes for specials by default
	doombool					allow_boom_sector_specials;				// Comes with Boom fixes for specials by default
	doombool					allow_mbf_line_sky_specials;			// complevel 9 is boom + extras
	doombool					allow_mbf_line_specials;				// Sky transfers, that's it
	doombool					allow_mbf_thing_mobj_flags;				// Bouncy! Friendly! TOUCHY!
	doombool					allow_mbf_code_pointers;				// Dehacked additions
	doombool					allow_mbf21_line_specials;				// New texture scrollers, new flags
	doombool					allow_mbf21_sector_specials;			// Insta-deaths
	doombool					allow_mbf21_thing_flags;				// flags2 field
	doombool					allow_mbf21_code_pointers;				// Dehacked additions

	doombool					reset_player_visited_secret;			// Start a new game, still thinks you've visited secret levels on intermission screen
	doombool					noclip_cheats_work_everywhere;			// idspispopd and idclip everywhere
	doombool					pre_ultimate_bossdeath_support;
	doombool					bfg_map02_secret_exit_to_map33;			// Find that one unmarked line in MAP02 in BFG edition
} gameoptions_t;

DOOM_C_API typedef struct setupoptions_s
{
	doombool					correct_opl3_by_default;
} setupoptions_t;

DOOM_C_API typedef struct wadoptions_s
{
	doombool					allow_mapinfo_lumps;
	doombool					extended_map_datatypes;
	doombool					allow_unlimited_lumps;
} wadoptions_t;

DOOM_C_API typedef struct savegameoptions_s
{
	doombool					large_size;
	doombool					extended_data;
} savegameoptions_t;

DOOM_C_API typedef struct renderoptions_s
{
	doombool					unlimited_solidsegs;
	doombool					no_medusa;
	doombool					texture_any_height;
	doombool					texture_any_surface;
	doombool					texture_blank_name;
	doombool					widescreen_assets;
	doombool					full_rgb_range;
	doombool					invalid_thing_frames;
	doombool					allow_transmaps;
} renderoptions_t;

DOOM_C_API typedef struct playsimoptions_s
{
	attractoptions_t			attract;
	dehackedoptions_t			dehacked;
	demooptions_t				demo;
	finaleoptions_t				finale;
	gameoptions_t				game;
	setupoptions_t				setup;
	savegameoptions_t			savegame;
	wadoptions_t				wad;
} playsimoptions_t;

DOOM_C_API extern playsimoptions_t		playsim;
DOOM_C_API extern renderoptions_t		render;

DOOM_C_API extern gameflow_t			doom_shareware;
DOOM_C_API extern gameflow_t			doom_registered;
DOOM_C_API extern gameflow_t			doom_registered_pre_v1_9;
DOOM_C_API extern gameflow_t			doom_ultimate;
DOOM_C_API extern gameflow_t			doom_2;
DOOM_C_API extern gameflow_t			doom_2_bfg;
DOOM_C_API extern gameflow_t			doom_tnt;
DOOM_C_API extern gameflow_t			doom_plutonia;
DOOM_C_API extern gameflow_t			doom_chex;
DOOM_C_API extern gameflow_t			doom_hacx;

// This is designed to analyse the IWAD and register the base gameflow.
// It also sets up the current playsim and render options.
DOOM_C_API void D_RegisterPlaysim();

#endif // __D_PLAYSIM_H__

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
	doombool					unlimited_scrollers;
	doombool					reset_player_visited_secret;
	doombool					doom2_bad_secret_exit_doesnt_loop;
	doombool					noclip_cheats_work_everywhere;
	doombool					pre_ultimate_bossdeath_support;
	doombool					bfg_map02_secret_exit_to_map33;
	doombool					hud_shows_boom_combined_keys;
	doombool					allow_sky_change_between_levels;
	doombool					allow_boom_specials;
	doombool					allow_mbf_specials;
	doombool					allow_mbf21_specials;
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
	doombool					always_update_sky_texture;
	doombool					allow_transmaps;
} renderoptions_t;

DOOM_C_API typedef struct playsimoptions_s
{
	attractoptions_t		attract;
	dehackedoptions_t		dehacked;
	demooptions_t			demo;
	finaleoptions_t			finale;
	gameoptions_t			game;
	setupoptions_t			setup;
	savegameoptions_t		savegame;
	wadoptions_t			wad;
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

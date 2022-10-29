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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct attractoptions_s
{
	boolean					demo4;
} attractoptions_t;

typedef struct dehackedoptions_s
{
	boolean					frame_966;
	boolean					lump_from_iwads;
} dehackedoptions_t;

typedef struct demoopitions_s
{
	boolean					large_size;
	boolean					longtics;
} demooptions_t;

typedef struct gameoptions_s
{
	boolean					unlimited_scrollers;
} gameoptions_t;

typedef struct wadoptions_s
{
	boolean					extended_map_datatypes;
	boolean					allow_unlimited_lumps;
} wadoptions_t;

typedef struct savegameoptions_s
{
	boolean					large_size;
	boolean					extended_data;
} savegameoptions_t;

typedef struct renderoptions_s
{
	boolean					unlimited_solidsegs;
	boolean					no_medusa;
	boolean					texture_any_height;
	boolean					texture_any_surface;
	boolean					texture_blank_name;
	boolean					widescreen_assets;
	boolean					full_rgb_range;
	boolean					invalid_thing_frames;
} renderoptions_t;

typedef struct playsimoptions_s
{
	attractoptions_t		attract;
	dehackedoptions_t		dehacked;
	demooptions_t			demo;
	gameoptions_t			game;
	savegameoptions_t		savegame;
	wadoptions_t			wad;
} playsimoptions_t;

extern playsimoptions_t		playsim;
extern renderoptions_t		render;

extern gameflow_t			doom_shareware;
extern gameflow_t			doom_registered;
extern gameflow_t			doom_ultimate;
extern gameflow_t			doom_2;
extern gameflow_t			doom_tnt;
extern gameflow_t			doom_plutonia;
extern gameflow_t			doom_chex;
extern gameflow_t			doom_hacx;

// This is designed to analyse the IWAD and register the base gameflow.
// It also sets up the current playsim and render options.
void D_RegisterPlaysim();

#ifdef __cplusplus
}
#endif

#endif // __D_PLAYSIM_H__

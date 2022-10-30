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
// DESCRIPTION: Structures that define the gameflow, breaking
//              away from vanilla hardcoded limits. Various port
//              mapinfo structures get mapped in to these; and
//              defaults for all supported games are defined.
//


#ifndef __D_GAMEFLOW_H__
#define __D_GAMEFLOW_H__

#include "doomtype.h"

DOOM_C_API typedef enum endgametype_e
{
	EndGame_None,
	EndGame_Pic				= 0x0001,
	EndGame_Bunny			= 0x0002,
	EndGame_Cast			= 0x0003,

	EndGame_Ultimate		= 0x1000,		// Combined with EndGame_Pic, chooses between primary or secondary if it's Ultimate Doom
	EndGame_SkipInterlevel	= 0x2000,		// Standard Doom 1 behavior on endgame, can be ignored with an option
} endgametype_t;

DOOM_C_API typedef enum interleveltype_e
{
	Interlevel_None,
	Interlevel_Static,
	Interlevel_Animated,
} interleveltype_t;

DOOM_C_API typedef enum frametype_s
{
	Frame_None,
	Frame_Infinite,
	Frame_FixedDuration,
	Frame_RandomDuration,
} frametype_t;

DOOM_C_API typedef enum animcondition_s
{
	AnimCondition_None,
	AnimCondition_MapNumGreater,
	AnimCondition_MapNumEqual,
	AnimCondition_FitsInFrame,
} animcondition_t;

DOOM_C_API typedef enum lumpnameflags_s
{
	Lumpname_None				= 0x00,
	Lumpname_Dehacked			= 0x01,
	Lumpname_RuntimeGenerated	= 0x02,
} lumpnameflags_t;

DOOM_C_API typedef struct mapinfo_s mapinfo_t;
DOOM_C_API typedef struct episodeinfo_s episodeinfo_t;

DOOM_C_API typedef struct bossaction_s
{
	int32_t					thing_type;
	int32_t					line_special;
	int32_t					tag;
} bossaction_t;

DOOM_C_API typedef struct intermission_s
{
	const char*				text;
	const char*				music_lump;
	const char*				background_lump;
} intermission_t;

DOOM_C_API typedef struct interlevelframe_s
{
	const char*				image_lump;
	frametype_t				type;
	int32_t					duration;
	lumpnameflags_t			lumpname_flags;
	int32_t					lumpname_animindex;
	int32_t					lumpname_animframe;
} interlevelframe_t;

DOOM_C_API typedef struct interlevelcond_s
{
	animcondition_t			condition;
	int32_t					param;
} interlevelcond_t;

DOOM_C_API typedef struct interlevelanim_s
{
	interlevelframe_t*		frames;
	int32_t					num_frames;
	int32_t					x_pos;
	int32_t					y_pos;
	interlevelcond_t*		conditions;
	int32_t					num_conditions;
} interlevelanim_t;

DOOM_C_API typedef struct interlevel_s
{
	interleveltype_t		type;

	const char*				background_lump;

	interlevelanim_t*		background_anims;
	int32_t					num_background_anims;

	interlevelanim_t*		foreground_anims;
	int32_t					num_foreground_anims;

} interlevel_t;

DOOM_C_API typedef struct endgame_s
{
	endgametype_t			type;
	intermission_t*			intermission;
	const char*				primary_image_lump;
	const char*				secondary_image_lump;
} endgame_t;

DOOM_C_API typedef struct mapinfo_s
{
	const char*				data_lump;
	const char*				name;
	const char*				name_patch_lump;
	const char*				authors;

	episodeinfo_t*			episode;
	int32_t					map_num;

	const char*				music_lump;
	const char*				sky_texture;
	int32_t					sky_scroll_speed;
	int32_t					par_time;

	bossaction_t*			boss_actions;
	int32_t					num_boss_actions;

	interlevel_t*			interlevel_finished;
	interlevel_t*			interlevel_entering;

	mapinfo_t*				next_map;
	intermission_t*			next_map_intermission;
	mapinfo_t*				secret_map;
	intermission_t*			secret_map_intermission;

	endgame_t*				endgame;
} mapinfo_t;

DOOM_C_API typedef struct episodeinfo_s
{
	const char*				name;
	const char*				name_patch_lump;

	mapinfo_t*				first_map;
	int32_t					episode_num;
} episodeinfo_t;

// Still working out the details, but playsim_base can equal:
// * vanilla
// * limitremoving
// * boom
// * mbf21
//
// Options will define subversion, for example
// * vanilla
//   * version=1.9
//   * version=final
// * boom
//   * version=2.02

DOOM_C_API typedef struct gameflow_s
{
	const char*				name;
	episodeinfo_t**			episodes;
	int32_t					num_episodes;

	const char*				playsim_base;
	const char*				playsim_options;
} gameflow_t;

DOOM_C_API extern gameflow_t*			current_game;
DOOM_C_API extern episodeinfo_t*		current_episode;
DOOM_C_API extern mapinfo_t*			current_map;

#endif // __D_GAMEFLOW_H__

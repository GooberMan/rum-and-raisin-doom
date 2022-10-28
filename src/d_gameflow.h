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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum endgametype_e
{
	EndGame_Pic,
	EndGame_Bunny,
	EndGame_Cast
} endgametype_t;

typedef enum interleveltype_e
{
	Interlevel_Animated,
	Interlevel_Skip,
} interleveltype_t;

typedef struct mapinfo_s mapinfo_t;
typedef struct episodeinfo_s episodeinfo_t;

typedef struct bossaction_s
{
	int32_t				thing_type;
	int32_t				line_special;
	int32_t				tag;
} bossaction_t;

typedef struct intermission_s
{
	const char*			text;
	const char*			music_lump;
	const char*			background_lump;
} intermission_t;

typedef struct interanim_s
{
	const char**		image_lumps;
	int32_t				num_image_lumps;
	int32_t				x_pos;
	int32_t				y_pos;
	int32_t				duration;
};

typedef struct interlevel_s
{
	const char*			background_lump;

	interanim_s*		background_anims;
	int32_t				num_background_anims;

	interanim_s*		foreground_anims;
	int32_t				num_foreground_anims;
} interlevel_t;

typedef struct mapinfo_s
{
	const char*			name;
	episodeinfo_t*		episode;
	int32_t				map_num;

	const char*			name_patch_lump;
	const char*			music_lump;
	const char*			sky_texture;
	int32_t				sky_scroll_speed;
	int32_t				par_time;

	bossaction_t*		boss_actions;
	int32_t				num_boss_actions;

	interleveltype_t	interlevel_type;

	interlevel_t*		interlevel_finished;
	interlevel_t*		interlevel_entering;

	mapinfo_t*			next_map;
	intermission_t*		next_map_intermission;
	mapinfo_t*			secret_map;
	intermission_t*		secret_map_intermission;

	endgametype_t		endgame_type;

} mapinfo_t;

typedef struct episodeinfo_s
{
	const char*			name;
	const char*			name_patch_lump;
	mapinfo_t*			first_map;
	int32_t				episode_num;
} episodeinfo_t;

#ifdef __cplusplus
}
#endif

#endif // __D_GAMEFLOW_H__

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
#include "deh_str.h"

DOOM_C_API typedef enum intermissiontype_e
{
	Intermission_None,
	Intermission_Skippable,
} intermissiontype_t;

DOOM_C_API typedef enum endgametype_e
{
	EndGame_None,
	EndGame_Pic					= 0x0001,
	EndGame_Bunny				= 0x0002,
	EndGame_Cast				= 0x0004,

	EndGame_LoopingMusic		= 0x0100,
	EndGame_StraightToVictory	= 0x0200,
	EndGame_DoNextMap			= 0x0400,

	EndGame_Ultimate			= 0x1000,		// Combined with EndGame_Pic, chooses between primary or secondary if it's Ultimate Doom
	EndGame_BuiltInCast			= 0x2000,		// Original DooM II cast roll call
	EndGame_NoEndOnBunny		= 0x4000,

	EndGame_AnyArtScreen		= EndGame_Pic | EndGame_Bunny
} endgametype_t;

DOOM_C_API typedef enum frametype_s
{
	Frame_None						= 0x0000,
	Frame_Infinite					= 0x0001,
	Frame_FixedDuration				= 0x0002,
	Frame_RandomDuration			= 0x0004,

	Frame_RandomStart				= 0x1000,

	Frame_Valid						= 0x100F,

	// Non-standard flags
	Frame_AdjustForWidescreen		= 0x8000000,

	Frame_Background				= Frame_Infinite | Frame_AdjustForWidescreen
} frametype_t;

DOOM_C_API typedef enum animcondition_s
{
	AnimCondition_None,
	AnimCondition_MapNumGreater,	// Checks: Current/next map number.
									// Parameter: map number

	AnimCondition_MapNumEqual,		// Checks: Current/next map number.
									// Parameter: map number

	AnimCondition_MapVisited,		// Checks: Visited flag for map number.
									// Parameter: map number

	AnimCondition_MapNotSecret,		// Checks: Current/next map.
									// Parameter: none

	AnimCondition_SecretVisited,	// Checks: Any secret map visited.
									// Parameter: none

	AnimCondition_IsExiting,		// Checks: Victory screen type
									// Parameter: none

	AnimCondition_IsEntering,		// Checks: Victory screen type
									// Parameter: none

	AnimCondition_Max,

	// Here be dragons. Non-standard conditions.
	AnimCondition_FitsInFrame,		// Checks: Patch dimensions.
									// Parameter: group number, allowing only one condition of this type to succeed per group

} animcondition_t;

DOOM_C_API typedef enum flowstringflags_s
{
	FlowString_None					= 0x00,
	FlowString_Dehacked				= 0x01,
	FlowString_RuntimeGenerated		= 0x02,
	FlowString_IgnoreEpisodeParam	= 0x04,
} flowstringflags_t;

DOOM_C_API typedef enum mapflags_s
{
	Map_None						= 0x0000,
	Map_Secret						= 0x0001,
	Map_EndOfEpisode				= 0x0002,
	Map_NoInterlevel				= 0x0004,
	Map_NoParTime					= 0x0008,
	Map_NoEnterBanner				= 0x0010,
	
	Map_MonstersTelefrag			= 0x1000,

	Map_Doom1EndOfEpisode			= Map_EndOfEpisode | Map_NoInterlevel,
	Map_Doom2EndOfGame				= Map_EndOfEpisode | Map_NoEnterBanner | Map_MonstersTelefrag,
	Map_GenericEndOfGame			= Map_EndOfEpisode | Map_NoEnterBanner,
} mapflags_t;

DOOM_C_API typedef struct flowstring_s
{
	const char*				val;
	flowstringflags_t		flags;

#if defined( __cplusplus )
	INLINE const char*		Resolve() const { return ( flags & FlowString_Dehacked ) ? DEH_String( val ) : val; };
#endif //defined( __cplusplus )
} flowstring_t;

DOOM_C_API typedef struct mapinfo_s mapinfo_t;
DOOM_C_API typedef struct episodeinfo_s episodeinfo_t;

DOOM_C_API typedef struct bossaction_s
{
	int32_t					thing_type;
	int32_t					mbf21_flag_type;
	int32_t					line_special;
	int32_t					tag;
} bossaction_t;

DOOM_C_API typedef struct intermission_s
{
	intermissiontype_t		type;

	flowstring_t			text;
	flowstring_t			music_lump;
	flowstring_t			background_lump;
} intermission_t;

DOOM_C_API typedef struct interlevelframe_s
{
	flowstring_t			image_lump;
	frametype_t				type;
	int32_t					duration;
	int32_t					maxduration;
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

DOOM_C_API typedef struct interlevellayer_s
{
	interlevelanim_t*		anims;
	int32_t					num_anims;
	interlevelcond_t*		conditions;
	int32_t					num_conditions;
} interlevellayer_t;

DOOM_C_API typedef struct interlevel_s
{
	flowstring_t			music_lump;

	flowstring_t			background_lump;

	interlevellayer_t*		anim_layers;
	int32_t					num_anim_layers;
} interlevel_t;

DOOM_C_API typedef struct endgame_castframe_s
{
	flowstring_t			lump;
	flowstring_t			tranmap;
	flowstring_t			translation;
	doombool				flipped;
	int32_t					duration;
	int32_t					sound;
} endgame_castframe_t;

DOOM_C_API typedef struct endgame_castmember_s
{
	flowstring_t			name_mnemonic;
	endgame_castframe_t*	alive_frames;
	endgame_castframe_t*	death_frames;
	int32_t					num_alive_frames;
	int32_t					num_death_frames;
} endgame_castmember_t;

DOOM_C_API typedef struct endgame_s
{
	endgametype_t			type;
	intermission_t*			intermission;
	flowstring_t			primary_image_lump;
	flowstring_t			secondary_image_lump;
	flowstring_t			music_lump;
	endgame_castmember_t*	cast_members;
	int32_t					num_cast_members;
	flowstring_t			bunny_end_overlay;
	int32_t					bunny_end_count;
	int32_t					bunny_end_x;
	int32_t					bunny_end_y;
	int32_t					bunny_end_sound;
} endgame_t;

DOOM_C_API typedef struct mapinfo_s
{
	flowstring_t			data_lump;
	flowstring_t			name;
	flowstring_t			name_patch_lump;
	flowstring_t			authors;

	episodeinfo_t*			episode;
	int32_t					map_num;
	mapflags_t				map_flags;

	flowstring_t			music_lump[ 65 ];
	flowstring_t			sky_texture;
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
	flowstring_t			name;
	flowstring_t			name_patch_lump;

	int32_t					episode_num;

	mapinfo_t**				all_maps;
	int32_t					num_maps;
	int32_t					highest_map_num;

	mapinfo_t*				first_map;
} episodeinfo_t;

DOOM_C_API typedef struct gameflow_s
{
	flowstring_t			name;
	episodeinfo_t**			episodes;
	int32_t					num_episodes;
} gameflow_t;

DOOM_C_API extern gameflow_t*			current_game;
DOOM_C_API extern episodeinfo_t*		current_episode;
DOOM_C_API extern mapinfo_t*			current_map;

DOOM_C_API episodeinfo_t* D_GameflowGetEpisode( int32_t episodenum );
DOOM_C_API mapinfo_t* D_GameflowGetMap( episodeinfo_t* episode, int32_t mapnum );

DOOM_C_API void D_GameflowSetCurrentEpisode( episodeinfo_t* episode );
DOOM_C_API void D_GameflowSetCurrentMap( mapinfo_t* map );

DOOM_C_API endgame_t* D_GameflowGetFinaleCopy( const char* lumpname );
DOOM_C_API interlevel_t* D_GameflowGetInterlevel( const char* lumpname );

DOOM_C_API void D_GameflowCheckAndParseMapinfos( void );

#if defined( __cplusplus )

#include "m_container.h"
#include "m_misc.h"
#include "deh_str.h"

INLINE auto D_GetEpisodes()
{
	return std::span( current_game->episodes, current_game->num_episodes );
}

INLINE auto D_GetMapsFor( episodeinfo_t* episode )
{
	return std::span( episode->all_maps, episode->num_maps );
}

template< typename _strtype, typename _param1, typename... _params >
INLINE _strtype AsString( const flowstring_t& str, _param1 p1, _params... params )
{
	const char* base = str.Resolve();
	if( str.flags & FlowString_RuntimeGenerated )
	{
		if( str.flags & FlowString_IgnoreEpisodeParam )
		{
			size_t len = M_snprintf( nullptr, 0, base, params... );
			_strtype formatted( len, '\0' );
			M_snprintf( formatted.data(), len + 1, base, params... );
			return formatted;
		}
		size_t len = M_snprintf( nullptr, 0, base, p1, params... );
		_strtype formatted( len, '\0' );
		M_snprintf( formatted.data(), len + 1, base, p1, params... );
		return formatted;
	}
	return _strtype( base ? base : "" );
}

template< typename _param1, typename... _params >
INLINE DoomString AsDoomString( const flowstring_t& str, _param1 p1, _params... params )
{
	return AsString< DoomString >( str, p1, params... );
}

template< typename _param1, typename... _params >
INLINE std::string AsStdString( const flowstring_t& str, _param1 p1, _params... params )
{
	return AsString< std::string >( str, p1, params... );
}


#endif // defined( __cplusplus )

#endif // __D_GAMEFLOW_H__

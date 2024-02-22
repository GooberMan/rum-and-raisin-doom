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
// DESCRIPTION: Defines the gameflow types for Doom 2/commercial
//              game types

#include "d_gamesim.h"
#include "dstrings.h"

#include "d_gameflow.h"

#include "p_mobj.h"

// Forward declarations
extern episodeinfo_t	hacx_episode;

extern mapinfo_t		hacx_map_map01;
extern mapinfo_t		hacx_map_map02;
extern mapinfo_t		hacx_map_map03;
extern mapinfo_t		hacx_map_map04;
extern mapinfo_t		hacx_map_map05;
extern mapinfo_t		hacx_map_map06;
extern mapinfo_t		hacx_map_map07;
extern mapinfo_t		hacx_map_map08;
extern mapinfo_t		hacx_map_map09;
extern mapinfo_t		hacx_map_map10;
extern mapinfo_t		hacx_map_map11;
extern mapinfo_t		hacx_map_map12;
extern mapinfo_t		hacx_map_map13;
extern mapinfo_t		hacx_map_map14;
extern mapinfo_t		hacx_map_map15;
extern mapinfo_t		hacx_map_map16;
extern mapinfo_t		hacx_map_map17;
extern mapinfo_t		hacx_map_map18;
extern mapinfo_t		hacx_map_map19;
extern mapinfo_t		hacx_map_map20;
extern mapinfo_t		hacx_map_map31;

extern interlevel_t		hacx_interlevel;

extern intermission_t	hacx_intermission_e1;
extern intermission_t	hacx_intermission_e2;
extern intermission_t	hacx_intermission_e3;
extern intermission_t	hacx_intermission_s1;

extern endgame_t		hacx_endgame;

#define levename_format_text "CWILV%2.2d"
#define levellump0_format_text "map0%d"
#define levellumpn_format_text "map%d"

// Helpers

// This needs to live in a header somewhere... and I think we need to 100% opt-in to it, at least while we mix C and C++ code everywhere

// You can use std::enable_if in earlier C++ versions if required...
template< typename _type >
requires std::is_enum_v< _type >
constexpr auto operator|( _type lhs, _type rhs )
{
	using underlying = std::underlying_type_t< _type >;
	return (_type)( (underlying)lhs | (underlying)rhs );
}

constexpr auto EmptyFlowString()
{
	return flowstring_t();
}

constexpr auto FlowString( const char* name, flowstringflags_t additional = FlowString_None )
{
	flowstring_t lump = { name, FlowString_Dehacked | additional };
	return lump;
}

constexpr auto LevelFlowString( const char* name )
{
	flowstring_t lump = { name, FlowString_Dehacked | FlowString_RuntimeGenerated | FlowString_IgnoreEpisodeParam };
	return lump;
}

constexpr auto RuntimeFlowString( const char* name )
{
	flowstring_t lump = { name, FlowString_Dehacked | FlowString_RuntimeGenerated };
	return lump;
}

constexpr auto PlainFlowString( const char* name )
{
	flowstring_t lump = { name, FlowString_None };
	return lump;
}

// Now for the full declarations

//============================================================================
// Game setup (episodes etc)
//============================================================================

mapinfo_t* hacx_maps[] =
{
	&hacx_map_map01, &hacx_map_map02, &hacx_map_map03, &hacx_map_map04,
	&hacx_map_map05, &hacx_map_map06, &hacx_map_map07, &hacx_map_map08,
	&hacx_map_map09, &hacx_map_map10, &hacx_map_map11, &hacx_map_map12,
	&hacx_map_map13, &hacx_map_map14, &hacx_map_map15, &hacx_map_map16,
	&hacx_map_map17, &hacx_map_map18, &hacx_map_map19, &hacx_map_map20,
	&hacx_map_map31,
};

episodeinfo_t hacx_episode =
{
	PlainFlowString( "HacX" ),					// name
	EmptyFlowString(),							// name_patch_lump
	1,											// episode_num
	hacx_maps,									// all_maps
	arrlen( hacx_maps ),						// num_maps
	hacx_map_map31.map_num,						// highest_map_num
	&hacx_map_map01								// first_map
};

episodeinfo_t* hacx_episodes[] =
{
	&hacx_episode
};

gameflow_t doom_hacx =
{
	PlainFlowString( "HACX: Twitch 'n Kill" ),		// name
	hacx_episodes,									// episodes
	arrlen( hacx_episodes ),						// num_episodes
	PlainFlowString( "vanilla" ),					// playsim_base
	PlainFlowString( "" )							// playsim_options
};

//============================================================================
// Maps
//============================================================================

//============================================================================
// Maps 1-11
//============================================================================

mapinfo_t hacx_map_map01 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( HUSTR_1 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Herndon, Michael Reed" ),	// authors
	&hacx_episode,									// episode
	1,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "runnin" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	30,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map02,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map01,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map02 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( HUSTR_2 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Stephen Watson" ),			// authors
	&hacx_episode,									// episode
	2,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "stalks" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map03,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map02,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map03 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( HUSTR_3 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Marc Pullen" ),				// authors
	&hacx_episode,									// episode
	3,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "countd" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map04,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map03,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map04 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( HUSTR_4 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Herndon" ),				// authors
	&hacx_episode,									// episode
	4,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "betwee" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map05,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map04,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map05 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( HUSTR_5 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Iikka Ker�nen" ),				// authors
	&hacx_episode,									// episode
	5,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "doom" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map06,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map05,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map06 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( HUSTR_6 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Stephen Watson" ),			// authors
	&hacx_episode,									// episode
	6,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "the_da" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map07,								// next_map
	&hacx_intermission_e1,							// next_map_intermission
	&hacx_map_map06,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

static bossaction_t		hacx_map07special[] =
{
	{ 8 /*MT_FATSO*/, MF2_MBF21_MAP07BOSS1, 82 /*lowerFloorToLowest repeatable*/, 666 },
	{ 20 /*MT_BABY*/, MF2_MBF21_MAP07BOSS2, 96 /*raiseToTexture repeatable*/, 667 },
};

mapinfo_t hacx_map_map07 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( HUSTR_7 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Adam Williamson" ),			// authors
	&hacx_episode,									// episode
	7,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "shawn" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	hacx_map07special,								// boss_actions
	arrlen( hacx_map07special ),					// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map08,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map07,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map08 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( HUSTR_8 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Ryan Rapsys" ),				// authors
	&hacx_episode,									// episode
	8,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "ddtblu" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map09,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map08,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map09 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( HUSTR_9 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( " Anthony Czerwonka" ),		// authors
	&hacx_episode,									// episode
	9,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "in_cit" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	270,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map10,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map09,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map10 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_10 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Ryan Rapsys" ),				// authors
	&hacx_episode,									// episode
	10,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "dead" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map11,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map10,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map11 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_11 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Michal Mesko" ),				// authors
	&hacx_episode,									// episode
	11,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "stlks2" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	210,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map12,								// next_map
	&hacx_intermission_e2,							// next_map_intermission
	&hacx_map_map11,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

//============================================================================
// Maps 12-20
//============================================================================

mapinfo_t hacx_map_map12 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_12 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Rich Johnston" ),				// authors
	&hacx_episode,									// episode
	12,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "theda2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map13,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map12,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map13 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_13 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Anthony Czerwonka" ),			// authors
	&hacx_episode,									// episode
	13,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "doom2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map14,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map13,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map14 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_14 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Andrew Gate" ),				// authors
	&hacx_episode,									// episode
	14,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "ddtbl2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map15,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map14,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map15 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_15 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Jeremy Statz" ),				// authors
	&hacx_episode,									// episode
	15,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "runni2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	210,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map16,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map31,								// secret_map
	&hacx_intermission_s1,							// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map16 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_16 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Marc Pullen" ),				// authors
	&hacx_episode,									// episode
	16,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "dead2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map17,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map16,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map17 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_17 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Marc Pullen" ),				// authors
	&hacx_episode,									// episode
	17,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "stlks3" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	420,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map18,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map17,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map18 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_18 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Anthony Czerwonka" ),			// authors
	&hacx_episode,									// episode
	18,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "romero" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map19,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map18,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map19 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_19 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Adam Williamson" ),			// authors
	&hacx_episode,									// episode
	19,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "shawn2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	200,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map20,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map19,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t hacx_map_map20 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_20 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Herndon" ),				// authors
	&hacx_episode,									// episode
	20,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "messag" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	nullptr,										// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map20,								// secret_map
	nullptr,										// secret_map_intermission
	&hacx_endgame,									// endgame
};

//============================================================================
// Maps 31-35
//============================================================================

mapinfo_t hacx_map_map31 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( HUSTR_31 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Stephen Watson" ),			// authors
	&hacx_episode,									// episode
	31,												// map_num
	Map_Secret,										// map_flags
	RuntimeFlowString( "evil" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&hacx_interlevel,								// interlevel_finished
	&hacx_interlevel,								// interlevel_entering
	&hacx_map_map16,								// next_map
	nullptr,										// next_map_intermission
	&hacx_map_map31,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

//============================================================================
// Intermissions, endgames, and interlevels
//============================================================================

//============================================================================
// Hell On Earth
//============================================================================

intermission_t hacx_intermission_e1 =
{
	Intermission_Skippable,							// type
	FlowString( C1TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "SLIME16" ),						// background_lump
};

intermission_t hacx_intermission_e2 =
{
	Intermission_Skippable,							// type
	FlowString( C2TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK14" ),						// background_lump
};

intermission_t hacx_intermission_e3 =
{
	Intermission_Skippable,							// type
	FlowString( C3TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK07" ),						// background_lump
};

intermission_t hacx_intermission_s1 =
{
	Intermission_Skippable,							// type
	FlowString( C5TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK13" ),						// background_lump
};

endgame_t hacx_endgame =
{
	EndGame_Cast | EndGame_LoopingMusic,			// type
	&hacx_intermission_e3,							// intermission
	EmptyFlowString(),								// primary_image_lump
	EmptyFlowString(),								// secondary_image_lump
	RuntimeFlowString( "evil" ),					// music_lump
};

interlevel_t hacx_interlevel =
{
	Interlevel_Static,								// type
	RuntimeFlowString( "dm2int" ),					// music_lump
	FlowString( "INTERPIC" ),						// background_lump
	nullptr,										// background_anims
	0,												// num_background_anims
	nullptr,										// foreground_anims
	0,												// num_foreground_anims
};


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

#include "d_playsim.h"
#include "dstrings.h"

// Forward declarations
extern episodeinfo_t	doom2_episode_hellonearth;

extern mapinfo_t		doom2_map_map01;
extern mapinfo_t		doom2_map_map02;
extern mapinfo_t		doom2_map_map02_bfg;
extern mapinfo_t		doom2_map_map03;
extern mapinfo_t		doom2_map_map04;
extern mapinfo_t		doom2_map_map05;
extern mapinfo_t		doom2_map_map06;
extern mapinfo_t		doom2_map_map07;
extern mapinfo_t		doom2_map_map08;
extern mapinfo_t		doom2_map_map09;
extern mapinfo_t		doom2_map_map10;
extern mapinfo_t		doom2_map_map11;
extern mapinfo_t		doom2_map_map12;
extern mapinfo_t		doom2_map_map13;
extern mapinfo_t		doom2_map_map14;
extern mapinfo_t		doom2_map_map15;
extern mapinfo_t		doom2_map_map16;
extern mapinfo_t		doom2_map_map17;
extern mapinfo_t		doom2_map_map18;
extern mapinfo_t		doom2_map_map19;
extern mapinfo_t		doom2_map_map20;
extern mapinfo_t		doom2_map_map21;
extern mapinfo_t		doom2_map_map22;
extern mapinfo_t		doom2_map_map23;
extern mapinfo_t		doom2_map_map24;
extern mapinfo_t		doom2_map_map25;
extern mapinfo_t		doom2_map_map26;
extern mapinfo_t		doom2_map_map27;
extern mapinfo_t		doom2_map_map28;
extern mapinfo_t		doom2_map_map29;
extern mapinfo_t		doom2_map_map30;
extern mapinfo_t		doom2_map_map31;
extern mapinfo_t		doom2_map_map32;
extern mapinfo_t		doom2_map_map33;

extern interlevel_t		doom2_interlevel;

extern intermission_t	doom2_intermission_e1;
extern intermission_t	doom2_intermission_e2;
extern intermission_t	doom2_intermission_e3;
extern intermission_t	doom2_intermission_e4;
extern intermission_t	doom2_intermission_s1;
extern intermission_t	doom2_intermission_s2;

extern endgame_t		doom2_endgame;

#define levename_format_text "CWILV%2.2d"
#define levellump0_format_text "MAP0%d"
#define levellumpn_format_text "MAP%d"

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

mapinfo_t* doom2_maps_hellonearth[] =
{
	&doom2_map_map01, &doom2_map_map02, &doom2_map_map03, &doom2_map_map04,
	&doom2_map_map05, &doom2_map_map06, &doom2_map_map07, &doom2_map_map08,
	&doom2_map_map09, &doom2_map_map10, &doom2_map_map11, &doom2_map_map12,
	&doom2_map_map13, &doom2_map_map14, &doom2_map_map15, &doom2_map_map16,
	&doom2_map_map17, &doom2_map_map18, &doom2_map_map19, &doom2_map_map20,
	&doom2_map_map21, &doom2_map_map22, &doom2_map_map23, &doom2_map_map24,
	&doom2_map_map25, &doom2_map_map26, &doom2_map_map27, &doom2_map_map28,
	&doom2_map_map29, &doom2_map_map30, &doom2_map_map31, &doom2_map_map32,
};

episodeinfo_t doom2_episode_hellonearth =
{
	PlainFlowString( "Hell On Earth" ),				// name
	EmptyFlowString(),								// name_patch_lump
	1,												// episode_num
	doom2_maps_hellonearth,							// all_maps
	arrlen( doom2_maps_hellonearth ),				// num_maps
	doom2_map_map32.map_num,						// highest_map_num
	&doom2_map_map01								// first_map
};

episodeinfo_t* doom2_episodes[] =
{
	&doom2_episode_hellonearth
};

gameflow_t doom_2 =
{
	PlainFlowString( "DOOM 2: Hell on Earth" ),		// name
	doom2_episodes,									// episodes
	arrlen( doom2_episodes ),						// num_episodes
	PlainFlowString( "vanilla" ),					// playsim_base
	PlainFlowString( "" )							// playsim_options
};

mapinfo_t* doom2_maps_hellonearth_bfg[] =
{
	&doom2_map_map01, &doom2_map_map02_bfg, &doom2_map_map03, &doom2_map_map04,
	&doom2_map_map05, &doom2_map_map06, &doom2_map_map07, &doom2_map_map08,
	&doom2_map_map09, &doom2_map_map10, &doom2_map_map11, &doom2_map_map12,
	&doom2_map_map13, &doom2_map_map14, &doom2_map_map15, &doom2_map_map16,
	&doom2_map_map17, &doom2_map_map18, &doom2_map_map19, &doom2_map_map20,
	&doom2_map_map21, &doom2_map_map22, &doom2_map_map23, &doom2_map_map24,
	&doom2_map_map25, &doom2_map_map26, &doom2_map_map27, &doom2_map_map28,
	&doom2_map_map29, &doom2_map_map30, &doom2_map_map31, &doom2_map_map32,
	&doom2_map_map33,
};

episodeinfo_t doom2_episode_hellonearth_bfg =
{
	PlainFlowString( "Hell On Earth" ),				// name
	EmptyFlowString(),								// name_patch_lump
	1,												// episode_num
	doom2_maps_hellonearth_bfg,						// all_maps
	arrlen( doom2_maps_hellonearth_bfg ),			// num_maps
	doom2_map_map33.map_num,						// highest_map_num
	&doom2_map_map01								// first_map
};

episodeinfo_t* doom2_episodes_bfg[] =
{
	&doom2_episode_hellonearth_bfg
};

gameflow_t doom_2_bfg =
{
	PlainFlowString( "DOOM 2: Hell on Earth" ),		// name
	doom2_episodes_bfg,								// episodes
	arrlen( doom2_episodes_bfg ),					// num_episodes
	PlainFlowString( "vanilla" ),					// playsim_base
	PlainFlowString( "" )							// playsim_options
};

//============================================================================
// Maps
//============================================================================

//============================================================================
// Maps 1-11
//============================================================================

mapinfo_t doom2_map_map01 =
{
	RuntimeFlowString( levellump0_format_text ),	// data_lump
	FlowString( HUSTR_1 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	1,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "runnin" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	30,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map02,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map01,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map02 =
{
	RuntimeFlowString( levellump0_format_text ),	// data_lump
	FlowString( HUSTR_2 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "American McGee" ),			// authors
	&doom2_episode_hellonearth,						// episode
	2,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "stalks" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map03,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map02,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map02_bfg =
{
	RuntimeFlowString( levellump0_format_text ),	// data_lump
	FlowString( HUSTR_2 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "American McGee" ),			// authors
	&doom2_episode_hellonearth,						// episode
	2,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "stalks" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map03,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map33,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map03 =
{
	RuntimeFlowString( levellump0_format_text ),	// data_lump
	FlowString( HUSTR_3 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "American McGee" ),			// authors
	&doom2_episode_hellonearth,						// episode
	3,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "countd" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map04,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map03,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map04 =
{
	RuntimeFlowString( levellump0_format_text ),	// data_lump
	FlowString( HUSTR_4 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "American McGee" ),			// authors
	&doom2_episode_hellonearth,						// episode
	4,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "betwee" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map05,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map04,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map05 =
{
	RuntimeFlowString( levellump0_format_text ),	// data_lump
	FlowString( HUSTR_5 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "American McGee" ),			// authors
	&doom2_episode_hellonearth,						// episode
	5,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "doom" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map06,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map05,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map06 =
{
	RuntimeFlowString( levellump0_format_text ),	// data_lump
	FlowString( HUSTR_6 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "American McGee" ),			// authors
	&doom2_episode_hellonearth,						// episode
	6,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "the_da" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map07,								// next_map
	&doom2_intermission_e1,							// next_map_intermission
	&doom2_map_map06,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

static bossaction_t		doom2_map07special[] =
{
	{ 8 /*MT_FATSO*/, 82 /*lowerFloorToLowest repeatable*/, 666 },
	{ 20 /*MT_BABY*/, 96 /*raiseToTexture repeatable*/, 667 },
};

mapinfo_t doom2_map_map07 =
{
	RuntimeFlowString( levellump0_format_text ),	// data_lump
	FlowString( HUSTR_7 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "American McGee, Sandy Petersen" ),	// authors
	&doom2_episode_hellonearth,						// episode
	7,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "shawn" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	doom2_map07special,								// boss_actions
	arrlen( doom2_map07special ),					// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map08,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map07,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map08 =
{
	RuntimeFlowString( levellump0_format_text ),	// data_lump
	FlowString( HUSTR_8 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	8,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "ddtblu" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map09,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map08,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map09 =
{
	RuntimeFlowString( levellump0_format_text ),	// data_lump
	FlowString( HUSTR_9 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	9,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "in_cit" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	270,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map10,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map09,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map10 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_10 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Hall, Sandy Petersen" ),	// authors
	&doom2_episode_hellonearth,						// episode
	10,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "dead" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map11,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map10,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map11 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_11 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom2_episode_hellonearth,						// episode
	11,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "stlks2" ),					// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	210,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map12,								// next_map
	&doom2_intermission_e2,							// next_map_intermission
	&doom2_map_map11,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

//============================================================================
// Maps 12-20
//============================================================================

mapinfo_t doom2_map_map12 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_12 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	12,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "theda2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map13,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map12,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map13 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_13 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	13,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "doom2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map14,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map13,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map14 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_14 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "American McGee" ),			// authors
	&doom2_episode_hellonearth,						// episode
	14,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "ddtbl2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map15,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map14,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map15 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_15 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom2_episode_hellonearth,						// episode
	15,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "runni2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	210,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map16,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map31,								// secret_map
	&doom2_intermission_s1,							// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map16 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_16 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	16,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "dead2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map17,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map16,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map17 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_17 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom2_episode_hellonearth,						// episode
	17,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "stlks3" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	420,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map18,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map17,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map18 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_18 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	18,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "romero" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map19,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map18,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map19 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_19 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	19,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "shawn2" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	200,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map20,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map19,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map20 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_20 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom2_episode_hellonearth,						// episode
	20,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "messag" ),					// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map21,								// next_map
	&doom2_intermission_e3,							// next_map_intermission
	&doom2_map_map20,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

//============================================================================
// Maps 21-30
//============================================================================

mapinfo_t doom2_map_map21 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_21 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	21,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "count2" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	240,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map22,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map21,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map22 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_22 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "American McGee" ),			// authors
	&doom2_episode_hellonearth,						// episode
	22,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "ddtbl3" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map23,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map22,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map23 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_23 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	23,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "ampie" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	180,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map24,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map23,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map24 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_24 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	24,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "theda3" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map25,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map24,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map25 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_25 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Shawn Green" ),				// authors
	&doom2_episode_hellonearth,						// episode
	25,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "adrian" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map26,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map25,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map26 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_26 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom2_episode_hellonearth,						// episode
	26,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "messg2" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	300,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map27,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map26,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map27 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_27 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	27,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "romer2" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	330,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map28,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map27,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map28 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_28 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	28,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "tense" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	420,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map29,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map28,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map29 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_29 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom2_episode_hellonearth,						// episode
	29,												// map_num
	Map_None,										// map_flags
	RuntimeFlowString( "shawn3" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	300,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map30,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map29,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map30 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_30 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	30,												// map_num
	Map_Doom2EndOfGame,								// map_flags
	RuntimeFlowString( "openin" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	180,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	nullptr,										// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map30,								// secret_map
	nullptr,										// secret_map_intermission
	&doom2_endgame,									// endgame
};

//============================================================================
// Maps 31-35
//============================================================================

mapinfo_t doom2_map_map31 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_31 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	31,												// map_num
	Map_Secret,										// map_flags
	RuntimeFlowString( "evil" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map16,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map32,								// secret_map
	&doom2_intermission_s2,							// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map32 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( HUSTR_32 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom2_episode_hellonearth,						// episode
	32,												// map_num
	Map_Secret,										// map_flags
	RuntimeFlowString( "ultima" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	30,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map16,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map32,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom2_map_map33 =
{
	RuntimeFlowString( levellumpn_format_text ),	// data_lump
	FlowString( PHUSTR_1 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Michael Bukowski" ),			// authors
	&doom2_episode_hellonearth,						// episode
	33,												// map_num
	Map_Secret,										// map_flags
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	0,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom2_interlevel,								// interlevel_finished
	&doom2_interlevel,								// interlevel_entering
	&doom2_map_map03,								// next_map
	nullptr,										// next_map_intermission
	&doom2_map_map33,								// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

//============================================================================
// Intermissions, endgames, and interlevels
//============================================================================

//============================================================================
// Hell On Earth
//============================================================================

intermission_t doom2_intermission_e1 =
{
	Intermission_Skippable,							// type
	FlowString( C1TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "SLIME16" ),						// background_lump
};

intermission_t doom2_intermission_e2 =
{
	Intermission_Skippable,							// type
	FlowString( C2TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK14" ),						// background_lump
};

intermission_t doom2_intermission_e3 =
{
	Intermission_Skippable,							// type
	FlowString( C3TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK07" ),						// background_lump
};

intermission_t doom2_intermission_e4 =
{
	Intermission_Skippable,							// type
	FlowString( C4TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK17" ),						// background_lump
};

intermission_t doom2_intermission_s1 =
{
	Intermission_Skippable,							// type
	FlowString( C5TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK13" ),						// background_lump
};

intermission_t doom2_intermission_s2 =
{
	Intermission_Skippable,							// type
	FlowString( C6TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK19" ),						// background_lump
};

endgame_t doom2_endgame =
{
	EndGame_Cast | EndGame_LoopingMusic,			// type
	&doom2_intermission_e4,							// intermission
	EmptyFlowString(),								// primary_image_lump
	EmptyFlowString(),								// secondary_image_lump
	RuntimeFlowString( "evil" ),					// music_lump
};

interlevel_t doom2_interlevel =
{
	Interlevel_Static,								// type
	RuntimeFlowString( "dm2int" ),					// music_lump
	FlowString( "INTERPIC" ),						// background_lump
	nullptr,										// background_anims
	0,												// num_background_anims
	nullptr,										// foreground_anims
	0,												// num_foreground_anims
};


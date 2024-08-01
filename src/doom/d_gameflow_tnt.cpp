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
extern episodeinfo_t	tnt_episode;

extern mapinfo_t		tnt_map_map01;
extern mapinfo_t		tnt_map_map02;
extern mapinfo_t		tnt_map_map03;
extern mapinfo_t		tnt_map_map04;
extern mapinfo_t		tnt_map_map05;
extern mapinfo_t		tnt_map_map06;
extern mapinfo_t		tnt_map_map07;
extern mapinfo_t		tnt_map_map08;
extern mapinfo_t		tnt_map_map09;
extern mapinfo_t		tnt_map_map10;
extern mapinfo_t		tnt_map_map11;
extern mapinfo_t		tnt_map_map12;
extern mapinfo_t		tnt_map_map13;
extern mapinfo_t		tnt_map_map14;
extern mapinfo_t		tnt_map_map15;
extern mapinfo_t		tnt_map_map16;
extern mapinfo_t		tnt_map_map17;
extern mapinfo_t		tnt_map_map18;
extern mapinfo_t		tnt_map_map19;
extern mapinfo_t		tnt_map_map20;
extern mapinfo_t		tnt_map_map21;
extern mapinfo_t		tnt_map_map22;
extern mapinfo_t		tnt_map_map23;
extern mapinfo_t		tnt_map_map24;
extern mapinfo_t		tnt_map_map25;
extern mapinfo_t		tnt_map_map26;
extern mapinfo_t		tnt_map_map27;
extern mapinfo_t		tnt_map_map28;
extern mapinfo_t		tnt_map_map29;
extern mapinfo_t		tnt_map_map30;
extern mapinfo_t		tnt_map_map31;
extern mapinfo_t		tnt_map_map32;
extern mapinfo_t		tnt_map_map33;
extern mapinfo_t		tnt_map_map34;
extern mapinfo_t		tnt_map_map35;

extern interlevel_t		tnt_interlevel;

extern intermission_t	tnt_intermission_e1;
extern intermission_t	tnt_intermission_e2;
extern intermission_t	tnt_intermission_e3;
extern intermission_t	tnt_intermission_e4;
extern intermission_t	tnt_intermission_s1;
extern intermission_t	tnt_intermission_s2;

extern endgame_t		tnt_endgame;

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

mapinfo_t* tnt_maps[] =
{
	&tnt_map_map01, &tnt_map_map02, &tnt_map_map03, &tnt_map_map04,
	&tnt_map_map05, &tnt_map_map06, &tnt_map_map07, &tnt_map_map08,
	&tnt_map_map09, &tnt_map_map10, &tnt_map_map11, &tnt_map_map12,
	&tnt_map_map13, &tnt_map_map14, &tnt_map_map15, &tnt_map_map16,
	&tnt_map_map17, &tnt_map_map18, &tnt_map_map19, &tnt_map_map20,
	&tnt_map_map21, &tnt_map_map22, &tnt_map_map23, &tnt_map_map24,
	&tnt_map_map25, &tnt_map_map26, &tnt_map_map27, &tnt_map_map28,
	&tnt_map_map29, &tnt_map_map30, &tnt_map_map31, &tnt_map_map32,
};

episodeinfo_t tnt_episode =
{
	PlainFlowString( "Evilution" ),					// name
	EmptyFlowString(),								// name_patch_lump
	1,												// episode_num
	tnt_maps,										// all_maps
	arrlen( tnt_maps ),								// num_maps
	tnt_map_map32.map_num,							// highest_map_num
	&tnt_map_map01									// first_map
};

episodeinfo_t* tnt_episodes[] =
{
	&tnt_episode
};

gameflow_t doom_tnt =
{
	PlainFlowString( "DOOM 2: TNT - Evilution" ),	// name
	tnt_episodes,									// episodes
	arrlen( tnt_episodes ),							// num_episodes
};

//============================================================================
// Maps
//============================================================================

//============================================================================
// Maps 1-11
//============================================================================

mapinfo_t tnt_map_map01 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( THUSTR_1 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Mustaine" ),				// authors
	&tnt_episode,									// episode
	1,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "runnin" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	30,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map02,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map01,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map02 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( THUSTR_2 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Wakelin" ),				// authors
	&tnt_episode,									// episode
	2,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "stalks" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map03,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map02,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map03 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( THUSTR_3 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Minadeo, Robin Patenall" ),	// authors
	&tnt_episode,									// episode
	3,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "countd" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map04,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map03,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map04 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( THUSTR_4 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Ty Halderman" ),				// authors
	&tnt_episode,									// episode
	4,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "betwee" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map05,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map04,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map05 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( THUSTR_5 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Jim Dethlefsen" ),			// authors
	&tnt_episode,									// episode
	5,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "doom" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map06,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map05,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map06 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( THUSTR_6 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Ty Halderman, Jimmy Sieben" ),	// authors
	&tnt_episode,									// episode
	6,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "the_da" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map07,									// next_map
	&tnt_intermission_e1,							// next_map_intermission
	&tnt_map_map06,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

static bossaction_t		tnt_map07special[] =
{
	{ 8 /*MT_FATSO*/, MF2_MBF21_MAP07BOSS1, 82 /*lowerFloorToLowest repeatable*/, 666 },
	{ 20 /*MT_BABY*/, MF2_MBF21_MAP07BOSS2, 96 /*raiseToTexture repeatable*/, 667 },
};

mapinfo_t tnt_map_map07 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( THUSTR_7 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Andrew Dowswell" ),			// authors
	&tnt_episode,									// episode
	7,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "shawn" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	tnt_map07special,								// boss_actions
	arrlen( tnt_map07special ),						// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map08,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map07,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map08 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( THUSTR_8 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Minadeo" ),				// authors
	&tnt_episode,									// episode
	8,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "ddtblu" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map09,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map08,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map09 =
{
	LevelFlowString( levellump0_format_text ),		// data_lump
	FlowString( THUSTR_9 ),							// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Jimmy Sieben" ),				// authors
	&tnt_episode,									// episode
	9,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "in_cit" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	270,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map10,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map09,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map10 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_10 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Mustaine" ),				// authors
	&tnt_episode,									// episode
	10,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "dead" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map11,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map10,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map11 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_11 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Dean Johnson" ),				// authors
	&tnt_episode,									// episode
	11,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "stlks2" ) },				// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	210,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map12,									// next_map
	&tnt_intermission_e2,							// next_map_intermission
	&tnt_map_map11,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

//============================================================================
// Maps 12-20
//============================================================================

mapinfo_t tnt_map_map12 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_12 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Jim Lowell" ),				// authors
	&tnt_episode,									// episode
	12,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "theda2" ) },				// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map13,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map12,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map13 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_13 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Brian Kidby" ),				// authors
	&tnt_episode,									// episode
	13,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "doom2" ) },				// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map14,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map13,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map14 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_14 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Robin Patenall" ),			// authors
	&tnt_episode,									// episode
	14,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "ddtbl2" ) },				// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map15,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map14,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map15 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_15 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "William D. Whitaker" ),		// authors
	&tnt_episode,									// episode
	15,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "runni2" ) },				// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	210,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map16,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map31,									// secret_map
	&tnt_intermission_s1,							// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map16 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_16 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Andre Arsenault" ),			// authors
	&tnt_episode,									// episode
	16,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "dead2" ) },				// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map17,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map16,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map17 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_17 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Mustaine" ),				// authors
	&tnt_episode,									// episode
	17,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "stlks3" ) },				// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	420,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map18,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map17,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map18 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_18 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Ty Halderman" ),				// authors
	&tnt_episode,									// episode
	18,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "romero" ) },				// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map19,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map18,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map19 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_19 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Ty Halderman" ),				// authors
	&tnt_episode,									// episode
	19,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "shawn2" ) },				// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	200,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map20,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map19,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map20 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_20 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Drake O'Brien" ),				// authors
	&tnt_episode,									// episode
	20,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "messag" ) },				// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map21,									// next_map
	&tnt_intermission_e3,							// next_map_intermission
	&tnt_map_map20,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

//============================================================================
// Maps 21-30
//============================================================================

mapinfo_t tnt_map_map21 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_21 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Drake O'Brien" ),				// authors
	&tnt_episode,									// episode
	21,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "count2" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	240,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map22,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map21,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map22 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_22 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Christopher Buteau" ),		// authors
	&tnt_episode,									// episode
	22,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "ddtbl3" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map23,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map22,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map23 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_23 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Paul Turnbull" ),				// authors
	&tnt_episode,									// episode
	23,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "ampie" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	180,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map24,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map23,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map24 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_24 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Dean Johnson" ),				// authors
	&tnt_episode,									// episode
	24,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "theda3" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map25,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map24,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map25 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_25 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "David J. Hill" ),				// authors
	&tnt_episode,									// episode
	25,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "adrian" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	150,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map26,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map25,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map26 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_26 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Jim Lowell, Mark Snell" ),	// authors
	&tnt_episode,									// episode
	26,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "messg2" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	300,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map27,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map26,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map27 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_27 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Drake O'Brien" ),				// authors
	&tnt_episode,									// episode
	27,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "romer2" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	330,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map28,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map27,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map28 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_28 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Milo Casali" ),				// authors
	&tnt_episode,									// episode
	28,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "tense" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	420,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map29,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map28,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map29 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_29 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Jimmy Sieben" ),				// authors
	&tnt_episode,									// episode
	29,												// map_num
	Map_None,										// map_flags
	{ RuntimeFlowString( "shawn3" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	300,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map30,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map29,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map30 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_30 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Jimmy Sieben" ),				// authors
	&tnt_episode,									// episode
	30,												// map_num
	Map_Doom2EndOfGame,								// map_flags
	{ RuntimeFlowString( "openin" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	180,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	nullptr,										// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map30,									// secret_map
	nullptr,										// secret_map_intermission
	&tnt_endgame,									// endgame
};

//============================================================================
// Maps 31-35
//============================================================================

mapinfo_t tnt_map_map31 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_31 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Dario Casali" ),				// authors
	&tnt_episode,									// episode
	31,												// map_num
	Map_Secret,										// map_flags
	{ RuntimeFlowString( "evil" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map16,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map32,									// secret_map
	&tnt_intermission_s2,							// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t tnt_map_map32 =
{
	LevelFlowString( levellumpn_format_text ),		// data_lump
	FlowString( THUSTR_32 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Dario Casali" ),				// authors
	&tnt_episode,									// episode
	32,												// map_num
	Map_Secret,										// map_flags
	{ RuntimeFlowString( "ultima" ) },				// music_lump
	FlowString( "SKY3" ),							// sky_texture
	0,												// sky_scroll_speed
	30,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&tnt_interlevel,								// interlevel_finished
	&tnt_interlevel,								// interlevel_entering
	&tnt_map_map16,									// next_map
	nullptr,										// next_map_intermission
	&tnt_map_map32,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

//============================================================================
// Intermissions, endgames, and interlevels
//============================================================================

//============================================================================
// Hell On Earth
//============================================================================

intermission_t tnt_intermission_e1 =
{
	Intermission_Skippable,							// type
	FlowString( T1TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "SLIME16" ),						// background_lump
};

intermission_t tnt_intermission_e2 =
{
	Intermission_Skippable,							// type
	FlowString( T2TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK14" ),						// background_lump
};

intermission_t tnt_intermission_e3 =
{
	Intermission_Skippable,							// type
	FlowString( T3TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK07" ),						// background_lump
};

intermission_t tnt_intermission_e4 =
{
	Intermission_Skippable,							// type
	FlowString( T4TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK17" ),						// background_lump
};

intermission_t tnt_intermission_s1 =
{
	Intermission_Skippable,							// type
	FlowString( T5TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK13" ),						// background_lump
};

intermission_t tnt_intermission_s2 =
{
	Intermission_Skippable,							// type
	FlowString( T6TEXT ),							// text
	RuntimeFlowString( "read_m" ),					// music_lump
	FlowString( "RROCK19" ),						// background_lump
};

endgame_t tnt_endgame =
{
	EndGame_Cast | EndGame_LoopingMusic | EndGame_BuiltInCast,	// type
	&tnt_intermission_e4,							// intermission
	FlowString( "BOSSBACK" ),						// primary_image_lump
	EmptyFlowString(),								// secondary_image_lump
	RuntimeFlowString( "evil" ),					// music_lump
	nullptr,										// cast_members
	0,												// num_cast_members
	EmptyFlowString(),								// bunny_end_overlay
	0,												// bunny_end_count
	0,												// bunny_end_x
	0,												// bunny_end_y
	0,												// bunny_end_sound
};

interlevel_t tnt_interlevel =
{
	RuntimeFlowString( "dm2int" ),					// music_lump
	FlowString( "INTERPIC" ),						// background_lump
	nullptr,										// layers
	0,												// num_layers
};


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
// DESCRIPTION: Defines the gameflow types for all iterations of
//              the original Doom.

#include "d_playsim.h"
#include "dstrings.h"

#include "i_timer.h"

#include "m_container.h"

// Forward declarations
extern episodeinfo_t	doom_episode_one;
extern episodeinfo_t	doom_episode_two;
extern episodeinfo_t	doom_episode_three;
extern episodeinfo_t	doom_episode_four;

extern mapinfo_t		doom_map_e1m1;
extern mapinfo_t		doom_map_e1m2;
extern mapinfo_t		doom_map_e1m3;
extern mapinfo_t		doom_map_e1m4;
extern mapinfo_t		doom_map_e1m5;
extern mapinfo_t		doom_map_e1m6;
extern mapinfo_t		doom_map_e1m7;
extern mapinfo_t		doom_map_e1m8;
extern mapinfo_t		doom_map_e1m9;

extern mapinfo_t		doom_map_e2m1;
extern mapinfo_t		doom_map_e2m2;
extern mapinfo_t		doom_map_e2m3;
extern mapinfo_t		doom_map_e2m4;
extern mapinfo_t		doom_map_e2m5;
extern mapinfo_t		doom_map_e2m6;
extern mapinfo_t		doom_map_e2m7;
extern mapinfo_t		doom_map_e2m8;
extern mapinfo_t		doom_map_e2m9;

extern mapinfo_t		doom_map_e3m1;
extern mapinfo_t		doom_map_e3m2;
extern mapinfo_t		doom_map_e3m3;
extern mapinfo_t		doom_map_e3m4;
extern mapinfo_t		doom_map_e3m5;
extern mapinfo_t		doom_map_e3m6;
extern mapinfo_t		doom_map_e3m7;
extern mapinfo_t		doom_map_e3m8;
extern mapinfo_t		doom_map_e3m9;

extern mapinfo_t		doom_map_e4m1;
extern mapinfo_t		doom_map_e4m2;
extern mapinfo_t		doom_map_e4m3;
extern mapinfo_t		doom_map_e4m4;
extern mapinfo_t		doom_map_e4m5;
extern mapinfo_t		doom_map_e4m6;
extern mapinfo_t		doom_map_e4m7;
extern mapinfo_t		doom_map_e4m8;
extern mapinfo_t		doom_map_e4m9;

extern interlevel_t		doom_interlevel_e1finished;
extern interlevel_t		doom_interlevel_e1entering;

extern interlevel_t		doom_interlevel_e2finished;
extern interlevel_t		doom_interlevel_e2entering;

extern interlevel_t		doom_interlevel_e3finished;
extern interlevel_t		doom_interlevel_e3entering;

extern interlevel_t		doom_interlevel_e4;

extern intermission_t	doom_intermission_e1;
extern intermission_t	doom_intermission_e2;
extern intermission_t	doom_intermission_e3;
extern intermission_t	doom_intermission_e4;

extern endgame_t		doom_endgame_e1;
extern endgame_t		doom_endgame_e2;
extern endgame_t		doom_endgame_e3;
extern endgame_t		doom_endgame_e4;

extern gameflow_t		doom_gameflow_shareware;
extern gameflow_t		doom_gameflow_registered;
extern gameflow_t		doom_gameflow_ultimate;

// Helpers

// This needs to live in a header somewhere... and I think we need to 100% opt-in to it, at least while we mix C and C++ code everywhere

// You can use std::enable_if in earlier C++ versions if required...
template< typename _type >
requires std::is_enum_v< _type >
auto operator|( _type lhs, _type rhs )
{
	using underlying = std::underlying_type_t< _type >;
	return (_type)( (underlying)lhs | (underlying)rhs );
}

template< typename _type >
requires std::is_enum_v< _type >
auto operator&( _type lhs, _type rhs )
{
	using underlying = std::underlying_type_t< _type >;
	return (_type)( (underlying)lhs & (underlying)rhs );
}

template< typename _type >
requires std::is_enum_v< _type >
auto operator^( _type lhs, _type rhs )
{
	using underlying = std::underlying_type_t< _type >;
	return (_type)( (underlying)lhs ^ (underlying)rhs );
}

template< typename _type >
requires std::is_enum_v< _type >
auto operator~( _type val )
{
	using underlying = std::underlying_type_t< _type >;
	return (_type)( (underlying)~val );
}

// I tried to do all this with constexpr/consteval but MSVC refuses to play nice.
// So there's some terrible static object generation just to get this working...

constexpr int32_t standardduration = TICRATE / 3;

#define frame( lump, type ) { lump, type, standardduration }
#define frame_withtime( lump, type, duration ) { lump, type, duration }

#define stringof( val ) #val

#define generate_frameseqstatic( name ) static interlevelframe_t doom_frames_ ## name [] = { \
	frame( stringof( val ) "00", Frame_Infinite ), \
};

#define generate_frameseq3( name ) static interlevelframe_t doom_frames_ ## name [] = { \
	frame( stringof( val ) "00", Frame_RandomDuration ), \
	frame( stringof( val ) "01", Frame_RandomDuration ), \
	frame( stringof( val ) "02", Frame_RandomDuration ), \
};

#define get_frameseq( name ) doom_frames_ ## name
#define get_frameseqlen( name ) arrlen( get_frameseq( name ) )

generate_frameseq3( WIA000 );
generate_frameseq3( WIA001 );
generate_frameseq3( WIA002 );
generate_frameseq3( WIA003 );
generate_frameseq3( WIA004 );
generate_frameseq3( WIA005 );
generate_frameseq3( WIA006 );
generate_frameseq3( WIA007 );
generate_frameseq3( WIA008 );
generate_frameseq3( WIA009 );

generate_frameseqstatic( WIA100 );
generate_frameseqstatic( WIA101 );
generate_frameseqstatic( WIA102 );
generate_frameseqstatic( WIA103 );
generate_frameseqstatic( WIA104 );
generate_frameseqstatic( WIA105 );
generate_frameseqstatic( WIA106 );
generate_frameseq3( WIA107 );

generate_frameseq3( WIA200 );
generate_frameseq3( WIA201 );
generate_frameseq3( WIA202 );
generate_frameseq3( WIA203 );
generate_frameseq3( WIA204 );
generate_frameseq3( WIA205 );

static interlevelframe_t doom_frames_splat[] =
{
	frame_withtime( "WISPLAT", Frame_Infinite, -1 )
};

static interlevelframe_t doom_frames_youarehereleft[] =
{
	frame_withtime( "WIURH0", Frame_FixedDuration, 20 ),
	frame_withtime( NULL, Frame_FixedDuration, 12 ),
};

static interlevelframe_t doom_frames_youarehereright[] =
{
	frame_withtime( "WIURH1", Frame_FixedDuration, 20 ),
	frame_withtime( NULL, Frame_FixedDuration, 12 ),
};

#define generate_locationcond( map, levelnum ) \
static interlevelcond_t doom_splatcond_ ## map [] = \
{ \
	{ \
		AnimCondition_MapNumGreater, \
		levelnum \
	} \
}; \
\
static interlevelcond_t doom_herecond_ ## map [] = \
{ \
	{ \
		AnimCondition_MapNumEqual, \
		levelnum \
	}, \
	{ \
		AnimCondition_FitsInFrame, \
		levelnum \
	}, \
}

#define get_splatcond( map ) doom_splatcond_ ## map
#define get_splatcondlen( map ) arrlen( get_splatcond( map ) )
#define get_herecond( map ) doom_herecond_ ## map
#define get_herecondlen( map ) arrlen( get_herecond( map ) )

generate_locationcond( E1M1, 1 );
generate_locationcond( E1M2, 2 );
generate_locationcond( E1M3, 3 );
generate_locationcond( E1M4, 4 );
generate_locationcond( E1M5, 5 );
generate_locationcond( E1M6, 6 );
generate_locationcond( E1M7, 7 );
generate_locationcond( E1M8, 8 );
generate_locationcond( E1M9, 9 );

generate_locationcond( E2M1, 1 );
generate_locationcond( E2M2, 2 );
generate_locationcond( E2M3, 3 );
generate_locationcond( E2M4, 4 );
generate_locationcond( E2M5, 5 );
generate_locationcond( E2M6, 6 );
generate_locationcond( E2M7, 7 );
generate_locationcond( E2M8, 8 );
generate_locationcond( E2M9, 9 );

generate_locationcond( E3M1, 1 );
generate_locationcond( E3M2, 2 );
generate_locationcond( E3M3, 3 );
generate_locationcond( E3M4, 4 );
generate_locationcond( E3M5, 5 );
generate_locationcond( E3M6, 6 );
generate_locationcond( E3M7, 7 );
generate_locationcond( E3M8, 8 );
generate_locationcond( E3M9, 9 );

#define generatelocationanim( arrayname, map, xpos, ypos ) \
{ \
	arrayname, \
	arrlen( arrayname ), \
	xpos, \
	ypos, \
	get_herecond( map ), \
	get_herecondlen( map ) \
}

#define generatebackgroundanim( frameseq, xpos, ypos ) \
{ \
	get_frameseq( frameseq ), \
	get_frameseqlen( frameseq ), \
	xpos, \
	ypos, \
	NULL, \
	0 \
}


#define generatelocationanims( map, xpos, ypos ) \
generatelocationanim( doom_frames_splat, map, xpos, ypos ), \
generatelocationanim( doom_frames_youarehereleft, map, xpos, ypos ), \
generatelocationanim( doom_frames_youarehereright, map, xpos, ypos )

// Now for the full declarations

//============================================================================
// Game setup (episodes etc)
//============================================================================

episodeinfo_t doom_episode_one =
{
	"Knee-Deep In The Dead",		// name
	"M_EPI1",						// name_patch_lump
	&doom_map_e1m1,					// first_map
	1								// episode_num
};

//episodeinfo_t doom_episode_two =
//{
//	"The Shores Of Hell",			// name
//	"M_EPI2",						// name_patch_lump
//	&doom_map_e2m1,					// first_map
//	2								// episode_num
//};
//
//episodeinfo_t doom_episode_three =
//{
//	"Inferno",						// name
//	"M_EPI3",						// name_patch_lump
//	&doom_map_e3m1,					// first_map
//	3								// episode_num
//};
//
//episodeinfo_t doom_episode_four =
//{
//	"Thy Flesh Consumed",			// name
//	"M_EPI4",						// name_patch_lump
//	&doom_map_e4m1,					// first_map
//	4								// episode_num
//};

episodeinfo_t* doom_episodes_shareware[] =
{
	&doom_episode_one
};

//episodeinfo_t* doom_episodes_registered[] =
//{
//	&doom_episode_one,
//	&doom_episode_two,
//	&doom_episode_three
//};
//
//episodeinfo_t* doom_episodes_ultimate[] =
//{
//	&doom_episode_one,
//	&doom_episode_two,
//	&doom_episode_three,
//	&doom_episode_four
//};

gameflow_t doom_gameflow_shareware =
{
	"DOOM Shareware",				// name
	doom_episodes_shareware,		// episodes
	arrlen( doom_episodes_shareware ),	// num_episodes
	"vanilla",						// playsim_base
	""								// playsim_options
};

//gameflow_t doom_gameflow_registered =
//{
//	"DOOM Registered",				// name
//	doom_episodes_registered,		// episodes
//	arrlen( doom_episodes_registered ),	// num_episodes
//	"vanilla",						// playsim_base
//	""								// playsim_options
//};
//
//gameflow_t doom_gameflow_ultimate =
//{
//	"The Ultimate DOOM",			// name
//	doom_episodes_ultimate,			// episodes
//	arrlen( doom_episodes_ultimate ),	// num_episodes
//	"vanilla",						// playsim_base
//	""								// playsim_options
//};

//============================================================================
// Maps
//============================================================================

mapinfo_t doom_map_e1m1 =
{
	"E1M1",							// data_lump
	HUSTR_E1M1,						// name
	"WILV00",						// name_patch_lump
	"John Romero",					// authors
	&doom_episode_one,				// episode
	1,								// map_num
	"D_E1M1",						// music_lump
	"SKY1",							// sky_texture
	0,								// sky_scroll_speed
	30,								// par_time
	NULL,							// boss_actions
	0,								// num_boss_actions
	&doom_interlevel_e1finished,	// interlevel_finished
	&doom_interlevel_e1entering,	// interlevel_entering
	&doom_map_e1m2,					// next_map
	NULL,							// next_map_intermission
	&doom_map_e1m9,					// secret_map
	NULL,							// secret_map_intermission
	NULL,							// endgame
};

mapinfo_t doom_map_e1m2 =
{
	"E1M2",							// data_lump
	HUSTR_E1M2,						// name
	"WILV01",						// name_patch_lump
	"John Romero",					// authors
	&doom_episode_one,				// episode
	2,								// map_num
	"D_E1M2",						// music_lump
	"SKY1",							// sky_texture
	0,								// sky_scroll_speed
	75,								// par_time
	NULL,							// boss_actions
	0,								// num_boss_actions
	&doom_interlevel_e1finished,	// interlevel_finished
	&doom_interlevel_e1entering,	// interlevel_entering
	&doom_map_e1m3,					// next_map
	NULL,							// next_map_intermission
	&doom_map_e1m9,					// secret_map
	NULL,							// secret_map_intermission
	NULL,							// endgame
};

mapinfo_t doom_map_e1m3 =
{
	"E1M3",							// data_lump
	HUSTR_E1M3,						// name
	"WILV02",						// name_patch_lump
	"John Romero",					// authors
	&doom_episode_one,				// episode
	3,								// map_num
	"D_E1M3",						// music_lump
	"SKY1",							// sky_texture
	0,								// sky_scroll_speed
	120,							// par_time
	NULL,							// boss_actions
	0,								// num_boss_actions
	&doom_interlevel_e1finished,	// interlevel_finished
	&doom_interlevel_e1entering,	// interlevel_entering
	&doom_map_e1m4,					// next_map
	NULL,							// next_map_intermission
	&doom_map_e1m9,					// secret_map
	NULL,							// secret_map_intermission
	NULL,							// endgame
};

mapinfo_t doom_map_e1m4 =
{
	"E1M4",							// data_lump
	HUSTR_E1M4,						// name
	"WILV03",						// name_patch_lump
	"Tom Hall, John Romero",		// authors
	&doom_episode_one,				// episode
	4,								// map_num
	"D_E1M4",						// music_lump
	"SKY1",							// sky_texture
	0,								// sky_scroll_speed
	90,								// par_time
	NULL,							// boss_actions
	0,								// num_boss_actions
	&doom_interlevel_e1finished,	// interlevel_finished
	&doom_interlevel_e1entering,	// interlevel_entering
	&doom_map_e1m5,					// next_map
	NULL,							// next_map_intermission
	&doom_map_e1m9,					// secret_map
	NULL,							// secret_map_intermission
	NULL,							// endgame
};

mapinfo_t doom_map_e1m5 =
{
	"E1M5",							// data_lump
	HUSTR_E1M5,						// name
	"WILV04",						// name_patch_lump
	"John Romero",					// authors
	&doom_episode_one,				// episode
	5,								// map_num
	"D_E1M5",						// music_lump
	"SKY1",							// sky_texture
	0,								// sky_scroll_speed
	165,							// par_time
	NULL,							// boss_actions
	0,								// num_boss_actions
	&doom_interlevel_e1finished,	// interlevel_finished
	&doom_interlevel_e1entering,	// interlevel_entering
	&doom_map_e1m6,					// next_map
	NULL,							// next_map_intermission
	&doom_map_e1m9,					// secret_map
	NULL,							// secret_map_intermission
	NULL,							// endgame
};

mapinfo_t doom_map_e1m6 =
{
	"E1M6",							// data_lump
	HUSTR_E1M6,						// name
	"WILV05",						// name_patch_lump
	"John Romero",					// authors
	&doom_episode_one,				// episode
	6,								// map_num
	"D_E1M6",						// music_lump
	"SKY1",							// sky_texture
	0,								// sky_scroll_speed
	180,							// par_time
	NULL,							// boss_actions
	0,								// num_boss_actions
	&doom_interlevel_e1finished,	// interlevel_finished
	&doom_interlevel_e1entering,	// interlevel_entering
	&doom_map_e1m7,					// next_map
	NULL,							// next_map_intermission
	&doom_map_e1m9,					// secret_map
	NULL,							// secret_map_intermission
	NULL,							// endgame
};

mapinfo_t doom_map_e1m7 =
{
	"E1M7",							// data_lump
	HUSTR_E1M7,						// name
	"WILV06",						// name_patch_lump
	"John Romero",					// authors
	&doom_episode_one,				// episode
	7,								// map_num
	"D_E1M7",						// music_lump
	"SKY1",							// sky_texture
	0,								// sky_scroll_speed
	180,							// par_time
	NULL,							// boss_actions
	0,								// num_boss_actions
	&doom_interlevel_e1finished,	// interlevel_finished
	&doom_interlevel_e1entering,	// interlevel_entering
	&doom_map_e1m8,					// next_map
	NULL,							// next_map_intermission
	&doom_map_e1m9,					// secret_map
	NULL,							// secret_map_intermission
	NULL,							// endgame
};

mapinfo_t doom_map_e1m8 =
{
	"E1M8",							// data_lump
	HUSTR_E1M8,						// name
	"WILV07",						// name_patch_lump
	"Tom Hall, Sandy Petersen",		// authors
	&doom_episode_one,				// episode
	8,								// map_num
	"D_E1M8",						// music_lump
	"SKY1",							// sky_texture
	0,								// sky_scroll_speed
	30,								// par_time
	NULL,							// boss_actions
	0,								// num_boss_actions
	&doom_interlevel_e1finished,	// interlevel_finished
	NULL,							// interlevel_entering
	NULL,							// next_map
	NULL,							// next_map_intermission
	NULL,							// secret_map
	NULL,							// secret_map_intermission
	&doom_endgame_e1,				// endgame
};

mapinfo_t doom_map_e1m9 =
{
	"E1M9",							// data_lump
	HUSTR_E1M9,						// name
	"WILV08",						// name_patch_lump
	"John Romero",					// authors
	&doom_episode_one,				// episode
	9,								// map_num
	"D_E1M9",						// music_lump
	"SKY1",							// sky_texture
	0,								// sky_scroll_speed
	165,							// par_time
	NULL,							// boss_actions
	0,								// num_boss_actions
	&doom_interlevel_e1finished,	// interlevel_finished
	&doom_interlevel_e1entering,	// interlevel_entering
	&doom_map_e1m4,					// next_map
	NULL,							// next_map_intermission
	&doom_map_e1m9,					// secret_map
	NULL,							// secret_map_intermission
	NULL,							// endgame
};

//============================================================================
// Intermissions, endgames, and interlevels
//============================================================================

intermission_t doom_intermission_e1 =
{
	E1TEXT,							// text
	"D_VICTOR",						// music_lump
	"FLOOR4_8",						// background_lump
};

endgame_t doom_endgame_e1 =
{
	EndGame_Pic | EndGame_Ultimate | EndGame_SkipInterlevel,	// type
	&doom_intermission_e1,			// intermission
	"HELP2",						// primary_image_lump
	"CREDIT",						// secondary_image_lump
};

static interlevelanim_t doom_anim_e1_back[] =
{
	generatebackgroundanim( WIA000, 224, 104 ),
	generatebackgroundanim( WIA001, 184, 160 ),
	generatebackgroundanim( WIA002, 112, 136 ),
	generatebackgroundanim( WIA003, 72, 112 ),
	generatebackgroundanim( WIA004, 88, 96 ),
	generatebackgroundanim( WIA005, 64, 48 ),
	generatebackgroundanim( WIA006, 192, 140 ),
	generatebackgroundanim( WIA007, 136, 16 ),
	generatebackgroundanim( WIA008, 80, 16 ),
	generatebackgroundanim( WIA009, 64, 24 )
};

static interlevelanim_t doom_anim_e1_fore[] =
{
	generatelocationanims( E1M1, 185, 164 ),
	generatelocationanims( E1M2, 148, 143 ),
	generatelocationanims( E1M3, 69, 122 ),
	generatelocationanims( E1M4, 209, 102 ),
	generatelocationanims( E1M5, 116, 89 ),
	generatelocationanims( E1M6, 166, 55 ),
	generatelocationanims( E1M7, 71, 56 ),
	generatelocationanims( E1M8, 135, 29 ),
	generatelocationanims( E1M9, 71, 24 ),
};

interlevel_t doom_interlevel_e1finished =
{
	Interlevel_Animated,			// type
	"WIMAP0",						// background_lump
	doom_anim_e1_back,				// background_anims
	arrlen( doom_anim_e1_back ),	// num_background_anims
	NULL,							// foreground_anims
	0,								// num_foreground_anims
};

interlevel_t doom_interlevel_e1entering =
{
	Interlevel_Animated,			// type
	"WIMAP0",						// background_lump
	doom_anim_e1_back,				// background_anims
	arrlen( doom_anim_e1_back ),	// num_background_anims
	doom_anim_e1_fore,				// foreground_anims
	arrlen( doom_anim_e1_fore ),	// num_foreground_anims
};

//============================================================================
// HACKS
//============================================================================

gameflow_t* current_game = &doom_gameflow_shareware;
episodeinfo_t* current_episode = &doom_episode_one;
mapinfo_t* current_map = &doom_map_e1m1;

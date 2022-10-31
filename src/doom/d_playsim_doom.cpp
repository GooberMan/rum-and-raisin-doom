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
extern episodeinfo_t	doom_shareware_episode_two;
extern episodeinfo_t	doom_shareware_episode_three;
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

#define frame_format_text "WIA%d%.2d%.2d"
#define levename_format_text "WILV%d%d"
#define levellump_format_text "E%dM%d"
#define background_format_text "WIMAP%d"

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

// I tried to do all this with constexpr/consteval but MSVC refuses to play nice.
// So there's some terrible static object generation just to get this working...

constexpr int32_t standardduration = TICRATE / 3;

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


#define frame( lump, type )								{ FlowString( lump ), type, standardduration }
#define frame_withtime( lump, type, duration )			{ FlowString( lump ), type, duration }
#define frame_runtime( lump, type, index, frame )		{ RuntimeFlowString( lump ), type, standardduration, index, frame }
#define frame_runtime_infinite( lump, index, frame )	{ RuntimeFlowString( lump ), Frame_Infinite, -1, index, frame }

#define generate_name( ep, index ) doom_frames_ ## ep ## _ ## index

#define generate_frameseqstatic( ep, index ) static interlevelframe_t generate_name( ep, index ) [] = { \
	frame_runtime_infinite( frame_format_text, index, 0 ), \
};

#define generate_frameseq3( ep, index ) static interlevelframe_t generate_name( ep, index ) [] = { \
	frame_runtime( frame_format_text, Frame_FixedDuration | Frame_RandomStart, index, 0 ), \
	frame_runtime( frame_format_text, Frame_FixedDuration | Frame_RandomStart, index, 1 ), \
	frame_runtime( frame_format_text, Frame_FixedDuration | Frame_RandomStart, index, 2 ), \
};

#define generate_frameseq3_ep2secret( ep, index ) static interlevelframe_t generate_name( ep, index ) [] = { \
	frame_runtime( frame_format_text, Frame_FixedDuration, index, 0 ), \
	frame_runtime( frame_format_text, Frame_FixedDuration, index, 1 ), \
	frame_runtime_infinite( frame_format_text, index, 2 ), \
};

#define get_frameseq( ep, index ) generate_name( ep, index )
#define get_frameseqlen( ep, index ) arrlen( get_frameseq( ep, index ) )

generate_frameseq3( 0, 0 );
generate_frameseq3( 0, 1 );
generate_frameseq3( 0, 2 );
generate_frameseq3( 0, 3 );
generate_frameseq3( 0, 4 );
generate_frameseq3( 0, 5 );
generate_frameseq3( 0, 6 );
generate_frameseq3( 0, 7 );
generate_frameseq3( 0, 8 );
generate_frameseq3( 0, 9 );

generate_frameseqstatic( 1, 0 );
generate_frameseqstatic( 1, 1 );
generate_frameseqstatic( 1, 2 );
generate_frameseqstatic( 1, 3 );
generate_frameseqstatic( 1, 4 );
generate_frameseqstatic( 1, 5 );
generate_frameseqstatic( 1, 6 );
generate_frameseq3_ep2secret( 1, 7 );

generate_frameseq3( 2, 0 );
generate_frameseq3( 2, 1 );
generate_frameseq3( 2, 2 );
generate_frameseq3( 2, 3 );
generate_frameseq3( 2, 4 );
generate_frameseq3( 2, 5 );

static interlevelframe_t doom_frames_splat[] =
{
	frame_withtime( "WISPLAT", Frame_Infinite, -1 )
};

static interlevelframe_t doom_frames_youarehereleft[] =
{
	frame_withtime( "WIURH0", Frame_FixedDuration, 20 ),
	frame_withtime( nullptr, Frame_FixedDuration, 12 ),
};

static interlevelframe_t doom_frames_youarehereright[] =
{
	frame_withtime( "WIURH1", Frame_FixedDuration, 20 ),
	frame_withtime( nullptr, Frame_FixedDuration, 12 ),
};

#define generate_herecond( map, levelnum ) \
static interlevelcond_t doom_herecond_ ## map [] = \
{ \
	{ \
		AnimCondition_MapNumEqual, \
		levelnum \
	}, \
	{ \
		AnimCondition_FitsInFrame, \
		1 \
	}, \
}

#define generate_locationcond( map, levelnum ) \
static interlevelcond_t doom_splatcond_ ## map [] = \
{ \
	{ \
		AnimCondition_MapNumGreater, \
		levelnum \
	} \
}; \
\
generate_herecond( map, levelnum )

#define generate_locationcond_aftersecret( map, levelnum ) \
static interlevelcond_t doom_splatcond_ ## map [] = \
{ \
	{ \
		AnimCondition_MapNumGreater, \
		levelnum \
	}, \
	{ \
		AnimCondition_MapNotSecret, \
		0 \
	} \
}; \
\
generate_herecond( map, levelnum )


#define generate_locationcond_issecret( map, levelnum ) \
static interlevelcond_t doom_splatcond_ ## map [] = \
{ \
	{ \
		AnimCondition_SecretVisited, \
		0 \
	} \
}; \
\
generate_herecond( map, levelnum )

#define generate_exactmap_condition( map, levelnum ) \
static interlevelcond_t doom_exactmapcond_ ## map [] = \
{ \
	{ \
		AnimCondition_MapNumEqual, \
		levelnum \
	} \
}

#define get_splatcond( map ) doom_splatcond_ ## map
#define get_splatcondlen( map ) arrlen( get_splatcond( map ) )
#define get_herecond( map ) doom_herecond_ ## map
#define get_herecondlen( map ) arrlen( get_herecond( map ) )
#define get_exactmapcond( map ) doom_exactmapcond_ ## map
#define get_exactmapcondlen( map ) arrlen( get_exactmapcond( map ) )

generate_locationcond( E1M1, 1 );
generate_locationcond( E1M2, 2 );
generate_locationcond( E1M3, 3 );
generate_locationcond_aftersecret( E1M4, 4 );
generate_locationcond_aftersecret( E1M5, 5 );
generate_locationcond_aftersecret( E1M6, 6 );
generate_locationcond_aftersecret( E1M7, 7 );
generate_locationcond_aftersecret( E1M8, 8 );
generate_locationcond_issecret( E1M9, 9 );

generate_locationcond( E2M1, 1 );
generate_locationcond( E2M2, 2 );
generate_locationcond( E2M3, 3 );
generate_locationcond( E2M4, 4 );
generate_locationcond( E2M5, 5 );
generate_locationcond_aftersecret( E2M6, 6 );
generate_locationcond_aftersecret( E2M7, 7 );
generate_locationcond_aftersecret( E2M8, 8 );
generate_locationcond_issecret( E2M9, 9 );

generate_locationcond( E3M1, 1 );
generate_locationcond( E3M2, 2 );
generate_locationcond( E3M3, 3 );
generate_locationcond( E3M4, 4 );
generate_locationcond( E3M5, 5 );
generate_locationcond( E3M6, 6 );
generate_locationcond_aftersecret( E3M7, 7 );
generate_locationcond_aftersecret( E3M8, 8 );
generate_locationcond_issecret( E3M9, 9 );

generate_exactmap_condition( E2M1, 1 );
generate_exactmap_condition( E2M2, 2 );
generate_exactmap_condition( E2M3, 3 );
generate_exactmap_condition( E2M4, 4 );
generate_exactmap_condition( E2M5, 5 );
generate_exactmap_condition( E2M6, 6 );
generate_exactmap_condition( E2M7, 7 );
generate_exactmap_condition( E2M8, 8 );
generate_exactmap_condition( E2M9, 9 );

#define generatesplatanim( arrayname, map, xpos, ypos ) \
{ \
	arrayname, \
	arrlen( arrayname ), \
	xpos, \
	ypos, \
	get_splatcond( map ), \
	get_splatcondlen( map ) \
}

#define generatehereanim( arrayname, map, xpos, ypos ) \
{ \
	arrayname, \
	arrlen( arrayname ), \
	xpos, \
	ypos, \
	get_herecond( map ), \
	get_herecondlen( map ) \
}

#define generatebackgroundanim( ep, index, xpos, ypos ) \
{ \
	get_frameseq( ep, index ), \
	get_frameseqlen( ep, index ), \
	xpos, \
	ypos, \
	nullptr, \
	0 \
}

#define generateexactmapanim( map, ep, index, xpos, ypos ) \
{ \
	get_frameseq( ep, index ), \
	get_frameseqlen( ep, index ), \
	xpos, \
	ypos, \
	get_exactmapcond( map ), \
	get_exactmapcondlen( map ) \
}

#define generatelocationanims( map, xpos, ypos ) \
generatesplatanim( doom_frames_splat, map, xpos, ypos ), \
generatehereanim( doom_frames_youarehereleft, map, xpos, ypos ), \
generatehereanim( doom_frames_youarehereright, map, xpos, ypos )

// Now for the full declarations

//============================================================================
// Game setup (episodes etc)
//============================================================================

mapinfo_t* doom_maps_episode_1[] =
{
	&doom_map_e1m1, &doom_map_e1m2, &doom_map_e1m3,
	&doom_map_e1m4, &doom_map_e1m5, &doom_map_e1m6,
	&doom_map_e1m7, &doom_map_e1m8, &doom_map_e1m9
};

mapinfo_t* doom_maps_episode_2[] =
{
	&doom_map_e2m1, &doom_map_e2m2, &doom_map_e2m3,
	&doom_map_e2m4, &doom_map_e2m5, &doom_map_e2m6,
	&doom_map_e2m7, &doom_map_e2m8, &doom_map_e2m9
};

episodeinfo_t doom_episode_one =
{
	PlainFlowString( "Knee-Deep In The Dead" ),		// name
	FlowString( "M_EPI1" ),							// name_patch_lump
	1,												// episode_num
	doom_maps_episode_1,							// all_maps
	arrlen( doom_maps_episode_1 ),					// num_maps
	doom_map_e1m9.map_num,							// highest_map_num
	&doom_map_e1m1									// first_map
};

episodeinfo_t doom_shareware_episode_two =
{
	PlainFlowString( "The Shores Of Hell" ),		// name
	FlowString( "M_EPI2" ),							// name_patch_lump
	2,												// episode_num
	nullptr,										// all_maps
	0,												// num_maps
	0,												// highest_map_num
	nullptr											// first_map
};

episodeinfo_t doom_shareware_episode_three =
{
	PlainFlowString( "Inferno" ),					// name
	FlowString( "M_EPI3" ),							// name_patch_lump
	3,												// episode_num
	nullptr,										// all_maps
	0,												// num_maps
	0,												// highest_map_num
	nullptr											// first_map
};

episodeinfo_t doom_episode_two =
{
	PlainFlowString( "The Shores Of Hell" ),		// name
	FlowString( "M_EPI2" ),							// name_patch_lump
	2,												// episode_num
	doom_maps_episode_2,							// all_maps
	arrlen( doom_maps_episode_2 ),					// num_maps
	9,												// highest_map_num
	&doom_map_e2m1									// first_map
};

episodeinfo_t doom_episode_three =
{
	PlainFlowString( "Inferno" ),					// name
	FlowString( "M_EPI3" ),							// name_patch_lump
	3,												// episode_num
	nullptr,										// all_maps
	0,												// num_maps
	9,												// highest_map_num
	nullptr											// first_map
};

episodeinfo_t doom_episode_four =
{
	PlainFlowString( "Thy Flesh Consumed" ),		// name
	FlowString( "M_EPI4" ),							// name_patch_lump
	4,												// episode_num
	nullptr,										// all_maps
	0,												// num_maps
	9,												// highest_map_num
	nullptr											// first_map
};

episodeinfo_t* doom_episodes_shareware[] =
{
	&doom_episode_one,
	&doom_shareware_episode_two,
	&doom_shareware_episode_three
};

episodeinfo_t* doom_episodes_registered[] =
{
	&doom_episode_one,
	&doom_episode_two,
	&doom_episode_three
};

episodeinfo_t* doom_episodes_ultimate[] =
{
	&doom_episode_one,
	&doom_episode_two,
	&doom_episode_three,
	&doom_episode_four
};

gameflow_t doom_shareware =
{
	PlainFlowString( "DOOM Shareware" ),			// name
	doom_episodes_shareware,						// episodes
	arrlen( doom_episodes_shareware ),				// num_episodes
	PlainFlowString( "vanilla" ),					// playsim_base
	PlainFlowString( "" )							// playsim_options
};

gameflow_t doom_registered =
{
	PlainFlowString( "DOOM Registered" ),			// name
	doom_episodes_registered,						// episodes
	arrlen( doom_episodes_registered ),				// num_episodes
	PlainFlowString( "vanilla" ),					// playsim_base
	PlainFlowString( "" )							// playsim_options
};

gameflow_t doom_ultimate =
{
	PlainFlowString( "The Ultimate DOOM" ),			// name
	doom_episodes_ultimate,							// episodes
	arrlen( doom_episodes_ultimate ),				// num_episodes
	PlainFlowString( "vanilla" ),					// playsim_base
	PlainFlowString( "" )							// playsim_options
};

//============================================================================
// Maps
//============================================================================

mapinfo_t doom_map_e1m1 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M1 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom_episode_one,								// episode
	1,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M1" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	30,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e1finished,					// interlevel_finished
	&doom_interlevel_e1entering,					// interlevel_entering
	&doom_map_e1m2,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e1m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e1m2 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M2 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom_episode_one,								// episode
	2,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M2" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	75,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e1finished,					// interlevel_finished
	&doom_interlevel_e1entering,					// interlevel_entering
	&doom_map_e1m3,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e1m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e1m3 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M3 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom_episode_one,								// episode
	3,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M3" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e1finished,					// interlevel_finished
	&doom_interlevel_e1entering,					// interlevel_entering
	&doom_map_e1m4,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e1m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e1m4 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M4 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Hall, John Romero" ),		// authors
	&doom_episode_one,								// episode
	4,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M4" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e1finished,					// interlevel_finished
	&doom_interlevel_e1entering,					// interlevel_entering
	&doom_map_e1m5,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e1m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e1m5 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M5 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom_episode_one,								// episode
	5,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M5" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	165,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e1finished,					// interlevel_finished
	&doom_interlevel_e1entering,					// interlevel_entering
	&doom_map_e1m6,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e1m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e1m6 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M6 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom_episode_one,								// episode
	6,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M6" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	180,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e1finished,					// interlevel_finished
	&doom_interlevel_e1entering,					// interlevel_entering
	&doom_map_e1m7,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e1m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e1m7 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M7 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom_episode_one,								// episode
	7,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M7" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	180,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e1finished,					// interlevel_finished
	&doom_interlevel_e1entering,					// interlevel_entering
	&doom_map_e1m8,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e1m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e1m8 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M8 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Hall, Sandy Petersen" ),	// authors
	&doom_episode_one,								// episode
	8,												// map_num
	Map_Doom1EndOfEpisode,							// map_flags
	FlowString( "D_E1M8" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	30,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e1finished,					// interlevel_finished
	&doom_interlevel_e1entering,					// interlevel_entering
	nullptr,										// next_map
	nullptr,										// next_map_intermission
	nullptr,										// secret_map
	nullptr,										// secret_map_intermission
	&doom_endgame_e1,								// endgame
};

mapinfo_t doom_map_e1m9 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M9 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "John Romero" ),				// authors
	&doom_episode_one,								// episode
	9,												// map_num
	Map_Secret,										// map_flags
	FlowString( "D_E1M9" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	165,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e1finished,					// interlevel_finished
	&doom_interlevel_e1entering,					// interlevel_entering
	&doom_map_e1m4,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e1m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e2m1 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E2M1 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Hall, Sandy Petersen" ),	// authors
	&doom_episode_two,								// episode
	1,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E2M1" ),							// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e2finished,					// interlevel_finished
	&doom_interlevel_e2entering,					// interlevel_entering
	&doom_map_e2m2,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e2m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e2m2 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E2M2 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Hall, Sandy Petersen" ),	// authors
	&doom_episode_two,								// episode
	2,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E2M2" ),							// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e2finished,					// interlevel_finished
	&doom_interlevel_e2entering,					// interlevel_entering
	&doom_map_e2m3,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e2m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e2m3 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E2M3 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Hall, Sandy Petersen" ),	// authors
	&doom_episode_two,								// episode
	3,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E2M3" ),							// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e2finished,					// interlevel_finished
	&doom_interlevel_e2entering,					// interlevel_entering
	&doom_map_e2m4,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e2m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e2m4 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E2M4 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Hall, Sandy Petersen" ),	// authors
	&doom_episode_two,								// episode
	4,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E2M4" ),							// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e2finished,					// interlevel_finished
	&doom_interlevel_e2entering,					// interlevel_entering
	&doom_map_e2m5,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e2m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e2m5 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E2M5 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom_episode_two,								// episode
	5,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E2M5" ),							// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e2finished,					// interlevel_finished
	&doom_interlevel_e2entering,					// interlevel_entering
	&doom_map_e2m6,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e2m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e2m6 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E2M6 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom_episode_two,								// episode
	6,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E2M6" ),							// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	360,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e2finished,					// interlevel_finished
	&doom_interlevel_e2entering,					// interlevel_entering
	&doom_map_e2m7,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e2m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e2m7 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E2M7 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Tom Hall, Sandy Petersen" ),	// authors
	&doom_episode_two,								// episode
	7,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E2M7" ),							// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	240,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e2finished,					// interlevel_finished
	&doom_interlevel_e2entering,					// interlevel_entering
	&doom_map_e2m8,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e2m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t doom_map_e2m8 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E2M8 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom_episode_two,								// episode
	8,												// map_num
	Map_Doom1EndOfEpisode,							// map_flags
	FlowString( "D_E2M8" ),							// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	30,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e2finished,					// interlevel_finished
	&doom_interlevel_e2entering,					// interlevel_entering
	nullptr,										// next_map
	nullptr,										// next_map_intermission
	nullptr,										// secret_map
	nullptr,										// secret_map_intermission
	&doom_endgame_e2,								// endgame
};

mapinfo_t doom_map_e2m9 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E2M9 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Sandy Petersen" ),			// authors
	&doom_episode_two,								// episode
	9,												// map_num
	Map_Secret,										// map_flags
	FlowString( "D_E2M9" ),							// music_lump
	FlowString( "SKY2" ),							// sky_texture
	0,												// sky_scroll_speed
	170,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&doom_interlevel_e2finished,					// interlevel_finished
	&doom_interlevel_e2entering,					// interlevel_entering
	&doom_map_e2m6,									// next_map
	nullptr,										// next_map_intermission
	&doom_map_e2m9,									// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

//============================================================================
// Intermissions, endgames, and interlevels
//============================================================================

//============================================================================
// Knee-deep In The Dead
//============================================================================

intermission_t doom_intermission_e1 =
{
	FlowString( E1TEXT ),							// text
	FlowString( "D_VICTOR" ),						// music_lump
	FlowString( "FLOOR4_8" ),						// background_lump
};

endgame_t doom_endgame_e1 =
{
	EndGame_Pic | EndGame_Ultimate,					// type
	&doom_intermission_e1,							// intermission
	FlowString( "HELP2" ),							// primary_image_lump
	FlowString( "CREDIT" ),							// secondary_image_lump
};

static interlevelanim_t doom_anim_e1_back[] =
{
	generatebackgroundanim( 0, 0, 224, 104 ),
	generatebackgroundanim( 0, 1, 184, 160 ),
	generatebackgroundanim( 0, 2, 112, 136 ),
	generatebackgroundanim( 0, 3, 72, 112 ),
	generatebackgroundanim( 0, 4, 88, 96 ),
	generatebackgroundanim( 0, 5, 64, 48 ),
	generatebackgroundanim( 0, 6, 192, 140 ),
	generatebackgroundanim( 0, 7, 136, 16 ),
	generatebackgroundanim( 0, 8, 80, 16 ),
	generatebackgroundanim( 0, 9, 64, 24 )
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
	Interlevel_Animated,							// type
	RuntimeFlowString( background_format_text ),	// background_lump
	doom_anim_e1_back,								// background_anims
	arrlen( doom_anim_e1_back ),					// num_background_anims
	nullptr,										// foreground_anims
	0,												// num_foreground_anims
};

interlevel_t doom_interlevel_e1entering =
{
	Interlevel_Animated,							// type
	RuntimeFlowString( background_format_text ),	// background_lump
	doom_anim_e1_back,								// background_anims
	arrlen( doom_anim_e1_back ),					// num_background_anims
	doom_anim_e1_fore,								// foreground_anims
	arrlen( doom_anim_e1_fore ),					// num_foreground_anims
};


//============================================================================
// The Shores Of Hell
//============================================================================

intermission_t doom_intermission_e2 =
{
	FlowString( E2TEXT ),							// text
	FlowString( "D_VICTOR" ),						// music_lump
	FlowString( "SFLR6_1" ),						// background_lump
};

endgame_t doom_endgame_e2 =
{
	EndGame_Pic,									// type
	&doom_intermission_e2,							// intermission
	FlowString( "VICTORY2" ),						// primary_image_lump
	nullptr,										// secondary_image_lump
};

static interlevelanim_t doom_anim_e2_finished[] =
{
	generateexactmapanim( E2M1, 1, 0, 128, 136 ),
	generateexactmapanim( E2M2, 1, 1, 128, 136 ),
	generateexactmapanim( E2M3, 1, 2, 128, 136 ),
	generateexactmapanim( E2M4, 1, 3, 128, 136 ),
	generateexactmapanim( E2M5, 1, 4, 128, 136 ),
	generateexactmapanim( E2M6, 1, 5, 128, 136 ),
	generateexactmapanim( E2M7, 1, 6, 128, 136 ),
	generateexactmapanim( E2M8, 1, 6, 128, 136 ),
	generateexactmapanim( E2M9, 1, 4, 128, 136 ),
};

static interlevelanim_t doom_anim_e2_entering[] =
{
	generateexactmapanim( E2M1, 1, 0, 128, 136 ),
	generateexactmapanim( E2M2, 1, 0, 128, 136 ),
	generateexactmapanim( E2M3, 1, 1, 128, 136 ),
	generateexactmapanim( E2M4, 1, 2, 128, 136 ),
	generateexactmapanim( E2M5, 1, 3, 128, 136 ),
	generateexactmapanim( E2M6, 1, 4, 128, 136 ),
	generateexactmapanim( E2M7, 1, 5, 128, 136 ),
	generateexactmapanim( E2M8, 1, 6, 128, 136 ),
	generateexactmapanim( E2M9, 1, 4, 128, 136 ),
	generateexactmapanim( E2M9, 1, 7, 192, 144 ),
	generatelocationanims( E2M1, 254, 25 ),
	generatelocationanims( E2M2, 97, 50 ),
	generatelocationanims( E2M3, 188, 64 ),
	generatelocationanims( E2M4, 128, 78 ),
	generatelocationanims( E2M5, 214, 92 ),
	generatelocationanims( E2M6, 133, 130 ),
	generatelocationanims( E2M7, 208, 136 ),
	generatelocationanims( E2M8, 148, 140 ),
	generatelocationanims( E2M9, 235, 158 ),
};

interlevel_t doom_interlevel_e2finished =
{
	Interlevel_Animated,							// type
	RuntimeFlowString( background_format_text ),	// background_lump
	doom_anim_e2_finished,							// background_anims
	arrlen( doom_anim_e2_finished ),				// num_background_anims
	nullptr,										// foreground_anims
	0,												// num_foreground_anims
};

interlevel_t doom_interlevel_e2entering =
{
	Interlevel_Animated,							// type
	RuntimeFlowString( background_format_text ),	// background_lump
	doom_anim_e2_entering,							// background_anims
	arrlen( doom_anim_e2_entering ),				// num_background_anims
	nullptr,										// foreground_anims
	0,												// num_foreground_anims
};



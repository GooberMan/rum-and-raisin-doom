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
extern episodeinfo_t	chex_episode_one;

extern mapinfo_t		chex_map_e1m1;
extern mapinfo_t		chex_map_e1m2;
extern mapinfo_t		chex_map_e1m3;
extern mapinfo_t		chex_map_e1m4;
extern mapinfo_t		chex_map_e1m5;

extern interlevel_t		chex_interlevel_e1finished;
extern interlevel_t		chex_interlevel_e1entering;

extern intermission_t	chex_intermission_e1;

extern endgame_t		chex_endgame_e1;

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


#define frame( lump, type )								{ FlowString( lump ), type, standardduration }
#define frame_withtime( lump, type, duration )			{ FlowString( lump ), type, duration }
#define frame_runtime( lump, type, index, frame )		{ RuntimeFlowString( lump ), type, standardduration, index, frame }
#define frame_runtime_infinite( lump, index, frame )	{ RuntimeFlowString( lump ), Frame_Infinite, -1, index, frame }

#define generate_name( ep, index ) chex_frames_ ## ep ## _ ## index

#define generate_frameseqstatic( ep, index ) static interlevelframe_t generate_name( ep, index ) [] = { \
	frame_runtime_infinite( frame_format_text, index, 0 ), \
};

#define generate_frameseq3( ep, index ) static interlevelframe_t generate_name( ep, index ) [] = { \
	frame_runtime( frame_format_text, Frame_FixedDuration | Frame_RandomStart, index, 0 ), \
	frame_runtime( frame_format_text, Frame_FixedDuration | Frame_RandomStart, index, 1 ), \
	frame_runtime( frame_format_text, Frame_FixedDuration | Frame_RandomStart, index, 2 ), \
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

static interlevelframe_t chex_frames_splat[] =
{
	frame_withtime( "WISPLAT", Frame_Infinite, -1 )
};

static interlevelframe_t chex_frames_youarehereleft[] =
{
	frame_withtime( "WIURH0", Frame_FixedDuration, 20 ),
	frame_withtime( nullptr, Frame_FixedDuration, 12 ),
};

static interlevelframe_t chex_frames_youarehereright[] =
{
	frame_withtime( "WIURH1", Frame_FixedDuration, 20 ),
	frame_withtime( nullptr, Frame_FixedDuration, 12 ),
};

#define generate_herecond( map, levelnum ) \
static interlevelcond_t chex_herecond_ ## map [] = \
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

#define generate_locationcond( map, levelnum ) \
static interlevelcond_t chex_splatcond_ ## map [] = \
{ \
	{ \
		AnimCondition_MapNumGreater, \
		levelnum \
	} \
}; \
\
generate_herecond( map, levelnum )

#define get_splatcond( map ) chex_splatcond_ ## map
#define get_splatcondlen( map ) arrlen( get_splatcond( map ) )
#define get_herecond( map ) chex_herecond_ ## map
#define get_herecondlen( map ) arrlen( get_herecond( map ) )
#define get_exactmapcond( map ) chex_exactmapcond_ ## map
#define get_exactmapcondlen( map ) arrlen( get_exactmapcond( map ) )

generate_locationcond( E1M1, 1 );
generate_locationcond( E1M2, 2 );
generate_locationcond( E1M3, 3 );
generate_locationcond( E1M4, 4 );
generate_locationcond( E1M5, 5 );

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

#define generatelocationanims( map, xpos, ypos ) \
generatesplatanim( chex_frames_splat, map, xpos, ypos ), \
generatehereanim( chex_frames_youarehereleft, map, xpos, ypos ), \
generatehereanim( chex_frames_youarehereright, map, xpos, ypos )

// Now for the full declarations

//============================================================================
// Game setup (episodes etc)
//============================================================================

mapinfo_t* chex_maps_episode_1[] =
{
	&chex_map_e1m1, &chex_map_e1m2, &chex_map_e1m3,	&chex_map_e1m4, &chex_map_e1m5
};

episodeinfo_t chex_episode_one =
{
	PlainFlowString( "Chex Quest" ),				// name
	FlowString( "M_EPI1" ),							// name_patch_lump
	1,												// episode_num
	chex_maps_episode_1,							// all_maps
	arrlen( chex_maps_episode_1 ),					// num_maps
	chex_map_e1m5.map_num,							// highest_map_num
	&chex_map_e1m1									// first_map
};

episodeinfo_t* chex_episodes[] =
{
	&chex_episode_one,
};

gameflow_t doom_chex =
{
	PlainFlowString( "Chex Quest" ),				// name
	chex_episodes,									// episodes
	arrlen( chex_episodes ),						// num_episodes
	PlainFlowString( "vanilla" ),					// playsim_base
	PlainFlowString( "" )							// playsim_options
};

//============================================================================
// Maps
//============================================================================

mapinfo_t chex_map_e1m1 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M1 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Digital Cafe" ),				// authors
	&chex_episode_one,								// episode
	1,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M1" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	30,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&chex_interlevel_e1finished,					// interlevel_finished
	&chex_interlevel_e1entering,					// interlevel_entering
	&chex_map_e1m2,									// next_map
	nullptr,										// next_map_intermission
	nullptr,										// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t chex_map_e1m2 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M2 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Digital Cafe" ),				// authors
	&chex_episode_one,								// episode
	2,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M2" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	75,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&chex_interlevel_e1finished,					// interlevel_finished
	&chex_interlevel_e1entering,					// interlevel_entering
	&chex_map_e1m3,									// next_map
	nullptr,										// next_map_intermission
	nullptr,										// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t chex_map_e1m3 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M3 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Digital Cafe" ),				// authors
	&chex_episode_one,								// episode
	3,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M3" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	120,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&chex_interlevel_e1finished,					// interlevel_finished
	&chex_interlevel_e1entering,					// interlevel_entering
	&chex_map_e1m4,									// next_map
	nullptr,										// next_map_intermission
	nullptr,										// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t chex_map_e1m4 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M4 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Digital Cafe" ),				// authors
	&chex_episode_one,								// episode
	4,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M4" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	90,												// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&chex_interlevel_e1finished,					// interlevel_finished
	&chex_interlevel_e1entering,					// interlevel_entering
	&chex_map_e1m5,									// next_map
	nullptr,										// next_map_intermission
	nullptr,										// secret_map
	nullptr,										// secret_map_intermission
	nullptr,										// endgame
};

mapinfo_t chex_map_e1m5 =
{
	RuntimeFlowString( levellump_format_text ),		// data_lump
	FlowString( HUSTR_E1M5 ),						// name
	RuntimeFlowString( levename_format_text ),		// name_patch_lump
	PlainFlowString( "Digital Cafe" ),				// authors
	&chex_episode_one,								// episode
	5,												// map_num
	Map_None,										// map_flags
	FlowString( "D_E1M5" ),							// music_lump
	FlowString( "SKY1" ),							// sky_texture
	0,												// sky_scroll_speed
	165,											// par_time
	nullptr,										// boss_actions
	0,												// num_boss_actions
	&chex_interlevel_e1finished,					// interlevel_finished
	&chex_interlevel_e1entering,					// interlevel_entering
	nullptr,										// secret_map
	nullptr,										// next_map_intermission
	nullptr,										// secret_map
	nullptr,										// secret_map_intermission
	&chex_endgame_e1,								// endgame
};

//============================================================================
// Intermissions, endgames, and interlevels
//============================================================================

intermission_t chex_intermission_e1 =
{
	Intermission_None,								// type
	FlowString( E1TEXT ),							// text
	FlowString( "D_VICTOR" ),						// music_lump
	FlowString( "FLOOR4_8" ),						// background_lump
};

endgame_t chex_endgame_e1 =
{
	EndGame_Pic | EndGame_LoopingMusic,				// type
	&chex_intermission_e1,							// intermission
	FlowString( "CREDIT" ),							// primary_image_lump
	EmptyFlowString( ),								// secondary_image_lump
	FlowString( "D_VICTOR" ),						// music_lump
};

static interlevelanim_t chex_anim_e1_back[] =
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

static interlevelanim_t chex_anim_e1_fore[] =
{
	generatelocationanims( E1M1, 185, 164 ),
	generatelocationanims( E1M2, 148, 143 ),
	generatelocationanims( E1M3, 69, 122 ),
	generatelocationanims( E1M4, 209, 102 ),
	generatelocationanims( E1M5, 116, 89 ),
};

interlevel_t chex_interlevel_e1finished =
{
	Interlevel_Animated,							// type
	FlowString( "D_INTER" ),						// music_lump
	RuntimeFlowString( background_format_text ),	// background_lump
	chex_anim_e1_back,								// background_anims
	arrlen( chex_anim_e1_back ),					// num_background_anims
	nullptr,										// foreground_anims
	0,												// num_foreground_anims
};

interlevel_t chex_interlevel_e1entering =
{
	Interlevel_Animated,							// type
	FlowString( "D_INTER" ),						// music_lump
	RuntimeFlowString( background_format_text ),	// background_lump
	chex_anim_e1_back,								// background_anims
	arrlen( chex_anim_e1_back ),					// num_background_anims
	chex_anim_e1_fore,								// foreground_anims
	arrlen( chex_anim_e1_fore ),					// num_foreground_anims
};

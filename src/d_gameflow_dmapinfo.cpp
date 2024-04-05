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
// DESCRIPTION: Sets up gameflow for DMAPINFO lumps

#include "d_gameflow.h"

#include "m_container.h"
#include "m_conv.h"

#include "p_mobj.h"

#include "w_wad.h"

#include <sstream>
#include <iomanip>

namespace dmapinfo
{
	typedef struct map_s
	{
		DoomString		map_lump_name;
		DoomString		map_title;
		DoomString		next_map;
		DoomString		secret_map;
		int32_t			par_time;
		DoomString		music_lump;
		int32_t			episode_number;
		int32_t			map_number;
		int32_t			defined_map_number;
		DoomString		end_sequence;
		DoomString		sky_lump;
		int32_t			sky_scroll_speed;
		bool			map_07_special;
	} map_t;

	typedef struct episode_s
	{
		DoomString		first_map;
		DoomString		name;
	} episode_t;

	typedef struct endsequence_s
	{
		DoomString		id;
		DoomString		text;
		DoomString		flat_lump;
		DoomString		music_lump;
	} endsequence_t;

	static std::vector< map_t >			maps;
	static std::vector< episode_t >		episodes;
	static std::vector< endsequence_t >	endsequences;
}

static void Sanitize( DoomString& currline )
{
	std::replace( currline.begin(), currline.end(), '\t', ' ' );
	currline.erase( std::remove( currline.begin(), currline.end(), '\r' ), currline.end() );
	size_t startpos = currline.find_first_not_of( " " );
	if( startpos != std::string::npos && startpos > 0 )
	{
		currline = currline.substr( startpos );
	}
}

static void ParseMap( DoomStringStream& lumpstream, DoomString& currline )
{
	dmapinfo::map_t& newmap = *dmapinfo::maps.insert( dmapinfo::maps.end(), dmapinfo::map_t() );
	newmap.map_number = -1;

	DoomString lhs;
	DoomString middle;
	DoomString rhs;

	do
	{
		Sanitize( currline );

		DoomIStringStream currlinestream( currline );

		currlinestream >> std::quoted( lhs );
		currlinestream >> std::quoted( middle );
		currlinestream >> std::quoted( rhs );

		if( lhs == "}" )
		{
			break;
		}
		else if( lhs == "map" )
		{
			newmap.map_lump_name = middle;
			newmap.map_title = rhs;

			if( middle.size() == 4
				&& std::toupper( middle[0] ) == 'E'
				&& std::isdigit( middle[1] )
				&& std::toupper( middle[2] ) == 'M'
				&& std::isdigit( middle[3] ) )
			{
				newmap.map_number = middle[3];
			}
			else if( middle.size() == 5
				&& std::toupper( middle[0] ) == 'M'
				&& std::toupper( middle[1] ) == 'A'
				&& std::toupper( middle[2] ) == 'P'
				&& std::isdigit( middle[3] )
				&& std::isdigit( middle[4] ) )
			{
				newmap.map_number = (middle[3] - '0') * 10 + (middle[4] - '0');
			}
		}
		else if( lhs == "next" )
		{
			newmap.next_map = rhs;
		}
		else if( lhs == "secretnext" )
		{
			newmap.secret_map = rhs;
		}
		else if( lhs == "par" )
		{
			newmap.par_time = to< int32_t >( rhs );
		}
		else if( lhs == "music" )
		{
			newmap.music_lump = rhs;
		}
		else if( lhs == "episodenumber" )
		{
			newmap.episode_number = to< int32_t >( rhs );
		}
		else if( lhs == "mapnumber" )
		{
			newmap.defined_map_number = to< int32_t >( rhs );
			if( newmap.map_number == -1 )
			{
				newmap.map_number = newmap.defined_map_number;
			}
		}
		else if( lhs == "endsequence" )
		{
			newmap.end_sequence = rhs;
		}
		else if( lhs == "sky1" )
		{
			newmap.sky_lump = rhs;
			DoomString comma;
			DoomString scroll;
			currlinestream >> std::quoted( comma );
			currlinestream >> std::quoted( scroll );
			if( !scroll.empty() )
			{
				newmap.sky_scroll_speed = to< int32_t >( scroll );
			}
		}
		else if( lhs == "map07special" )
		{
			newmap.map_07_special = true;
		}

	} while( std::getline( lumpstream, currline ) );

}

static void ParseEpisode( DoomStringStream& lumpstream, DoomString& currline )
{
	dmapinfo::episode_t& newep = *dmapinfo::episodes.insert( dmapinfo::episodes.end(), dmapinfo::episode_t() );

	DoomString lhs;
	DoomString middle;
	DoomString rhs;

	do
	{
		Sanitize( currline );

		DoomIStringStream currlinestream( currline );

		currlinestream >> std::quoted( lhs );
		currlinestream >> std::quoted( middle );
		currlinestream >> std::quoted( rhs );

		if( lhs == "}" )
		{
			break;
		}

		if( lhs == "episode" )
		{
			newep.first_map = middle;
		}
		else if( lhs == "name" )
		{
			newep.name = rhs;
		}

	} while( std::getline( lumpstream, currline ) );

}

static void ParseEndSequence( DoomStringStream& lumpstream, DoomString& currline )
{
	dmapinfo::endsequence_t& newseq = *dmapinfo::endsequences.insert( dmapinfo::endsequences.end(), dmapinfo::endsequence_t() );

	DoomString lhs;
	DoomString middle;
	DoomString rhs;

	bool wasparsingtext = false;

	do
	{
		Sanitize( currline );

		DoomIStringStream currlinestream( currline );

		currlinestream >> std::quoted( lhs );
		currlinestream >> std::quoted( middle );
		currlinestream >> std::quoted( rhs );

		if( lhs == "}" )
		{
			break;
		}

		if( wasparsingtext )
		{
			if( currline.find( '\"' ) == 0 )
			{
				newseq.text += "\n";
				newseq.text += lhs;
				continue;
			}
			else
			{
				wasparsingtext = false;
			}
		}

		if( lhs == "endsequence" )
		{
			newseq.id = middle;
		}
		else if( lhs == "text" )
		{
			newseq.text = rhs;
			wasparsingtext = true;
		}
		else if( lhs == "flat" )
		{
			newseq.flat_lump = rhs;
		}
		else if( lhs == "music" )
		{
			newseq.music_lump = rhs;
		}
	} while( std::getline( lumpstream, currline ) );

}

typedef struct dmapinfo_gameinfo_s
{
	std::map< int32_t, episodeinfo_t >				episodes;
	std::map< DoomString, mapinfo_t >					maps;

	std::map< int32_t, std::vector< mapinfo_t* > >	episodemaps;

	std::map< DoomString, endgame_t >					endgames;
	std::map< DoomString, intermission_t >			intermissions;

	std::vector< episodeinfo_t* >								episodelist;

	gameflow_t													game;

} dmapinfo_gameinfo_t;

static bossaction_t map07special[] =
{
	{ 8 /*MT_FATSO*/, 0x00400 /* MF2_MBF21_MAP07BOSS1 */, 82 /*lowerFloorToLowest repeatable*/, 666 },
	{ 20 /*MT_BABY*/, 0x00800 /* MF2_MBF21_MAP07BOSS2 */, 96 /*raiseToTexture repeatable*/, 667 },
};

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

constexpr auto FlowString( DoomString& name, flowstringflags_t flags = FlowString_None )
{
	flowstring_t str = { name.c_str(), flags };
	return str;
}

static dmapinfo_gameinfo_t		dmapinfogame;

static void BuildNewGameInfo()
{
	for( auto& sourcemap : dmapinfo::maps )
	{
		auto& episodemaps		= dmapinfogame.episodemaps[ sourcemap.episode_number ];
		auto& targetmap			= dmapinfogame.maps[ sourcemap.map_lump_name ];
		auto& targetepisode		= dmapinfogame.episodes[ sourcemap.episode_number ];

		mapinfo_t* original_map = D_GameflowGetMap( D_GameflowGetEpisode( sourcemap.episode_number ), sourcemap.map_number );
		if( original_map != nullptr )
		{
			targetmap = *original_map;
		}
		targetepisode.episode_num = sourcemap.episode_number;

		episodemaps.push_back( &targetmap );

		targetmap.data_lump				= FlowString( sourcemap.map_lump_name );
		targetmap.name					= FlowString( sourcemap.map_title );
		// targetmap.name_patch_lump
		targetmap.authors				= EmptyFlowString();
		targetmap.episode				= &targetepisode;
		targetmap.map_num				= sourcemap.map_number;
		 // targetmap.map_flags;
		if( !sourcemap.music_lump.empty() )		targetmap.music_lump[ 0 ]	= FlowString( sourcemap.music_lump );
		if( !sourcemap.sky_lump.empty() )		targetmap.sky_texture	= FlowString( sourcemap.sky_lump );
		targetmap.sky_scroll_speed		= sourcemap.sky_scroll_speed;
		targetmap.par_time				= sourcemap.par_time;

		targetmap.next_map				= sourcemap.next_map.empty() ? nullptr : &dmapinfogame.maps[ sourcemap.next_map ];
		targetmap.secret_map			= sourcemap.secret_map.empty() ? nullptr : &dmapinfogame.maps[ sourcemap.secret_map ];
		if( targetmap.secret_map && targetmap.secret_map != targetmap.next_map )
		{
			targetmap.secret_map->map_flags = targetmap.map_flags | Map_Secret;
		}
		// targetmap.secret_map_intermission;

		if( sourcemap.map_07_special )
		{
			targetmap.boss_actions = map07special;
			targetmap.num_boss_actions = arrlen( map07special );
		}

		if( !original_map )
		{
			mapinfo_t* backupmap = nullptr;
			episodeinfo_t* backupep = D_GameflowGetEpisode( sourcemap.episode_number );
			if( !backupep )
			{
				backupep = current_game->episodes[ 0 ];
			}

			backupmap = backupep->first_map;

			targetmap.interlevel_finished = backupmap->interlevel_finished;
			targetmap.interlevel_entering = backupmap->interlevel_entering;
		}

		if( !sourcemap.end_sequence.empty() )
		{
			if( targetmap.endgame != nullptr )
			{
				auto& targetendgame = dmapinfogame.endgames[ sourcemap.end_sequence ];
				targetendgame = *targetmap.endgame;
				targetendgame.intermission = &dmapinfogame.intermissions[ sourcemap.end_sequence ];
				targetmap.endgame = &targetendgame;
			}
			else
			{
				targetmap.next_map_intermission = &dmapinfogame.intermissions[ sourcemap.end_sequence ];
			}
		}
	}

	for( auto& sourceep : dmapinfo::episodes )
	{
		mapinfo_t& firstmap =  dmapinfogame.maps[ sourceep.first_map ];

		auto& targetep = dmapinfogame.episodes[ firstmap.episode->episode_num ];
		
		episodeinfo_t* original_ep = D_GameflowGetEpisode( targetep.episode_num );
		if( original_ep != nullptr )
		{
			targetep = *original_ep;
		}

		targetep.name = FlowString( sourceep.name );
		// targetep.name_patch_lump
		// targetep.episode_num;
		targetep.all_maps = dmapinfogame.episodemaps[ targetep.episode_num ].data();
		targetep.num_maps = dmapinfogame.episodemaps[ targetep.episode_num ].size();
		targetep.highest_map_num = 0;
		targetep.first_map = &firstmap;

		for( auto& testmap : dmapinfogame.episodemaps[ targetep.episode_num ] )
		{
			targetep.highest_map_num = M_MAX( targetep.highest_map_num, testmap->map_num );
		}

		dmapinfogame.episodelist.push_back( &targetep );
	}

	for( auto& sourceend : dmapinfo::endsequences )
	{
		intermission_t& targetend = dmapinfogame.intermissions[ sourceend.id ];
		targetend.text = FlowString( sourceend.text );
		targetend.background_lump = FlowString( sourceend.flat_lump );
		targetend.music_lump = FlowString( sourceend.music_lump );
	}

	dmapinfogame.game = *current_game;
	dmapinfogame.game.episodes = dmapinfogame.episodelist.data();
	dmapinfogame.game.num_episodes = dmapinfogame.episodelist.size();

	current_game = &dmapinfogame.game;
	current_episode = current_game->episodes[ 0 ];
	current_map = current_episode->first_map;
}

void D_GameflowParseDMAPINFO( int32_t lumpnum )
{
	DoomString lumptext( (size_t)W_LumpLength( lumpnum ) + 1, 0 );
	W_ReadLump( lumpnum, (void*)lumptext.c_str() );
	DoomStringStream lumpstream( lumptext );
	DoomString currline;

	while( std::getline( lumpstream, currline ) )
	{
		Sanitize( currline );
		if( currline.find( "map", 0 ) == 0 )
		{
			ParseMap( lumpstream, currline );
		}
		else if( currline.find( "episode" ) == 0 )
		{
			ParseEpisode( lumpstream, currline );
		}
		else if( currline.find( "endsequence" ) == 0 )
		{
			ParseEndSequence( lumpstream, currline );
		}
	}

	BuildNewGameInfo();
}

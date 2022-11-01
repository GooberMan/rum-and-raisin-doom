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

static void ParseMap( std::stringstream& lumpstream, DoomString& currline )
{
	dmapinfo::map_t& newmap = *dmapinfo::maps.insert( dmapinfo::maps.end(), dmapinfo::map_t() );

	DoomString lhs;
	DoomString middle;
	DoomString rhs;

	do
	{
		Sanitize( currline );

		std::istringstream currlinestream( currline );

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
			newmap.par_time = std::stoi( rhs );
		}
		else if( lhs == "music" )
		{
			newmap.music_lump = rhs;
		}
		else if( lhs == "episodenumber" )
		{
			newmap.episode_number = std::stoi( rhs );
		}
		else if( lhs == "mapnumber" )
		{
			newmap.map_number = std::stoi( rhs );
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
				newmap.sky_scroll_speed = std::stoi( scroll );
			}
		}
		else if( lhs == "map07special" )
		{
			newmap.map_07_special = true;
		}

	} while( std::getline( lumpstream, currline ) );

}

static void ParseEpisode( std::stringstream& lumpstream, DoomString& currline )
{
	dmapinfo::episode_t& newep = *dmapinfo::episodes.insert( dmapinfo::episodes.end(), dmapinfo::episode_t() );

	DoomString lhs;
	DoomString middle;
	DoomString rhs;

	do
	{
		Sanitize( currline );

		std::istringstream currlinestream( currline );

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

static void ParseEndSequence( std::stringstream& lumpstream, DoomString& currline )
{
	dmapinfo::endsequence_t& newseq = *dmapinfo::endsequences.insert( dmapinfo::endsequences.end(), dmapinfo::endsequence_t() );

	DoomString lhs;
	DoomString middle;
	DoomString rhs;

	bool wasparsingtext = false;

	do
	{
		Sanitize( currline );

		std::istringstream currlinestream( currline );

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

		if( lhs == "id" )
		{
			newseq.id = rhs;
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
	std::unordered_map< int32_t, episodeinfo_t >				episodes;
	std::unordered_map< DoomString, mapinfo_t >					maps;

	std::unordered_map< int32_t, std::vector< mapinfo_t* > >	episodemaps;

	std::vector< episodeinfo_t* >								episodelist;

	gameflow_t													game;

} dmapinfo_gameinfo_t;

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
		if( !sourcemap.music_lump.empty() )		targetmap.music_lump	= FlowString( sourcemap.music_lump );
		if( !sourcemap.sky_lump.empty() )		targetmap.sky_texture	= FlowString( sourcemap.sky_lump );
		targetmap.sky_scroll_speed		= sourcemap.sky_scroll_speed;
		targetmap.par_time				= sourcemap.par_time;

		targetmap.next_map				= sourcemap.next_map.empty() ? nullptr : &dmapinfogame.maps[ sourcemap.next_map ];
		targetmap.secret_map			= sourcemap.secret_map.empty() ? nullptr : &dmapinfogame.maps[ sourcemap.secret_map ];
		// targetmap.secret_map_intermission;
		// targetmap.endgame;

		if( sourcemap.map_07_special )
		{
			//targetmap.boss_actions;
			//targetmap.num_boss_actions;
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
			// targetmap.next_map_intermission;
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
	std::stringstream lumpstream( lumptext );
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

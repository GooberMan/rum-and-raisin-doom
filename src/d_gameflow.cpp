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
// DESCRIPTION: Manages the current game

#include "d_gameflow.h"

#include "i_terminal.h"
#include "m_container.h"
#include "w_wad.h"

gameflow_t*		current_game		= nullptr;
episodeinfo_t*	current_episode		= nullptr;
mapinfo_t*		current_map			= nullptr;

static std::map< DoomString, mapinfo_t* > allmaps;
static std::vector< const char* > musinfostorage;

inline auto Episodes()
{
	return std::span( current_game->episodes, current_game->num_episodes );
}

inline auto Maps( episodeinfo_t* episode )
{
	return std::span( episode->all_maps, episode->num_maps );
}

void D_GameflowParseUMAPINFO( int32_t lumpnum );
void D_GameflowParseDMAPINFO( int32_t lumpnum );

episodeinfo_t* D_GameflowGetEpisode( int32_t episodenum )
{
	for( auto& episode : D_GetEpisodes() )
	{
		if( episode->episode_num == episodenum )
		{
			return episode;
		}
	}

	return nullptr;
}

mapinfo_t* D_GameflowGetMap( episodeinfo_t* episode, int32_t mapnum )
{
	if( episode != nullptr )
	{
		for( auto& map : D_GetMapsFor( episode ) )
		{
			if( map->map_num == mapnum )
			{
				return map;
			}
		}
	}

	return nullptr;
}

void D_GameflowSetCurrentEpisode( episodeinfo_t* episode )
{
	current_episode = episode;
	current_map = episode->first_map;
}

void D_GameflowSetCurrentMap( mapinfo_t* map )
{
	current_episode = map->episode;
	current_map = map;
}

constexpr auto PlainFlowString( const char* name )
{
	flowstring_t lump = { name, FlowString_None };
	return lump;
}

static void Sanitize( DoomString& currline )
{
	std::replace( currline.begin(), currline.end(), '\t', ' ' );
	currline.erase( std::remove( currline.begin(), currline.end(), '\r' ), currline.end() );
	size_t startpos = currline.find_first_not_of( " " );
	if( startpos != DoomString::npos && startpos > 0 )
	{
		currline = currline.substr( startpos );
	}
}

void D_GameflowParseMUSINFO( lumpindex_t lumpnum )
{
	DoomString lumptext( (size_t)W_LumpLength( lumpnum ) + 1, 0 );
	W_ReadLump( lumpnum, (void*)lumptext.c_str() );

	DoomStringStream lumpstream( lumptext );
	DoomString currline;

	mapinfo_t* currmap = nullptr;

	while( std::getline( lumpstream, currline ) )
	{
		Sanitize( currline );
		auto found = allmaps.find( currline );
		if( found != allmaps.end() )
		{
			currmap = found->second;
		}
		else if( currmap )
		{
			int32_t index = -1;
			char lumpname[ 16 ] = {};
			if( sscanf( currline.c_str(), "%d %8s", &index, lumpname ) == 2
				&& index > 0
				&& index <= 64 )
			{
				char* copy = (char*)Z_MallocZero( 16, PU_STATIC, nullptr );
				M_StringCopy( copy, lumpname, 16 );
				auto inserted = musinfostorage.insert( musinfostorage.end(), copy );
				currmap->music_lump[ index ] = PlainFlowString( copy );
			}
		}
	}
}

void D_GameflowCheckAndParseMapinfos( void )
{
	lumpindex_t lumpnum = -1;
	if( ( lumpnum = W_CheckNumForName( "UMAPINFO" ) ) >= 0 )
	{
		D_GameflowParseUMAPINFO( lumpnum );
		I_TerminalPrintf( Log_Startup, " UMAPINFO gameflow defined\n" );
	}
	else if( ( lumpnum = W_CheckNumForName( "DMAPINFO" ) ) >= 0 )
	{
		D_GameflowParseDMAPINFO( lumpnum );
		I_TerminalPrintf( Log_Startup, " DMAPINFO gameflow defined\n" );
	}
	else if( ( lumpnum = W_CheckNumForName( "ZMAPINFO" ) ) >= 0 )
	{
		//D_GameflowParseZMAPINFO( lumpnum );
		I_TerminalPrintf( Log_Error, " ZMAPINFO gameflow unimplemented, retaining vanilla gameflow\n" );
	}
	else if( ( lumpnum = W_CheckNumForName( "MAPINFO" ) ) >= 0 )
	{
		//D_GameflowParseMAPINFO( lumpnum );
		I_TerminalPrintf( Log_Error, " MAPINFO gameflow unimplemented, retaining vanilla gameflow\n" );
	}

	for( episodeinfo_t* episode : Episodes() )
	{
		for( mapinfo_t* map : Maps( episode ) )
		{
			DoomString maplump = AsDoomString( map->data_lump, map->episode->episode_num, map->map_num );
			allmaps[ maplump ] = map;
		}
	}

	if( ( lumpnum = W_CheckNumForName( "MUSINFO" ) ) >= 0 )
	{
		D_GameflowParseMUSINFO( lumpnum );
	}
}

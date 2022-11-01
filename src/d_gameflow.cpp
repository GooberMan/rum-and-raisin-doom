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

#include "m_container.h"
#include "w_wad.h"

gameflow_t*		current_game		= nullptr;
episodeinfo_t*	current_episode		= nullptr;
mapinfo_t*		current_map			= nullptr;

auto Episodes()
{
	return std::span( current_game->episodes, current_game->num_episodes );
}

auto Maps( episodeinfo_t* episode )
{
	return std::span( episode->all_maps, episode->num_maps );
}

episodeinfo_t* D_GameflowGetEpisode( int32_t episodenum )
{
	for( auto& episode : Episodes() )
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
		for( auto& map : Maps( episode ) )
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

void D_GameflowParseDMAPINFO( int32_t lumpnum );

void D_GameflowCheckAndParseMapinfos( void )
{
	int32_t lumpnum = -1;
	if( ( lumpnum = W_GetNumForName( "DMAPINFO" ) ) >= 0 )
	{
		D_GameflowParseDMAPINFO( lumpnum );
	}
}

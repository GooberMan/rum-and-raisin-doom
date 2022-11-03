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
// DESCRIPTION: Sets up the active game

#include "m_fixed.h"
#include "d_playsim.h"
#include "doomstat.h"
#include "i_system.h"

static bossaction_t doom_bossspecial_pre_version_1_9[] =
{
	{ -1 /*any*/, 52 /*exitlevel*/, 0 }
};

extern mapinfo_t		doom_map_e2m8;
extern mapinfo_t		doom_map_e3m8;

static void SetGame( gameflow_t& game )
{
	current_game = &game;
	current_episode = game.episodes[ 0 ];
	current_map = game.episodes[ 0 ]->first_map;
}

void D_RegisterPlaysim()
{
	// Dirty hack to retain compatibility with older maps
	if( gameversion < exe_ultimate )
	{
		doom_map_e2m8.boss_actions = doom_map_e3m8.boss_actions = doom_bossspecial_pre_version_1_9;
		doom_map_e2m8.num_boss_actions = doom_map_e3m8.num_boss_actions = arrlen( doom_bossspecial_pre_version_1_9 );
	}

	switch( gamemission )
	{
	case doom:
		switch( gamemode )
		{
		case registered:
			SetGame( doom_registered );
			break;
		case retail:
			SetGame( doom_ultimate );
			break;
		case shareware:
		default:
			SetGame( doom_shareware );
			break;
		}
		break;
	case doom2:
		SetGame( doom_2 );
		break;
	case pack_tnt:
		SetGame( doom_tnt );
		break;
	case pack_plut:
		SetGame( doom_plutonia );
		break;
	case pack_chex:
		SetGame( doom_chex );
		break;
	case pack_hacx:
		SetGame( doom_hacx );
		break;
	default:
		I_Error( "Unsupported game configuration" );
		break;
	}
}
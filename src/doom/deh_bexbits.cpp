//
// Copyright(C) 2005-2014 Simon Howard
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
//
// Parses BEX-formatted Action Pointer entries in dehacked files
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "info.h"

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"

#include "d_items.h"
#include "d_mode.h"

#include "m_container.h"

struct flag_t
{
	uint32_t		flag;
	GameVersion_t	minimum_version;
};

using flagsmap_t = std::map< std::string, flag_t >;

static flagsmap_t ThingBitFlags =
{
	{ "SPECIAL",		{ 0x00000001,	exe_doom_1_2	} },
	{ "SOLID",			{ 0x00000002,	exe_doom_1_2	} },
	{ "SHOOTABLE",		{ 0x00000004,	exe_doom_1_2	} },
	{ "NOSECTOR",		{ 0x00000008,	exe_doom_1_2	} },
	{ "NOBLOCKMAP",		{ 0x00000010,	exe_doom_1_2	} },
	{ "AMBUSH",			{ 0x00000020,	exe_doom_1_2	} },
	{ "JUSTHIT",		{ 0x00000040,	exe_doom_1_2	} },
	{ "JUSTATTACKED",	{ 0x00000080,	exe_doom_1_2	} },
	{ "SPAWNCEILING",	{ 0x00000100,	exe_doom_1_2	} },
	{ "NOGRAVITY",		{ 0x00000200,	exe_doom_1_2	} },
	{ "DROPOFF",		{ 0x00000400,	exe_doom_1_2	} },
	{ "PICKUP",			{ 0x00000800,	exe_doom_1_2	} },
	{ "NOCLIP",			{ 0x00001000,	exe_doom_1_2	} },
	{ "SLIDE",			{ 0x00002000,	exe_doom_1_2	} },
	{ "FLOAT",			{ 0x00004000,	exe_doom_1_2	} },
	{ "TELEPORT",		{ 0x00008000,	exe_doom_1_2	} },
	{ "MISSILE",		{ 0x00010000,	exe_doom_1_2	} },
	{ "DROPPED",		{ 0x00020000,	exe_doom_1_2	} },
	{ "SHADOW",			{ 0x00040000,	exe_doom_1_2	} },
	{ "NOBLOOD",		{ 0x00080000,	exe_doom_1_2	} },
	{ "CORPSE",			{ 0x00100000,	exe_doom_1_2	} },
	{ "INFLOAT",		{ 0x00200000,	exe_doom_1_2	} },
	{ "COUNTKILL",		{ 0x00400000,	exe_doom_1_2	} },
	{ "COUNTITEM",		{ 0x00800000,	exe_doom_1_2	} },
	{ "SKULLFLY",		{ 0x01000000,	exe_doom_1_2	} },
	{ "NOTDMATCH",		{ 0x02000000,	exe_doom_1_2	} },
	{ "TRANSLATION",	{ 0x04000000,	exe_doom_1_2	} },
	{ "UNUSED1",		{ 0x08000000,	exe_doom_1_2	} },
	{ "UNUSED2",		{ 0x10000000,	exe_doom_1_2	} },
	{ "UNUSED3",		{ 0x20000000,	exe_doom_1_2	} },
	{ "UNUSED4",		{ 0x40000000,	exe_doom_1_2	} },
	{ "TRANSLUCENT",	{ 0x80000000,	exe_boom_2_02	} },
	{ "TOUCHY",			{ 0x10000000,	exe_mbf			} },
	{ "BOUNCES",		{ 0x20000000,	exe_mbf			} },
	{ "FRIEND",			{ 0x40000000,	exe_mbf			} },
	{ "TRANSLATION1",	{ 0x04000000,	exe_mbf			} },
	{ "TRANSLATION2",	{ 0x08000000,	exe_mbf			} },
};

static flagsmap_t ThingBitFlags2 =
{
	{ "LOGRAV",			{ 0x00000001,	exe_mbf21		} },
	{ "SHORTMRANGE",	{ 0x00000002,	exe_mbf21		} },
	{ "DMGIGNORED",		{ 0x00000004,	exe_mbf21		} },
	{ "NORADIUSDMG",	{ 0x00000008,	exe_mbf21		} },
	{ "FORCERADIUSDMG",	{ 0x00000010,	exe_mbf21		} },
	{ "HIGHERMPROB",	{ 0x00000020,	exe_mbf21		} },
	{ "RANGEHALF",		{ 0x00000040,	exe_mbf21		} },
	{ "NOTHRESHOLD",	{ 0x00000080,	exe_mbf21		} },
	{ "LONGMELEE",		{ 0x00000100,	exe_mbf21		} },
	{ "BOSS",			{ 0x00000200,	exe_mbf21		} },
	{ "MAP07BOSS1",		{ 0x00000400,	exe_mbf21		} },
	{ "MAP07BOSS2",		{ 0x00000800,	exe_mbf21		} },
	{ "E1M8BOSS",		{ 0x00001000,	exe_mbf21		} },
	{ "E2M8BOSS",		{ 0x00002000,	exe_mbf21		} },
	{ "E3M8BOSS",		{ 0x00004000,	exe_mbf21		} },
	{ "E4M6BOSS",		{ 0x00008000,	exe_mbf21		} },
	{ "E4M8BOSS",		{ 0x00010000,	exe_mbf21		} },
	{ "RIP",			{ 0x00020000,	exe_mbf21		} },
	{ "FULLVOLSOUNDS",	{ 0x00040000,	exe_mbf21		} },
};

static flagsmap_t FrameBitFlagsMBF21 =
{
	{ "SKILL5FAST",		{ 0x00000001,	exe_mbf21		} },
};

static flagsmap_t WeaponBitFlagsMBF21 =
{
	{ "NOTHRUST",		{ 0x00000001,	exe_mbf21		} },
	{ "SILENT",			{ 0x00000002,	exe_mbf21		} },
	{ "NOAUTOFIRE",		{ 0x00000004,	exe_mbf21		} },
	{ "FLEEMELEE",		{ 0x00000008,	exe_mbf21		} },
	{ "AUTOSWITCHFROM",	{ 0x00000010,	exe_mbf21		} },
	{ "NOAUTOSWITCHTO",	{ 0x00000020,	exe_mbf21		} },
};

static int32_t GetFlagsFrom( flagsmap_t& flagsmap, deh_context_t* context, const char* value )
{
	std::string test;
	test.reserve( 32 );

	const char* start = value;
	const char* end = value;

	int32_t newflags = 0;

	while( true )
	{
		if( start != end
			&& ( *end == '+'
				|| *end == '|'
				|| *end == ' '
				|| *end == 0 ) )
		{
			test.clear();
			test.insert( 0, start, end - start );
			
			auto found = ThingBitFlags.find( test );
			if( found == ThingBitFlags.end() )
			{
				DEH_Warning( context, "Flag %s is invalid", test.c_str() );
			}
			else
			{
				newflags |= *(int32_t*)&found->second.flag;
				DEH_IncreaseGameVersion( context, found->second.minimum_version );
			}

			start = end + 1;
		}
		else if( start == end
				&& ( *end == ' '
					|| *end == '|'
					|| *end == '+' ) )
		{
			start = end + 1;
		}

		if( *end != 0 )
		{
			++end;
		}
		else
		{
			break;
		}
	}

	return newflags;
}

DOOM_C_API void DEH_BexHandleThingBits( deh_context_t* context, const char* value, mobjinfo_t* mobj )
{
	mobj->flags = GetFlagsFrom( ThingBitFlags, context, value );
}

DOOM_C_API void DEH_BexHandleThingBits2( deh_context_t* context, const char* value, mobjinfo_t* mobj )
{
	mobj->flags2 = GetFlagsFrom( ThingBitFlags2, context, value );
}

DOOM_C_API void DEH_BexHandleFrameBitsMBF21( deh_context_t* context, const char* value, state_t* state )
{
	state->mbf21flags = GetFlagsFrom( FrameBitFlagsMBF21, context, value );
}

DOOM_C_API void DEH_BexHandleWeaponBitsMBF21( deh_context_t* context, const char* value, weaponinfo_t* weapon )
{
	weapon->mbf21flags = GetFlagsFrom( WeaponBitFlagsMBF21, context, value );
}

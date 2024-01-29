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

#include "m_container.h"

using flagsmap_t = std::map< DoomString, int32_t >;

static  flagsmap_t ThingBitFlags =
{
	{ "SPECIAL",		0x00000001	},
	{ "SOLID",			0x00000002	},
	{ "SHOOTABLE",		0x00000004	},
	{ "NOSECTOR",		0x00000008	},
	{ "NOBLOCKMAP",		0x00000010	},
	{ "AMBUSH",			0x00000020	},
	{ "JUSTHIT",		0x00000040	},
	{ "JUSTATTACKED",	0x00000080	},
	{ "SPAWNCEILING",	0x00000100	},
	{ "NOGRAVITY",		0x00000200	},
	{ "DROPOFF",		0x00000400	},
	{ "PICKUP",			0x00000800	},
	{ "NOCLIP",			0x00001000	},
	{ "SLIDE",			0x00002000	},
	{ "FLOAT",			0x00004000	},
	{ "TELEPORT",		0x00008000	},
	{ "MISSILE",		0x00010000	},
	{ "DROPPED",		0x00020000	},
	{ "SHADOW",			0x00040000	},
	{ "NOBLOOD",		0x00080000	},
	{ "CORPSE",			0x00100000	},
	{ "INFLOAT",		0x00200000	},
	{ "COUNTKILL",		0x00400000	},
	{ "COUNTITEM",		0x00800000	},
	{ "SKULLFLY",		0x01000000	},
	{ "NOTDMATCH",		0x02000000	},
	{ "TRANSLATION",	0x04000000	},
	{ "UNUSED1",		0x08000000	},
	{ "UNUSED2",		0x10000000	},
	{ "UNUSED3",		0x20000000	},
	{ "UNUSED4",		0x40000000	},
	{ "TRANSLUCENT",	0x80000000	},
};

static flagsmap_t ThingBitFlags2 =
{
	{ "LOGRAV",			0x00000001	},
	{ "SHORTMRANGE",	0x00000002	},
	{ "DMGIGNORED",		0x00000004	},
	{ "NORADIUSDMG",	0x00000008	},
	{ "FORCERADIUSDMG",	0x00000010	},
	{ "HIGHERMPROB",	0x00000020	},
	{ "RANGEHALF",		0x00000040	},
	{ "NOTHRESHOLD",	0x00000080	},
	{ "LONGMELEE",		0x00000100	},
	{ "BOSS",			0x00000200	},
	{ "MAP07BOSS1",		0x00000400	},
	{ "MAP07BOSS2",		0x00000800	},
	{ "E1M8BOSS",		0x00001000	},
	{ "E2M8BOSS",		0x00002000	},
	{ "E3M8BOSS",		0x00004000	},
	{ "E4M6BOSS",		0x00008000	},
	{ "E4M8BOSS",		0x00010000	},
	{ "RIP",			0x00020000	},
	{ "FULLVOLSOUNDS",	0x00040000	},
};

static int32_t GetFlagsFrom( flagsmap_t& flagsmap, deh_context_t* context, const char* value )
{
	DoomString test;
	test.reserve( 16 );

	const char* start = value;
	const char* end = value;

	int32_t newflags = 0;

	while( true )
	{
		if( *end == '+' || *end == 0 )
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
				newflags |= found->second;
			}

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
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
// BEX format part times
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

static std::map< int32_t, int32_t > ParTimes;

INLINE int32_t MakeParIndex( int32_t episode, int32_t map )
{
	return ( ( episode & 0xFF ) << 8 ) | ( map & 0xFF );
}

static void *DEH_BEXParsStart( deh_context_t* context, char* line )
{
	char section[ 7 ];

	if ( sscanf( line, "%6s", section ) == 0 || strncmp( "[PARS]", section, sizeof( section ) ) )
	{
		DEH_Warning(context, "Parse error on PARS start");
	}

	return NULL;
}

static void DEH_BEXParsParseLine( deh_context_t *context, char* line, void* tag )
{
	int32_t param1 = 0;
	int32_t param2 = 0;
	int32_t param3 = 0;

	int32_t found = sscanf( line, "par %d %d %d", &param1, &param2, &param3 );
	if( found == 2 )
	{
		ParTimes[ MakeParIndex( 1, param1 ) ] = param2;
	}
	else if( found == 3 )
	{
		ParTimes[ MakeParIndex( param1, param2 ) ] = param3;
	}
	else
	{
		DEH_Warning( context, "Malformed par line" );
	}
}

DOOM_C_API int32_t DEH_ParTime( mapinfo_t* map )
{
	auto found = ParTimes.find( MakeParIndex( map->episode->episode_num, map->map_num ) );
	if( found != ParTimes.end() )
	{
		return found->second;
	}
	return map->par_time;
}

deh_section_t deh_section_bexpars =
{
	"[PARS]",
	NULL,
	DEH_BEXParsStart,
	DEH_BEXParsParseLine,
	NULL,
	NULL
};


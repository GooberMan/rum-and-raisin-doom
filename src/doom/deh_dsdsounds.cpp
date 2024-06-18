//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020-2024 Ethan Watson
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
// DEHEXTRA and beyond sounds
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
#include "m_conv.h"

extern std::unordered_map< int32_t, sfxinfo_t* > sfxmap;
sfxinfo_t* DEH_GetSound(deh_context_t* context, int32_t sound_number );

enum
{
	NumVanillaSounds = 109,
	NumMBFSounds = 114,
	MinDehextra = 500,
	MaxDehextra = 699,
};

static GameVersion_t VersionFromSoundNumber( int32_t num )
{
	return num < 0 ? exe_mbf21_extended
		: num < NumVanillaSounds ? exe_doom_1_2
		: num < NumMBFSounds ? exe_boom_2_02
		: num >= MinDehextra && num <= MaxDehextra ? exe_mbf_dehextra
		: exe_mbf21_extended;
}

static void *DEH_DSDSoundsStart( deh_context_t* context, char* line )
{
	char section[ 16 ] = {};

	if ( sscanf( line, "%8s", section ) == 0 || strncmp( "[SOUNDS]", section, 8 ) )
	{
		DEH_Warning(context, "Parse error on SOUNDS start");
	}

	return NULL;
}

static void DEH_DSDSoundsParseLine( deh_context_t *context, char* line, void* tag )
{
	char *soundnum, *value;

	if (!DEH_ParseAssignment(line, &soundnum, &value))
	{
		DEH_Warning(context, "Failed to parse sprite assignment");
		return;
	}

	sfxinfo_t* sfx = nullptr;
	if( IsNumber( value ) )
	{
		int32_t soundindex = atoi( soundnum );

		sfx = DEH_GetSound( context, soundindex );
	}
	else
	{
		auto found = std::find_if( sfxmap.begin(), sfxmap.end(), [&value]( const std::unordered_map< int32_t, sfxinfo_t* >::value_type& info )
		{
			return strcasecmp( info.second->name, value ) == 0;
		} );
		if( found == sfxmap.end() )
		{
			DEH_Warning( context, "Sound lump '%s' not a built-in lump", value );
			return;
		}
		sfx = found->second;
	}

	strncpy( sfx->name, value, 6 );
}

deh_section_t deh_section_dsdsounds =
{
	"[SOUNDS]",
	NULL,
	DEH_DSDSoundsStart,
	DEH_DSDSoundsParseLine,
	NULL,
	NULL
};


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
// Parses "Sound" sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>

#include "doomtype.h"
#include "deh_defs.h"
#include "deh_main.h"
#include "deh_mapping.h"
#include "sounds.h"

DEH_BEGIN_MAPPING(sound_mapping, sfxinfo_t)
    DEH_UNSUPPORTED_MAPPING("Offset")
    DEH_UNSUPPORTED_MAPPING("Zero/One")
    DEH_MAPPING("Value", priority)
    DEH_MAPPING("Zero 1", link)
    DEH_MAPPING("Zero 2", pitch)
    DEH_MAPPING("Zero 3", volume)
    DEH_UNSUPPORTED_MAPPING("Zero 4")
    DEH_MAPPING("Neg. One 1", usefulness)
    DEH_MAPPING("Neg. One 2", lumpnum)
DEH_END_MAPPING

extern std::unordered_map< int32_t, sfxinfo_t* > sfxmap;

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

static void *DEH_SoundStart(deh_context_t *context, char *line)
{
    int sound_number = 0;
    
    if (sscanf(line, "Sound %i", &sound_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

	DEH_IncreaseGameVersion( context, VersionFromSoundNumber( sound_number ) );

	auto found = sfxmap.find( sound_number );
	{
		if( found == sfxmap.end() )
		{
			sfxinfo_t* sfx = Z_MallocAs( sfxinfo_t, PU_STATIC, nullptr );
			*sfx = {};
			sfx->priority = 127;
			sfx->pitch = -1;
			sfx->volume = -1;
			sfxmap[ sound_number ] = sfx;
			return sfx;
		}
	}

    return found->second;
}

static void DEH_SoundParseLine(deh_context_t *context, char *line, void *tag)
{
    sfxinfo_t *sfx;
    char *variable_name, *value;
    int ivalue;
    
    if (tag == NULL)
       return;

    sfx = (sfxinfo_t *) tag;

    // Parse the assignment

    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }
    
    // all values are integers

    ivalue = atoi(value);
    
    // Set the field value

    DEH_SetMapping(context, &sound_mapping, sfx, variable_name, ivalue);
}

deh_section_t deh_section_sound =
{
    "Sound",
    NULL,
    DEH_SoundStart,
    DEH_SoundParseLine,
    NULL,
    NULL,
};


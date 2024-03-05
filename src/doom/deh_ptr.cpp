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
// Parses Action Pointer entries in dehacked files
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "info.h"

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"

static actionf_t codeptrs[NUMSTATES];

static int CodePointerIndex(const actionf_t *ptr)
{
    int i;

    for (i=0; i<NUMSTATES; ++i)
    {
        if (!memcmp(&codeptrs[i], ptr, sizeof(actionf_t)))
        {
            return i;
        }
    }

    return -1;
}

static void DEH_PointerInit(void)
{
    int i;
    
    // Initialize list of dehacked pointers

    for (i=0; i<NUMSTATES; ++i)
        codeptrs[i] = states[i].action;
}

static void *DEH_PointerStart(deh_context_t *context, char *line)
{
    int frame_number = 0;
    
    // FIXME: can the third argument here be something other than "Frame"
    // or are we ok?

    if (sscanf(line, "Pointer %*i (%*s %i)", &frame_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    if (frame_number < 0 || frame_number >= NUMSTATES )
    {
        DEH_Error(context, "Invalid frame number: %i", frame_number);
        return NULL;
    }

	GameVersion_t version = frame_number < ( NUMSTATES_VANILLA - 1 ) ? exe_doom_1_2
							: frame_number < NUMSTATES_VANILLA ? exe_limit_removing
							: frame_number < NUMSTATES_BOOM ? exe_boom_2_02
							: frame_number < NUMSTATES_MBF ? exe_mbf
							: exe_mbf21;
	DEH_IncreaseGameVersion( context, version );

	extern std::unordered_map< int32_t, state_t* > statemap;
    return statemap[ frame_number ];
}

static void DEH_PointerParseLine(deh_context_t *context, char *line, void *tag)
{
    state_t *state;
    char *variable_name, *value;
    int ivalue;
    
    if (tag == NULL)
       return;

    state = (state_t *) tag;

    // Parse the assignment

    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }
    
//    printf("Set %s to %s for state\n", variable_name, value);

    // all values are integers

    ivalue = atoi(value);
    
    // set the appropriate field

    if (!strcasecmp(variable_name, "Codep frame"))
    {
        if ( ivalue < 0 || ivalue >= NUMSTATES )
        {
            DEH_Error(context, "Invalid state '%i'", ivalue);
        }
        else
        {        
			GameVersion_t version = ivalue < ( NUMSTATES_VANILLA - 1 ) ? exe_doom_1_2
									: ivalue < NUMSTATES_VANILLA ? exe_limit_removing
									: ivalue < NUMSTATES_BOOM ? exe_boom_2_02
									: ivalue < NUMSTATES_MBF ? exe_mbf
									: exe_mbf21;
			DEH_IncreaseGameVersion( context, version );
            state->action = codeptrs[ivalue];
        }
    }
    else
    {
        DEH_Warning(context, "Unknown variable name '%s'", variable_name);
    }
}

static void DEH_PointerSHA1Sum(sha1_context_t *context)
{
    int i;

    for (i=0; i<NUMSTATES; ++i)
    {
        SHA1_UpdateInt32(context, CodePointerIndex(&states[i].action));
    }
}

deh_section_t deh_section_pointer =
{
    "Pointer",
    DEH_PointerInit,
    DEH_PointerStart,
    DEH_PointerParseLine,
    NULL,
    DEH_PointerSHA1Sum,
};


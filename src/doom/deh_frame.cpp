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
// Parses "Frame" sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>

#include "doomtype.h"
#include "d_items.h"
#include "info.h"

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "deh_mapping.h"

DEH_BEGIN_MAPPING(state_mapping, state_t)
  DEH_MAPPING("Sprite number",    sprite)
  DEH_MAPPING("Sprite subnumber", frame)
  DEH_MAPPING("Duration",         tics)
  DEH_MAPPING("Next frame",       nextstate)
  DEH_MAPPING("Unknown 1",        misc1)
  DEH_MAPPING("Unknown 2",        misc2)
  MBF21_MAPPING("MBF21 Bits",     mbf21flags)
  MBF21_MAPPING("Args1",          arg1._int)
  MBF21_MAPPING("Args2",          arg2._int)
  MBF21_MAPPING("Args3",          arg3._int)
  MBF21_MAPPING("Args4",          arg4._int)
  MBF21_MAPPING("Args5",          arg5._int)
  MBF21_MAPPING("Args6",          arg6._int)
  MBF21_MAPPING("Args7",          arg7._int)
  MBF21_MAPPING("Args8",          arg8._int)
  DEH_UNSUPPORTED_MAPPING("Codep frame")
DEH_END_MAPPING

static void *DEH_FrameStart(deh_context_t *context, char *line)
{
    int frame_number = 0;
    
    if (sscanf(line, "Frame %i", &frame_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }
    
	GameVersion_t version = frame_number < ( NUMSTATES_VANILLA - 1 ) ? exe_doom_1_2
							: frame_number < NUMSTATES_VANILLA ? exe_doom_1_2 // exe_limit_removing
							: frame_number < NUMSTATES_BOOM ? exe_boom_2_02
							: frame_number < NUMSTATES_MBF ? exe_mbf
							: frame_number < NUMSTATES_PRBOOMPLUS ? exe_mbf_dehextra
							: exe_mbf21_extended;
	DEH_IncreaseGameVersion( context, version );

	extern std::unordered_map< int32_t, state_t* > statemap;
	auto foundstate = statemap.find( frame_number );
	if( foundstate == statemap.end() )
	{
		state_t* newstate = Z_MallocAs( state_t, PU_STATIC, nullptr );
		*newstate =
		{
			frame_number,
			SPR_TNT1,
			0,
			-1,
			nullptr,
			(statenum_t)frame_number,
		};
		statemap[ frame_number ] = newstate;
		return newstate;
	}
	else
	{
		return foundstate->second;
	}
}

// Simulate a frame overflow: Doom has 967 frames in the states[] array, but
// DOS dehacked internally only allocates memory for 966.  As a result, 
// attempts to set frame 966 (the last frame) will overflow the dehacked
// array and overwrite the weaponinfo[] array instead.
//
// This is noticable in Batman Doom where it is impossible to switch weapons
// away from the fist once selected.

//static void DEH_FrameOverflow(deh_context_t *context, char *varname, int value)
//{
//    if (!strcasecmp(varname, "Duration"))
//    {
//        weaponinfo[0].ammo = value;
//    }
//    else if (!strcasecmp(varname, "Codep frame")) 
//    {
//        weaponinfo[0].upstate = value;
//    }
//    else if (!strcasecmp(varname, "Next frame")) 
//    {
//        weaponinfo[0].downstate = value;
//    }
//    else if (!strcasecmp(varname, "Unknown 1"))
//    {
//        weaponinfo[0].readystate = value;
//    }
//    else if (!strcasecmp(varname, "Unknown 2"))
//    {
//        weaponinfo[0].atkstate = value;
//    }
//    else
//    {
//        DEH_Error(context, "Unable to simulate frame overflow: field '%s'",
//                  varname);
//    }
//}

static void DEH_FrameParseLine(deh_context_t *context, char *line, void *tag)
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
    
    // all values are integers

    ivalue = atoi(value);
    
	// TODO: Work out how to retain overflow behavior
    //if (state == &states[NUMSTATES_VANILLA - 1])
    //{
    //    DEH_FrameOverflow(context, variable_name, ivalue);
    //}
    //else
    {
        // set the appropriate field

        DEH_SetMapping(context, &state_mapping, state, variable_name, ivalue);
    }
}

static void DEH_FrameSHA1Sum(sha1_context_t *context)
{
    int i;

    for (i=0; i < NUMSTATES; ++i)
    {
        DEH_StructSHA1Sum(context, &state_mapping, (void*)&states[i]);
    }
}

deh_section_t deh_section_frame =
{
    "Frame",
    NULL,
    DEH_FrameStart,
    DEH_FrameParseLine,
    NULL,
    DEH_FrameSHA1Sum,
};


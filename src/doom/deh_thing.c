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
// Parses "Thing" sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "doomtype.h"

#include "deh_defs.h"
#include "deh_main.h"
#include "deh_mapping.h"

#include "info.h"

DEH_BEGIN_MAPPING(thing_mapping, mobjinfo_t)
  DEH_MAPPING("ID #",                doomednum)
  DEH_MAPPING("Initial frame",       spawnstate)
  DEH_MAPPING("Hit points",          spawnhealth)
  DEH_MAPPING("First moving frame",  seestate)
  DEH_MAPPING("Alert sound",         seesound)
  DEH_MAPPING("Reaction time",       reactiontime)
  DEH_MAPPING("Attack sound",        attacksound)
  DEH_MAPPING("Injury frame",        painstate)
  DEH_MAPPING("Pain chance",         painchance)
  DEH_MAPPING("Pain sound",          painsound)
  DEH_MAPPING("Close attack frame",  meleestate)
  DEH_MAPPING("Far attack frame",    missilestate)
  DEH_MAPPING("Death frame",         deathstate)
  DEH_MAPPING("Exploding frame",     xdeathstate)
  DEH_MAPPING("Death sound",         deathsound)
  DEH_MAPPING("Speed",               speed)
  DEH_MAPPING("Width",               radius)
  DEH_MAPPING("Height",              height)
  DEH_MAPPING("Mass",                mass)
  DEH_MAPPING("Missile damage",      damage)
  DEH_MAPPING("Action sound",        activesound)
  DEH_MAPPING("Bits",                flags)
  DEH_MAPPING("Respawn frame",       raisestate)
  MBF21_MAPPING("Bits2",             flags2)
  MBF21_MAPPING("Fast speed",        fastspeed)
  MBF21_MAPPING("Melee range",       meleerange)
  MBF21_MAPPING("Rip sound",         ripsound)
DEH_END_MAPPING

DOOM_C_API void DEH_BexHandleThingBits( deh_context_t* context, const char* value, mobjinfo_t* mobj );
DOOM_C_API void DEH_BexHandleThingBits2( deh_context_t* context, const char* value, mobjinfo_t* mobj );

static void *DEH_ThingStart(deh_context_t *context, char *line)
{
    int thing_number = 0;
    mobjinfo_t *mobj;
    
    if (sscanf(line, "Thing %i", &thing_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    // dehacked files are indexed from 1
    --thing_number;

    if (thing_number < 0 || thing_number >= NUMMOBJTYPES)
    {
        DEH_Warning(context, "Invalid thing number: %i", thing_number);
        return NULL;
    }
    
    mobj = &mobjinfo[thing_number];
    
    return mobj;
}

static void DEH_ThingParseLine(deh_context_t *context, char *line, void *tag)
{
    mobjinfo_t *mobj;
    char *variable_name, *value;
    int ivalue;
    
    if (tag == NULL)
       return;

    mobj = (mobjinfo_t *) tag;

    // Parse the assignment

    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse

        DEH_Warning(context, "Failed to parse assignment");
        return;
    }
    
//    printf("Set %s to %s for mobj\n", variable_name, value);

	if( deh_allow_bex && strcmp( variable_name, "Bits" ) == 0 && !isdigit( value[ 0 ] ) )
	{
		DEH_BexHandleThingBits( context, value, mobj );
	}
	else if( deh_allow_bex && strcmp( variable_name, "Bits2" ) == 0 && !isdigit( value[ 0 ] ) )
	{
		DEH_BexHandleThingBits2( context, value, mobj );
	}
	else
	{
		// all values are integers

		ivalue = atoi(value);

		// Set the field value

		DEH_SetMapping(context, &thing_mapping, mobj, variable_name, ivalue);
	}
}

static void DEH_ThingSHA1Sum(sha1_context_t *context)
{
    int i;

    for (i=0; i<NUMMOBJTYPES; ++i)
    {
        DEH_StructSHA1Sum(context, &thing_mapping, &mobjinfo[i]);
    }
}

deh_section_t deh_section_thing =
{
    "Thing",
    NULL,
    DEH_ThingStart,
    DEH_ThingParseLine,
    NULL,
    DEH_ThingSHA1Sum,
	NULL,
};


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
  MBF21_MAPPING("MBF21 Bits",        flags2)
  MBF21_MAPPING("Infighting group",  infightinggroup)
  MBF21_MAPPING("Projectile group",  projectilegroup)
  MBF21_MAPPING("Splash group",      splashgroup)
  MBF21_MAPPING("Fast speed",        fastspeed)
  MBF21_MAPPING("Melee range",       meleerange)
  MBF21_MAPPING("Rip sound",         ripsound)
DEH_END_MAPPING

DOOM_C_API void DEH_BexHandleThingBits( deh_context_t* context, const char* value, mobjinfo_t* mobj );
DOOM_C_API void DEH_BexHandleThingBits2( deh_context_t* context, const char* value, mobjinfo_t* mobj );

static void *DEH_ThingStart(deh_context_t *context, char *line)
{
    int thing_number = 0;

	if (sscanf(line, "Thing %i", &thing_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    // dehacked files are indexed from 1
    --thing_number;

	GameVersion_t version = thing_number < MT_NUMVANILLATYPES ? exe_doom_1_9
							: thing_number < MT_NUMBOOMTYPES ? exe_boom_2_02
							: thing_number < MT_NUMMBFTYPES ? exe_mbf
							: thing_number < MT_NUMPRBOOMTYPES ? exe_mbf_dehextra
							: exe_mbf21_extended;

	DEH_IncreaseGameVersion( context, version );

	extern std::unordered_map< int32_t, mobjinfo_t* > mobjtypemap;
	auto foundmobj = mobjtypemap.find( thing_number );
	if( foundmobj == mobjtypemap.end() )
	{
		constexpr auto NewMobj = []() -> mobjinfo_t
		{
			mobjinfo_t newmobjinfo = {};
			newmobjinfo.fastspeed = -1;
			newmobjinfo.meleerange = IntToFixed( 64 );
			return newmobjinfo;
		};

		mobjinfo_t* mobj = Z_MallocAs( mobjinfo_t, PU_STATIC, nullptr );
		*mobj = NewMobj();
		mobj->type = thing_number;
		mobjtypemap[ thing_number ] = mobj;
		return mobj;
	}
	else
	{
		return foundmobj->second;
	}
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

	if( strcmp( variable_name, "Bits" ) == 0
		&& !( isdigit( value[ 0 ] ) 
			|| ( value[ 0 ] == '-' && isdigit( value[ 1 ] ) ) )
		)
	{
		DEH_BexHandleThingBits( context, value, mobj );
	}
	else if( strcmp( variable_name, "MBF21 Bits" ) == 0
		&& !( isdigit( value[ 0 ] ) 
			|| ( value[ 0 ] == '-' && isdigit( value[ 1 ] ) ) )
		)
	{
		DEH_BexHandleThingBits2( context, value, mobj );
	}
	else if( strcmp( variable_name, "Infighting group" ) == 0 )
	{
		ivalue = atoi(value) + infighting_user;
		DEH_SetMapping(context, &thing_mapping, mobj, variable_name, ivalue);
	}
	else if( strcmp( variable_name, "Projectile group" ) == 0 )
	{
		ivalue = atoi(value);
		if( ivalue >= 0 )
		{
			ivalue += projectile_user;
		}
		else
		{
			ivalue = projectile_noimmunity;
		}
		DEH_SetMapping(context, &thing_mapping, mobj, variable_name, ivalue);
	}
	else if( strcmp( variable_name, "Splash group" ) == 0 )
	{
		ivalue = atoi(value) + splash_user;
		DEH_SetMapping(context, &thing_mapping, mobj, variable_name, ivalue);
	}
	else if( strcmp( variable_name, "ID #" ) == 0 )
	{
		extern std::unordered_map< int32_t, mobjinfo_t* > doomednummap;

		ivalue = atoi(value);
		doomednummap[ ivalue ] = mobj;
		DEH_SetMapping(context, &thing_mapping, mobj, variable_name, ivalue);
	}
	else if( strcmp( variable_name, "Retro Bits" ) == 0
		|| strcmp( variable_name, "Name1" ) == 0
		|| strcmp( variable_name, "Plural1" ) == 0 )
	{
		// Silently ignore
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
        DEH_StructSHA1Sum(context, &thing_mapping, (void*)&mobjinfo[i]);
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
};


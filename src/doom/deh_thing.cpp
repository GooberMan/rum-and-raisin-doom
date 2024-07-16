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

#include "i_timer.h"

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
  RNR_MAPPING("RNR24 Bits",          rnr24flags)
  RNR_MAPPING("Min respawn tics",    minrespawntics)
  RNR_MAPPING("Respawn dice",        respawndice)
  RNR_MAPPING("Dropped item",        dropthing)
  RNR_MAPPING("Pickup ammo type",    pickupammotype)
  RNR_MAPPING("Pickup ammo category", pickupammocategory)
  RNR_MAPPING("Pickup weapon type",  pickupweapontype)
  RNR_MAPPING("Pickup item type",    pickupitemtype)
  RNR_MAPPING("Pickup bonus count",  pickupbonuscount)
  RNR_MAPPING("Pickup sound",        pickupsound)
  RNR_MAPPING("Pickup message",      pickupstringmnemonic)
  RNR_MAPPING("Translation",         translationlump)
DEH_END_MAPPING

DOOM_C_API void DEH_BexHandleThingBits( deh_context_t* context, const char* value, mobjinfo_t* mobj );
DOOM_C_API void DEH_BexHandleThingBits2( deh_context_t* context, const char* value, mobjinfo_t* mobj );
DOOM_C_API void DEH_BexHandleThingBitsRNR24( deh_context_t* context, const char* value, mobjinfo_t* mobj );

extern std::unordered_map< int32_t, mobjinfo_t* > mobjtypemap;
extern std::vector< mobjinfo_t* > allmobjs;

mobjinfo_t* DEH_GetThing( deh_context_t* context, int32_t thing_number )
{
	if( thing_number == -1 )
	{
		DEH_Error(context, "Invalid thing number: 0" );
		return nullptr;
	}
	
	GameVersion_t version = thing_number < -1 ? exe_rnr24
							: thing_number < MT_NUMVANILLATYPES ? exe_doom_1_9
							: thing_number < MT_NUMBOOMTYPES ? exe_boom_2_02
							: thing_number < MT_NUMMBFTYPES ? exe_mbf
							: thing_number <= MT_DEHEXTRA_MAX ? exe_mbf_dehextra
							: exe_mbf21_extended;

	DEH_IncreaseGameVersion( context, version );

	auto foundmobj = mobjtypemap.find( thing_number );
	if( foundmobj == mobjtypemap.end() )
	{
		constexpr auto NewMobj = []() -> mobjinfo_t
		{
			mobjinfo_t newmobjinfo = {};
			newmobjinfo.fastspeed			= -1;
			newmobjinfo.meleerange			= IntToFixed( 64 );
			newmobjinfo.minrespawntics		= 12 * TICRATE;
			newmobjinfo.respawndice			= 4;
			newmobjinfo.dropthing			= -1;
			newmobjinfo.pickupammotype		= -1;
			newmobjinfo.pickupammocategory	= -1;
			newmobjinfo.pickupweapontype	= -1;
			newmobjinfo.pickupitemtype		= -1;
			newmobjinfo.pickupbonuscount	= 6;

			return newmobjinfo;
		};

		mobjinfo_t* mobj = Z_MallocAs( mobjinfo_t, PU_STATIC, nullptr );
		*mobj = NewMobj();
		mobj->type = thing_number;
		allmobjs.push_back( mobj );
		mobjtypemap[ thing_number ] = mobj;
		return mobj;
	}
	else
	{
		return foundmobj->second;
	}
}

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
	
	return DEH_GetThing( context, thing_number );
}

static const char* DEH_CopyString( const char* value )
{
	size_t valuelen = strlen( value );
	char* output = (char*)Z_MallocZero( valuelen + 1, PU_STATIC, nullptr );
	M_StringCopy( output, value, valuelen + 1 );
	M_ForceUppercase( output );
	return output;
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

	if( strcmp( variable_name, "Pickup message" ) == 0 )
	{
		DEH_IncreaseGameVersion( context, exe_rnr24 );
		mobj->pickupstringmnemonic = M_DuplicateStringToZone( value, PU_STATIC, nullptr );
	}
	else if( strcmp( variable_name, "Translation" ) == 0 )
	{
		DEH_IncreaseGameVersion( context, exe_rnr24 );
		mobj->translationlump = M_DuplicateStringToZone( value, PU_STATIC, nullptr );
		M_ForceUppercase( (char*)mobj->translationlump );
	}
	else if( strcmp( variable_name, "Bits" ) == 0
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
	else if( strcmp( variable_name, "RNR24 Bits" ) == 0
		&& !( isdigit( value[ 0 ] ) 
			|| ( value[ 0 ] == '-' && isdigit( value[ 1 ] ) ) )
		)
	{
		DEH_BexHandleThingBitsRNR24( context, value, mobj );
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
	else if( strcmp( variable_name, "Dropped item" ) == 0 )
	{
		// Need to account for dehacked things being 1-indexed
		ivalue = atoi(value) - 1;
		[[maybe_unused]] mobjinfo_t* foundthing = DEH_GetThing( context, ivalue );
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

static void DEH_ThingEnd( deh_context_t* context, void* tag )
{
	mobjinfo_t* mobj = (mobjinfo_t*)tag;

	mobj->minimumversion = DEH_GameVersion( context );
}

static void DEH_ThingSHA1Sum(sha1_context_t *context)
{
    int i;

    for (i=0; i<NUMMOBJTYPES; ++i)
    {
        DEH_StructSHA1Sum(context, &thing_mapping, (void*)&mobjinfo[i]);
    }
}

template< typename fnv >
static typename fnv::value_type DEH_ThingFNV1aHash( int32_t version, typename fnv::value_type base )
{
	for( mobjinfo_t* mobj : allmobjs )
	{
		if( version >= mobj->minimumversion )
		{
			base = fnv::calc( base, mobj->type );
			base = fnv::calc( base, mobj->doomednum );
			base = fnv::calc( base, mobj->spawnstate );
			base = fnv::calc( base, mobj->spawnhealth );
			base = fnv::calc( base, mobj->seestate );
			base = fnv::calc( base, mobj->seesound );
			base = fnv::calc( base, mobj->reactiontime );
			base = fnv::calc( base, mobj->attacksound );
			base = fnv::calc( base, mobj->painstate );
			base = fnv::calc( base, mobj->painchance );
			base = fnv::calc( base, mobj->painsound );
			base = fnv::calc( base, mobj->meleestate );
			base = fnv::calc( base, mobj->missilestate );
			base = fnv::calc( base, mobj->deathstate );
			base = fnv::calc( base, mobj->xdeathstate );
			base = fnv::calc( base, mobj->deathsound );
			base = fnv::calc( base, mobj->speed );
			base = fnv::calc( base, mobj->radius );
			base = fnv::calc( base, mobj->height );
			base = fnv::calc( base, mobj->mass );
			base = fnv::calc( base, mobj->damage );
			base = fnv::calc( base, mobj->activesound );
			base = fnv::calc( base, mobj->flags );
			base = fnv::calc( base, mobj->raisestate );

			if( version >= exe_mbf21 )
			{
				base = fnv::calc( base, mobj->flags2 );
				base = fnv::calc( base, mobj->infightinggroup );
				base = fnv::calc( base, mobj->projectilegroup );
				base = fnv::calc( base, mobj->splashgroup );
				base = fnv::calc( base, mobj->fastspeed );
				base = fnv::calc( base, mobj->meleerange );
				base = fnv::calc( base, mobj->ripsound );
			}

			if( version >= exe_rnr24 )
			{
				base = fnv::calc( base, mobj->rnr24flags );
				base = fnv::calc( base, mobj->minrespawntics );
				base = fnv::calc( base, mobj->respawndice );
				base = fnv::calc( base, mobj->dropthing );
				base = fnv::calc( base, mobj->pickupammotype );
				base = fnv::calc( base, mobj->pickupammocategory );
				base = fnv::calc( base, mobj->pickupweapontype );
				base = fnv::calc( base, mobj->pickupitemtype );
				base = fnv::calc( base, mobj->pickupbonuscount );
				base = fnv::calc( base, mobj->pickupsound );
				base = fnv::calc( base, mobj->pickupstringmnemonic );
				base = fnv::calc( base, mobj->translationlump ? mobj->translationlump : "null" );
			}
		}
	}
	
	return base;
}

deh_section_t deh_section_thing =
{
    "Thing",
    NULL,
    DEH_ThingStart,
    DEH_ThingParseLine,
    NULL,
    DEH_ThingSHA1Sum,
	DEH_ThingFNV1aHash< fnv1a< uint32_t > >,
	DEH_ThingFNV1aHash< fnv1a< uint64_t > >,
};


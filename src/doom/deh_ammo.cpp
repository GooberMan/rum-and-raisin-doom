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
// Parses "Ammo" sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomtype.h"
#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "deh_mapping.h"

#include "d_items.h"
#include "p_local.h"

DEH_BEGIN_MAPPING( ammo_mapping, ammoinfo_t )
  DEH_MAPPING( "Per ammo",                clipammo )
  DEH_MAPPING( "Max ammo",                maxammo )
  RNR_MAPPING( "Initial ammo",            initialammo )
  RNR_MAPPING( "Max upgraded ammo",       maxupgradedammo )
  RNR_MAPPING( "Box ammo",                boxammo )
  RNR_MAPPING( "Backpack ammo",           backpackammo )
  RNR_MAPPING( "Weapon ammo",             weaponammo )
  RNR_MAPPING( "Dropped ammo",            droppedclipammo )
  RNR_MAPPING( "Dropped box ammo",        droppedboxammo )
  RNR_MAPPING( "Dropped backpack ammo",   droppedbackpackammo )
  RNR_MAPPING( "Dropped weapon ammo",     droppedweaponammo )
  RNR_MAPPING( "Deathmatch weapon ammo",  deathmatchweaponammo )
  RNR_MAPPING( "Skill 1 multiplier",      skillmul[ 0 ] )
  RNR_MAPPING( "Skill 2 multiplier",      skillmul[ 1 ] )
  RNR_MAPPING( "Skill 3 multiplier",      skillmul[ 2 ] )
  RNR_MAPPING( "Skill 4 multiplier",      skillmul[ 3 ] )
  RNR_MAPPING( "Skill 5 multiplier",      skillmul[ 4 ] )
DEH_END_MAPPING

extern std::unordered_map< int32_t, ammoinfo_t* > ammomap;
extern std::vector< ammoinfo_t* > allammo;

static void *DEH_AmmoStart(deh_context_t *context, char *line)
{
    int32_t ammo_number = 0;

    if (sscanf(line, "Ammo %i", &ammo_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

	if( ammo_number == -1 )
	{
        DEH_Error(context, "Invalid ammo number: %i", ammo_number);
        return NULL;
	}

	auto found = ammomap.find( ammo_number );
	if( found == ammomap.end() )
	{
		DEH_IncreaseGameVersion( context, exe_rnr24 );

		ammoinfo_t* newammo = (ammoinfo_t*)Z_Malloc( sizeof( ammoinfo_t ), PU_STATIC, nullptr );
		*newammo =
		{
			ammo_number,		// index
			exe_rnr24,			// minimumversion
			0,					// clipammo
			0,					// maxammo
			0,					// initialammo
			0,					// maxupgradedammo
			0,					// boxammo
			0,					// backpackammo
			0,					// weaponammo
			0,					// droppedclipammo
			0,					// droppedboxammo
			0,					// droppedbackpackammo
			0,					// droppedweaponammo
			0,					// deathmatchweaponammo
			{ IntToFixed( 2 ), IntToFixed( 1 ), IntToFixed( 1 ), IntToFixed( 1 ), IntToFixed( 2 ) },	// skillmul
			true,				// recalculateFromOriginalValues
		};

		allammo.push_back( newammo );
		ammomap[ ammo_number ] = newammo;
		return newammo;
	}

	found->second->recalculateFromOriginalValues = true;
	return found->second;
}

static void DEH_AmmoParseLine(deh_context_t *context, char *line, void *tag)
{
    char *variable_name, *value;
    int ivalue;

    if (tag == NULL)
        return;

    ammoinfo_t* ammoinfo = (ammoinfo_t*)tag;

    // Parse the assignment

    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse

        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

	if( !( strcmp( variable_name, "Per ammo" ) == 0
			|| strcmp( variable_name, "Max ammo" ) == 0 ) )
	{
		ammoinfo->recalculateFromOriginalValues = false;
	}

    ivalue = atoi(value);

	DEH_SetMapping(context, &ammo_mapping, ammoinfo, variable_name, ivalue);
}

static void DEH_AmmoEnd( deh_context_t* context, ammoinfo_t* ammoinfo )
{
	if( ammoinfo->recalculateFromOriginalValues )
	{
		ammoinfo->maxupgradedammo		= ammoinfo->maxammo * 2;
		ammoinfo->boxammo				= ammoinfo->clipammo * 5;
		ammoinfo->backpackammo			= ammoinfo->clipammo;
		ammoinfo->weaponammo			= ammoinfo->clipammo * 2;

		ammoinfo->droppedclipammo		= ammoinfo->clipammo / 2;
		ammoinfo->droppedboxammo		= ammoinfo->boxammo / 2;
		ammoinfo->droppedbackpackammo	= ammoinfo->backpackammo / 2;
		ammoinfo->droppedweaponammo		= ammoinfo->weaponammo / 2;

		ammoinfo->deathmatchweaponammo	= ammoinfo->clipammo * 5;

		ammoinfo->recalculateFromOriginalValues = false;
	}
}

static void DEH_AmmoSHA1Hash(sha1_context_t *context)
{
    //int i;
	//
    //for (i=0; i<NUMAMMO; ++i)
    //{
    //    SHA1_UpdateInt32(context, clipammo[i]);
    //    SHA1_UpdateInt32(context, maxammo[i]);
    //}
}

template< typename fnv >
static typename fnv::value_type DEH_AmmoFNV1aHash( int32_t version, typename fnv::value_type base )
{
	for( ammoinfo_t* ammo : allammo )
	{
		if( version >= ammo->minimumversion )
		{
			base = fnv::calc( base, ammo->clipammo );
			base = fnv::calc( base, ammo->maxammo );
			if( version >= exe_rnr24 )
			{
				base = fnv::calc( base, ammo->initialammo );
				base = fnv::calc( base, ammo->maxupgradedammo );
				base = fnv::calc( base, ammo->boxammo );
				base = fnv::calc( base, ammo->backpackammo );
				base = fnv::calc( base, ammo->weaponammo );
				base = fnv::calc( base, ammo->droppedclipammo );
				base = fnv::calc( base, ammo->droppedboxammo );
				base = fnv::calc( base, ammo->droppedbackpackammo );
				base = fnv::calc( base, ammo->droppedweaponammo );
				base = fnv::calc( base, ammo->deathmatchweaponammo );
				base = fnv::calc( base, ammo->skillmul[ 0 ] );
				base = fnv::calc( base, ammo->skillmul[ 1 ] );
				base = fnv::calc( base, ammo->skillmul[ 2 ] );
				base = fnv::calc( base, ammo->skillmul[ 3 ] );
				base = fnv::calc( base, ammo->skillmul[ 4 ] );
			}
		}
	}

	return base;
}

deh_section_t deh_section_ammo =
{
    "Ammo",
    NULL,
    DEH_AmmoStart,
    DEH_AmmoParseLine,
    (deh_section_end_t)DEH_AmmoEnd,
    DEH_AmmoSHA1Hash,
	DEH_AmmoFNV1aHash< fnv1a< uint32_t > >,
	DEH_AmmoFNV1aHash< fnv1a< uint64_t > >,
};


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
// Parses "Weapon" sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"

#include "d_items.h"
#include "info.h"

#include "deh_defs.h"
#include "deh_main.h"
#include "deh_mapping.h"

#include "m_container.h"
#include "m_misc.h"

DEH_BEGIN_MAPPING(weapon_mapping, weaponinfo_t)
  DEH_MAPPING("Ammo type",        ammo)
  DEH_MAPPING("Deselect frame",   upstate)
  DEH_MAPPING("Select frame",     downstate)
  DEH_MAPPING("Bobbing frame",    readystate)
  DEH_MAPPING("Shooting frame",   atkstate)
  DEH_MAPPING("Firing frame",     flashstate)
  MBF21_MAPPING("Ammo per shot",  ammopershot)
  MBF21_MAPPING("MBF21 Bits",     mbf21flags)
  RNR_MAPPING("Slot",             slot)
  RNR_MAPPING("Priority",         priority)
DEH_END_MAPPING

DOOM_C_API void DEH_BexHandleWeaponBitsMBF21( deh_context_t* context, const char* value, weaponinfo_t* weapon );

static void *DEH_WeaponStart(deh_context_t *context, char *line)
{
    int weapon_number = 0;

    if (sscanf(line, "Weapon %i", &weapon_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    if (weapon_number == wp_nochange)
    {
        DEH_Warning(context, "Invalid weapon number: %i", weapon_number);
        return NULL;
    }

	extern std::unordered_map< int32_t, weaponinfo_t* >	weaponmap;
	extern std::vector< weaponinfo_t* >					weaponpriority;

	auto foundweapon = weaponmap.find( weapon_number );
	if( foundweapon == weaponmap.end() )
	{
		DEH_IncreaseGameVersion( context, exe_mbf21_rnr );

		weaponinfo_t* newweapon = Z_MallocAs( weaponinfo_t, PU_STATIC, nullptr );
		*newweapon =
		{
			weapon_number,	// index
			-1,				// slot
			-1,				// priority
			registered,		// mingamemode
			am_noammo,		// ammo
			S_NULL,			// upstate
			S_NULL,			// downstate
			S_NULL,			// readystate
			S_NULL,			// atkstate
			S_NULL,			// flashstate
			WF_NONE,		// mbf21flags
			1,				// ammopershot
		};

		weaponmap[ weapon_number ] = newweapon;
		weaponpriority.push_back( newweapon );
		return newweapon;
	}
	else
	{
		return foundweapon->second;
	}
}

static void DEH_WeaponParseLine(deh_context_t *context, char *line, void *tag)
{
    char *variable_name, *value;
    weaponinfo_t *weapon;
    int ivalue;
    
    if (tag == NULL)
        return;

    weapon = (weaponinfo_t *) tag;

    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse

        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

	if( strcmp( variable_name, "MBF21 Bits" ) == 0
		&& !( isdigit( value[ 0 ] ) 
			|| ( value[ 0 ] == '-' && isdigit( value[ 1 ] ) ) )
		)
	{
		DEH_BexHandleWeaponBitsMBF21( context, value, weapon );
	}
	else
	{
		ivalue = atoi(value);

		DEH_SetMapping(context, &weapon_mapping, weapon, variable_name, ivalue);
	}
}

static void DEH_WeaponEnd(deh_context_t* context, void* tag)
{
	weaponinfo_t* weapon = (weaponinfo_t*)tag;
	if( weapon->priority < 0 )
	{
		DEH_Warning(context, "Weapon %d priority less than zero", weapon->index );
	}
	else if( weapon->slot < 0 || weapon->slot > 9 )
	{
		DEH_Warning(context, "Weapon %d slot %d out of range (0-9)", weapon->index, weapon->slot );
	}
	else if( weapon->ammo != am_noammo && weapon->ammopershot <= 0 )
	{
		DEH_Warning(context, "Weapon %d ammopershot %d invalid for ammo type", weapon->index, weapon->ammopershot );
	}

	extern std::vector< weaponinfo_t* >					weaponpriority;
	std::sort( weaponpriority.begin(), weaponpriority.end(), []( const weaponinfo_t* lhs, const weaponinfo_t* rhs ) -> bool
		{
			return lhs->priority > rhs->priority;
		} );
}

static void DEH_WeaponSHA1Sum(sha1_context_t *context)
{
    int i;

    for (i=0; i<NUMWEAPONS ;++i)
    {
        DEH_StructSHA1Sum(context, &weapon_mapping, (void*)&weaponinfo[i]);
    }
}

deh_section_t deh_section_weapon =
{
    "Weapon",
    NULL,
    DEH_WeaponStart,
    DEH_WeaponParseLine,
    DEH_WeaponEnd,
    DEH_WeaponSHA1Sum,
};


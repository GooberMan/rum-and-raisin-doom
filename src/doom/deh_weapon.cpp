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
  RNR_MAPPING("Slot Priority",    slotpriority)
  RNR_MAPPING("Switch Priority",  switchpriority)
  RNR_MAPPING("Initial Owned",    initialowned)
  RNR_MAPPING("Initial Raised",   initialraised)
  RNR_MAPPING("Carousel Icon",    carouselicon)
  RNR_MAPPING("Allow switch with owned weapon", allowswitchifownedweapon)
  RNR_MAPPING("No switch with owned weapon", noswitchifownedweapon)
  RNR_MAPPING("Allow switch with owned item", allowswitchifowneditem)
  RNR_MAPPING("No switch with owned item", noswitchifowneditem)
DEH_END_MAPPING

DOOM_C_API void DEH_BexHandleWeaponBitsMBF21( deh_context_t* context, const char* value, weaponinfo_t* weapon );

extern std::vector< weaponinfo_t* >					allweapons;
extern std::unordered_map< int32_t, weaponinfo_t* >	weaponmap;
extern std::vector< weaponinfo_t* >					weaponswitchpriority;

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
        DEH_Error(context, "Invalid weapon number: %i", weapon_number);
        return NULL;
    }

	auto foundweapon = weaponmap.find( weapon_number );
	if( foundweapon == weaponmap.end() )
	{
		DEH_IncreaseGameVersion( context, exe_rnr24 );

		weaponinfo_t* newweapon = Z_MallocAs( weaponinfo_t, PU_STATIC, nullptr );
		*newweapon =
		{
			weapon_number,	// index
			exe_rnr24,		// minimumversion
			-1,				// slot
			-1,				// slotpriority
			-1,				// switchpriority
			registered,		// mingamemode
			false,			// initialowned
			false,			// initialraised
			nullptr,		// carouselicon
			-1,				// allowswitchifownedweapon
			-1,				// noswitchifownedweapon
			-1,				// allowswitchifowneditem
			-1,				// noswitchifowneditem
			am_noammo,		// ammo
			S_NULL,			// upstate
			S_NULL,			// downstate
			S_NULL,			// readystate
			S_NULL,			// atkstate
			S_NULL,			// flashstate
			WF_NONE,		// mbf21flags
			1,				// ammopershot
		};

		allweapons.push_back( newweapon );
		weaponmap[ weapon_number ] = newweapon;
		weaponswitchpriority.push_back( newweapon );
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

	if( strcmp( variable_name, "Carousel Icon" ) == 0 )
	{
		DEH_IncreaseGameVersion( context, exe_rnr24 );
		weapon->carouselicon = M_DuplicateStringToZone( value, PU_STATIC, nullptr );
		M_ForceUppercase( (char*)weapon->carouselicon );
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
	if( weapon->slotpriority < 0 )
	{
		DEH_Warning(context, "Weapon %d slot priority less than zero", weapon->index );
	}
	else if( weapon->switchpriority < 0 )
	{
		DEH_Warning(context, "Weapon %d switch priority less than zero", weapon->index );
	}
	else if( weapon->slot < 0 || weapon->slot > 9 )
	{
		DEH_Warning(context, "Weapon %d slot %d out of range (0-9)", weapon->index, weapon->slot );
	}
	else if( weapon->ammo != am_noammo && weapon->ammopershot <= 0 )
	{
		DEH_Warning(context, "Weapon %d ammopershot %d invalid for ammo type", weapon->index, weapon->ammopershot );
	}

	extern std::vector< weaponinfo_t* >					weaponswitchpriority;
	extern std::vector< std::vector< weaponinfo_t* > >	weaponslotpriority;
	std::sort( weaponswitchpriority.begin(), weaponswitchpriority.end(), []( const weaponinfo_t* lhs, const weaponinfo_t* rhs ) -> bool
		{
			return lhs->switchpriority > rhs->switchpriority;
		} );

	for( std::vector< weaponinfo_t* >& slotpriority : weaponslotpriority )
	{
		std::sort( slotpriority.begin(), slotpriority.end(), []( const weaponinfo_t* lhs, const weaponinfo_t* rhs ) -> bool
			{
				return lhs->slotpriority > rhs->slotpriority;
			} );
	}
}

static void DEH_WeaponSHA1Sum(sha1_context_t *context)
{
    int i;

    for (i=0; i<NUMWEAPONS ;++i)
    {
        DEH_StructSHA1Sum(context, &weapon_mapping, (void*)&weaponinfo[i]);
    }
}

static uint32_t DEH_WeaponFNV1aHash( int32_t version, uint32_t base )
{
	for( weaponinfo_t* weapon : allweapons )
	{
		if( version >= weapon->minimumversion )
		{
			base = fnv1a32( base, weapon->index );

			base = fnv1a32( base, weapon->ammo );
			base = fnv1a32( base, weapon->upstate );
			base = fnv1a32( base, weapon->downstate );
			base = fnv1a32( base, weapon->readystate );
			base = fnv1a32( base, weapon->atkstate );
			base = fnv1a32( base, weapon->flashstate );

			if( version >= exe_mbf21 )
			{
				base = fnv1a32( base, weapon->ammopershot );
				base = fnv1a32( base, weapon->mbf21flags );
			}

			if( version >= exe_rnr24 )
			{
				base = fnv1a32( base, weapon->slot );
				base = fnv1a32( base, weapon->slotpriority );
				base = fnv1a32( base, weapon->switchpriority );
				base = fnv1a32( base, weapon->initialowned );
				base = fnv1a32( base, weapon->initialraised );
				base = fnv1a32( base, weapon->carouselicon );
				base = fnv1a32( base, weapon->allowswitchifownedweapon );
				base = fnv1a32( base, weapon->noswitchifownedweapon );
				base = fnv1a32( base, weapon->allowswitchifowneditem );
				base = fnv1a32( base, weapon->noswitchifowneditem );
			}
		}
	}

	return base;
}

deh_section_t deh_section_weapon =
{
    "Weapon",
    NULL,
    DEH_WeaponStart,
    DEH_WeaponParseLine,
    DEH_WeaponEnd,
    DEH_WeaponSHA1Sum,
	DEH_WeaponFNV1aHash,
};


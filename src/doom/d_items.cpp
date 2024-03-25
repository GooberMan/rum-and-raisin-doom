//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//


// We are referring to sprite numbers.
#include "info.h"

#include "d_items.h"

#include "m_container.h"

//
// PSPRITE ACTIONS for waepons.
// This struct controls the weapon animations.
//
// Each entry is:
//   ammo/amunition type
//  upstate
//  downstate
// readystate
// atkstate, i.e. attack/fire/hit frame
// flashstate, muzzle flash
//

static weaponinfo_t	builtinweaponinfo[] =
{
	{
		// fist
		wp_fist,			// index
		1,					// slot
		0,					// priority
		shareware,			// mingamemode
		am_noammo,
		S_PUNCHUP,
		S_PUNCHDOWN,
		S_PUNCH,
		S_PUNCH1,
		S_NULL,
		WF_MBF21_FLEEMELEE | WF_MBF21_AUTOSWITCHFROM | WF_MBF21_NOAUTOSWITCHTO,
		1,
	},
	{
		// pistol
		wp_pistol,			// index
		2,					// slot
		4,					// priority
		shareware,			// mingamemode
		am_clip,
		S_PISTOLUP,
		S_PISTOLDOWN,
		S_PISTOL,
		S_PISTOL1,
		S_PISTOLFLASH,
		WF_MBF21_AUTOSWITCHFROM,
		1,
	},
	{
		// shotgun
		wp_shotgun,			// index
		3,					// slot
		5,					// priority
		shareware,			// mingamemode
		am_shell,
		S_SGUNUP,
		S_SGUNDOWN,
		S_SGUN,
		S_SGUN1,
		S_SGUNFLASH1,
		WF_NONE,
		1,
	},
	{
		// chaingun
		wp_chaingun,		// index
		4,					// slot
		6,					// priority
		shareware,			// mingamemode
		am_clip,
		S_CHAINUP,
		S_CHAINDOWN,
		S_CHAIN,
		S_CHAIN1,
		S_CHAINFLASH1,
		WF_NONE,
		1,
	},
	{
		// missile launcher
		wp_missile,			// index
		5,					// slot
		2,					// priority
		shareware,			// mingamemode
		am_misl,
		S_MISSILEUP,
		S_MISSILEDOWN,
		S_MISSILE,
		S_MISSILE1,
		S_MISSILEFLASH1,
		WF_MBF21_NOAUTOFIRE,
		1,
	},
	{
		// plasma rifle
		wp_plasma,			// index
		6,					// slot
		8,					// priority
		registered,			// mingamemode
		am_cell,
		S_PLASMAUP,
		S_PLASMADOWN,
		S_PLASMA,
		S_PLASMA1,
		S_PLASMAFLASH1,
		WF_NONE,
		1,
	},
	{
		// bfg 9000
		wp_bfg,				// index
		7,					// slot
		1,					// priority
		registered,			// mingamemode
		am_cell,
		S_BFGUP,
		S_BFGDOWN,
		S_BFG,
		S_BFG1,
		S_BFGFLASH1,
		WF_MBF21_NOAUTOFIRE,
		40,
	},
	{
		// chainsaw
		wp_chainsaw,		// index
		1,					// slot
		3,					// priority
		shareware,			// mingamemode
		am_noammo,
		S_SAWUP,
		S_SAWDOWN,
		S_SAW,
		S_SAW1,
		S_NULL,
		WF_MBF21_NOTHRUST | WF_MBF21_FLEEMELEE | WF_MBF21_NOAUTOSWITCHTO,
		1,
	},
	{
		// super shotgun
		wp_supershotgun,	// index
		3,					// slot
		7,					// priority
		commercial,			// mingamemode
		am_shell,
		S_DSGUNUP,
		S_DSGUNDOWN,
		S_DSGUN,
		S_DSGUN1,
		S_DSGUNFLASH1,
		WF_NONE,
		2,
	},
};

static std::unordered_map< int32_t, weaponinfo_t* > BuildWeaponMap( std::span< weaponinfo_t > weaponspan )
{
	std::unordered_map< int32_t, weaponinfo_t* > weaponmap;
	for( weaponinfo_t& weapon : weaponspan )
	{
		weaponmap[ weapon.index ] = &weapon;
	}

	return weaponmap;
}

static std::vector< weaponinfo_t* > BuildWeaponPrioritys( std::span< weaponinfo_t > weaponspan )
{
	std::vector< weaponinfo_t* > weaponpriority;
	weaponpriority.reserve( weaponspan.size() );
	for( weaponinfo_t& weapon : weaponspan )
	{
		weaponpriority.push_back( &weapon );
	}

	std::sort( weaponpriority.begin(), weaponpriority.end(), []( const weaponinfo_t* lhs, const weaponinfo_t* rhs ) -> bool
		{
			return lhs->priority > rhs->priority;
		} );
	return weaponpriority;
}

std::unordered_map< int32_t, weaponinfo_t* >	weaponmap = BuildWeaponMap( std::span( builtinweaponinfo ) );
std::vector< weaponinfo_t* >					weaponpriority = BuildWeaponPrioritys( std::span( builtinweaponinfo ) );
DoomWeaponLookup								weaponinfo;

int32_t DoomWeaponLookup::size()
{
	return weaponmap.size();
}

DoomWeapons DoomWeaponLookup::ByPriority()
{
	return { weaponpriority.data(), weaponpriority.data() + weaponpriority.size() };
}

weaponinfo_t& DoomWeaponLookup::Fetch( int32_t weapon )
{
	auto found = weaponmap.find( weapon );
	if( found == weaponmap.end() )
	{
		I_Error( "Attempting to access invalid weapon '%d'", weapon );
	}

	return *found->second;
	
}

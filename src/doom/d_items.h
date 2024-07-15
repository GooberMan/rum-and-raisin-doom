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
//	Items: key cards, artifacts, weapon, ammunition.
//


#ifndef __D_ITEMS__
#define __D_ITEMS__

#include "doomdef.h"
#include "m_fixed.h"

DOOM_C_API typedef struct ammoinfo_s
{
	int32_t	index;

	int32_t clipammo;
	int32_t maxammo;

	// R&R24 additions
	int32_t initialammo;
	int32_t maxupgradedammo;
	int32_t boxammo;
	int32_t backpackammo;
	int32_t weaponammo;
	int32_t droppedclipammo;
	int32_t droppedboxammo;
	int32_t droppedbackpackammo;
	int32_t droppedweaponammo;
	int32_t deathmatchweaponammo;
	fixed_t skillmul[ sk_max ];

	// Values used at load time by dehacked
	doombool recalculateFromOriginalValues;
} ammoinfo_t;

DOOM_C_API typedef enum weaponflagsmbf21_e
{
	WF_NONE						= 0x00000000,
	WF_MBF21_NOTHRUST			= 0x00000001,
	WF_MBF21_SILENT				= 0x00000002,
	WF_MBF21_NOAUTOFIRE			= 0x00000004,
	WF_MBF21_FLEEMELEE			= 0x00000008,
	WF_MBF21_AUTOSWITCHFROM		= 0x00000010,
	WF_MBF21_NOAUTOSWITCHTO		= 0x00000020,
} weaponflagsmbf21_t;

// Weapon info: sprite frames, ammunition use.
DOOM_C_API typedef struct weaponinfo_s
{
	// RNR extensions
	int32_t			index;
	int32_t			slot;
	int32_t			slotpriority;
	int32_t			switchpriority;
	GameMode_t		mingamemode;
	doombool		initialowned;
	doombool		initialraised;
	int32_t			allowswitchifownedweapon;
	int32_t			noswitchifownedweapon;
	int32_t			allowswitchifowneditem;
	int32_t			noswitchifowneditem;

	// Vanilla values
	int32_t			ammo;
	int32_t			upstate;
	int32_t			downstate;
	int32_t			readystate;
	int32_t			atkstate;
	int32_t			flashstate;

	// MBF21 extensions
	int32_t			mbf21flags;
	int32_t			ammopershot;

#if defined( __cplusplus )
	constexpr bool SwitchFromOnAmmoPickup() const		{ return mbf21flags & WF_MBF21_AUTOSWITCHFROM; }
	constexpr bool DontSwitchToOnAmmoPickup() const		{ return mbf21flags & WF_MBF21_NOAUTOSWITCHTO; }
	constexpr bool NoAutofire() const					{ return mbf21flags & WF_MBF21_NOAUTOFIRE; }
	constexpr bool NoThrusting() const					{ return mbf21flags & WF_MBF21_NOTHRUST; }
	constexpr bool Silent() const						{ return mbf21flags & WF_MBF21_SILENT; }

	INLINE const ammoinfo_t& AmmoInfo() const;
#endif // defined( __cplusplus )
} weaponinfo_t;

#if defined( __cplusplus )
struct DoomWeapons
{
	weaponinfo_t** _begin;
	weaponinfo_t** _end;

	constexpr weaponinfo_t** begin()								{ return _begin; }
	constexpr weaponinfo_t** end()									{ return _end; }
};

class DoomWeaponLookup
{
public:
	inline const weaponinfo_t& operator[]( int32_t weapon )			{ return Fetch( weapon ); }
	inline const weaponinfo_t& operator[]( weapontype_t weapon )	{ return Fetch( (int32_t)weapon ); }

	int32_t					size();
	DoomWeapons				All();
	DoomWeapons				BySwitchPriority();
	const weaponinfo_t*		NextInSlot( int32_t slot );
	const weaponinfo_t*		NextInSlot( const weaponinfo_t* info );

protected:
	weaponinfo_t&	Fetch( int32_t weapon );
};

struct DoomAmmos
{
	ammoinfo_t** _begin;
	ammoinfo_t** _end;

	constexpr ammoinfo_t** begin()									{ return _begin; }
	constexpr ammoinfo_t** end()									{ return _end; }
};

class DoomAmmoLookup
{
public:
	INLINE const ammoinfo_t& operator[]( int32_t ammo )				{ return Fetch( ammo ); }
	INLINE const ammoinfo_t& operator[]( ammotype_t ammo )			{ return Fetch( (int32_t)ammo ); }

	int32_t					size();
	DoomAmmos				All();

protected:
	ammoinfo_t&	Fetch( int32_t ammo );
};

extern DoomWeaponLookup weaponinfo;
extern DoomAmmoLookup ammoinfo;

const ammoinfo_t& weaponinfo_t::AmmoInfo() const
{
	return ammoinfo[ ammo ];
}

#endif // defined( __cplusplus )

#endif

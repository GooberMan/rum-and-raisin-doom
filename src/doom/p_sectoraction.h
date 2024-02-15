//
// Copyright(C) 2024 Ethan Watson
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

#ifndef __D_SECTORACTION_H__
#define __D_SECTORACTION_H__

#include "doomtype.h"

enum DoomSectorSpecials
{
	DSS_None,
	DSS_LightRandom,
	DSS_LightBlinkHalfSecond,
	DSS_LightBlinkSecond,
	DSS_20DamageAndLightBlink,
	DSS_10Damage,
	DSS_Unused1,
	DSS_5Damage,
	DSS_LightOscillate,
	DSS_Secret,
	DSS_30SecondsClose,
	DSS_20DamageAndEnd,
	DSS_LightBlinkHalfSecondSynchronised,
	DSS_LightBlinkSecondSynchronised,
	DSS_300SecondsOpen,
	DSS_Unused2,
	DSS_20Damage,
	DSS_LightFlicker,

	DSS_Mask = 0x0000001F,
};

enum BoomSectorFlags
{
	BoomSectorVal_Min							= DSS_LightFlicker + 1,

	SectorLight_Normal							= 0x00000000,
	SectorLight_Random							= 0x00000001,
	SectorLight_BlinkHalfSecond					= 0x00000002,
	SectorLight_BlinkSecond						= 0x00000003,
	SectorLight_20DamageAndLightBlink			= 0x00000004,
	SectorLight_Oscillate						= 0x00000008,
	SectorLight_BlinkHalfSecondSynchronised		= 0x0000000C,
	SectorLight_BlinkSecondSynchronised			= 0x0000000D,
	SectorLight_Flicker							= 0x00000010,

	SectorLight_Mask							= 0x0000001F,

	SectorDamage_None							= 0x00000000,
	SectorDamage_5								= 0x00000020,
	SectorDamage_10								= 0x00000040,
	SectorDamage_20								= 0x00000060,

	SectorDamage_Mask							= 0x00000060,
	SectorDamage_Shift							= 5,

	SectorSecret_No								= 0x00000000,
	SectorSecret_Yes							= 0x00000080,
	SectorSecret_Mask							= 0x00000080,

	SectorFriction_No							= 0x00000000,
	SectorFriction_Yes							= 0x00000100,
	SectorFriction_Mask							= 0x00000100,

	SectorWind_No								= 0x00000000,
	SectorWind_Yes								= 0x00000200,
	SectorWind_Mask								= 0x00000200,

	// Unused in Boom but reserved
	SectorSuppressSound_No						= 0x00000000,
	SectorSuppressSound_Yes						= 0x00000400,
	SectorSuppressSound_Mask					= 0x00000400,

	// Unused in Boom but reserved
	SectorNoMovementSound_No					= 0x00000000,
	SectorNoMovementSound_Yes					= 0x00000800,
	SectorNoMovementSound_Mask					= 0x00000800,
};

enum MBF21SectorFlags
{
	MBF21SectorVal_Min							= 0x00001000,

	SectorAltDamage_No							= 0x00000000,
	SectorAltDamage_Yes							= 0x00001000,
	SectorAltDamage_Mask						= 0x00001000,

	SectorAltDamage_KillPlayerWithoutRadsuit	= 0x00000000,
	SectorAltDamage_KillPlayer					= 0x00000020,
	SectorAltDamage_KillPlayersAndExit			= 0x00000040,
	SectorAltDamage_KillPlayersAndSecretExit	= 0x00000060,

	SectorKillGroundEnemies_No					= 0x00000000,
	SectorKillGroundEnemies_Yes					= 0x00020000,
	SectorKillGroundEnemies_Mask				= 0x00020000,
};

#endif // !defined( __D_SECTORACTION_H__ )

//
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
// DESCRIPTION: Sets up the active game

#include "doomstat.h"
#include "p_lineaction.h"

#include "r_defs.h"
#include "p_spec.h"
#include "d_player.h"
#include "z_zone.h"

#include <type_traits>
using underlyinglinetrigger_t = std::underlying_type_t< linetrigger_t >;

constexpr linetrigger_t operator|( const linetrigger_t lhs, const linetrigger_t rhs )
{
	return (linetrigger_t)( (underlyinglinetrigger_t)lhs | (underlyinglinetrigger_t)rhs );
};

constexpr linetrigger_t operator&( const linetrigger_t lhs, const linetrigger_t rhs )
{
	return (linetrigger_t)( (underlyinglinetrigger_t)lhs & (underlyinglinetrigger_t)rhs );
};

constexpr linetrigger_t operator^( const linetrigger_t lhs, const linetrigger_t rhs )
{
	return (linetrigger_t)( (underlyinglinetrigger_t)lhs ^ (underlyinglinetrigger_t)rhs );
};

using underlyinglinelock_t = std::underlying_type_t< linelock_t >;

constexpr linelock_t operator|( const linelock_t lhs, const linelock_t rhs )
{
	return (linelock_t)( (underlyinglinelock_t)lhs | (underlyinglinelock_t)rhs );
};

constexpr linelock_t operator&( const linelock_t lhs, const linelock_t rhs )
{
	return (linelock_t)( (underlyinglinelock_t)lhs & (underlyinglinelock_t)rhs );
};

constexpr linelock_t operator^( const linelock_t lhs, const linelock_t rhs )
{
	return (linelock_t)( (underlyinglinelock_t)lhs ^ (underlyinglinelock_t)rhs );
};

namespace constants
{
	static constexpr fixed_t doorspeeds[] =
	{
		IntToFixed( 2 ),
		IntToFixed( 4 ),
		IntToFixed( 8 ),
		IntToFixed( 16 ),
	};

	static constexpr int32_t doordelay[] =
	{
		35,
		150,
		300,
		1050,
	};

	static constexpr linetrigger_t triggers[] =
	{
		LT_Walk,
		LT_Walk,
		LT_Switch,
		LT_Switch,
		LT_Gun,
		LT_Gun,
		LT_Use,
		LT_Use,
	};
}

namespace precon
{
	constexpr linelock_t lockfromcard[ NUMCARDS ] =
	{
		LL_BlueCard,		// it_bluecard
		LL_YellowCard,		// it_yellowcard
		LL_RedCard,			// it_redcard
		LL_BlueSkull,		// it_blueskull
		LL_YellowSkull,		// it_yellowskull
		LL_RedSkull,		// it_redskull
	};

	// This table is UGLY
	constexpr card_t keyfromlock[] =
	{
		NUMCARDS, NUMCARDS, NUMCARDS, NUMCARDS, NUMCARDS, NUMCARDS, NUMCARDS, NUMCARDS,
		NUMCARDS, it_bluecard, it_yellowcard, NUMCARDS, it_redcard, NUMCARDS, NUMCARDS, NUMCARDS,
		NUMCARDS, it_blueskull, it_yellowskull, NUMCARDS, it_redskull, NUMCARDS, NUMCARDS, NUMCARDS,
	};

	constexpr card_t GetExactKeyFor( linelock_t lock )
	{
		linelock_t linecard = lock & LL_CardMask;
		card_t card = keyfromlock[ linecard ];
		return card;
	}

	constexpr card_t GetCardFor( linelock_t lock )
	{
		linelock_t linecard = lock & LL_ColorMask;
		card_t card = keyfromlock[ linecard | LL_Card ];
		return card;
	}

	constexpr card_t GetSkullFor( linelock_t lock )
	{
		linelock_t linecard = lock & LL_ColorMask;
		card_t card = keyfromlock[ linecard | LL_Skull ];
		return card;
	}

	constexpr bool ActivationMatches( const linetrigger_t lhs, const linetrigger_t rhs )
	{
		return (lhs & LT_ActivationTypeMask) == (rhs & LT_ActivationTypeMask);
	}

	bool IsAnyThing( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return ActivationMatches( line->action->trigger, activationtype );
	}

	bool CanDoorRaise( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return ( activator->player != nullptr || ( line->flags & ML_SECRET ) != ML_SECRET )
			&& ActivationMatches( line->action->trigger, activationtype );
	}

	bool IsPlayer( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return activator->player != nullptr && ActivationMatches( line->action->trigger, activationtype );
	}

	bool IsMonster( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return activator->player == nullptr && ActivationMatches( line->action->trigger, activationtype );
	}

	bool HasExactKey( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
			&& activator->player->cards[ GetExactKeyFor( line->action->lock ) ];
	}

	bool HasAnyKeyOfColour( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
			&& ( activator->player->cards[ GetCardFor( line->action->lock ) ]
				|| activator->player->cards[ GetSkullFor( line->action->lock ) ] );
	}

	bool HasAnyKeys( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
			&& ( activator->player->cards[ it_bluecard ]
				|| activator->player->cards[ it_yellowcard ]
				|| activator->player->cards[ it_redcard ]
				|| activator->player->cards[ it_blueskull ]
				|| activator->player->cards[ it_yellowskull ]
				|| activator->player->cards[ it_redskull ]
				);
	}

	bool HasAllColours( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		if( IsPlayer( line, activator, activationtype, activationside ) )
		{
			bool hasallcards = activator->player->cards[ it_bluecard ]
								&& activator->player->cards[ it_yellowcard ]
								&& activator->player->cards[ it_redcard ];
			bool hasallskulls = activator->player->cards[ it_blueskull ]
								&& activator->player->cards[ it_yellowskull ]
								&& activator->player->cards[ it_redskull ];

			return hasallcards || hasallskulls;
		}
		return false;
	}

	bool HasAllCards( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
				&& activator->player->cards[ it_bluecard ]
				&& activator->player->cards[ it_yellowcard ]
				&& activator->player->cards[ it_redcard ];
	}

	bool HasAllSkulls( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
				&& activator->player->cards[ it_blueskull ]
				&& activator->player->cards[ it_yellowskull ]
				&& activator->player->cards[ it_redskull ];
	}

	bool HasAllKeys( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
				&& activator->player->cards[ it_bluecard ]
				&& activator->player->cards[ it_yellowcard ]
				&& activator->player->cards[ it_redcard ]
				&& activator->player->cards[ it_blueskull ]
				&& activator->player->cards[ it_yellowskull ]
				&& activator->player->cards[ it_redskull ];
	}
}

enum doordelay_t
{
	delay_1sec,
	delay_4sec,
	delay_9sec,
	delay_30sec,
};

static void DoGenericDoor( line_t* line )
{
	EV_DoDoorGeneric( line );
}

static void DoGenericDoorOnce( line_t* line )
{
	EV_DoDoorGeneric( line );
	line->action = nullptr;
	line->special = 0;
}

static void DoGenericDoorSwitch( line_t* line )
{
	if( EV_DoDoorGeneric( line ) )
	{
		P_ChangeSwitchTexture( line, 1 );
	}
}

static void DoGenericDoorSwitchOnce( line_t* line )
{
	if( EV_DoDoorGeneric( line ) )
	{
		P_ChangeSwitchTexture( line, 0 );
		line->action = nullptr;
		line->special = 0;
	}
}

constexpr lineaction_t DoomLineActions[ DoomActions_Max ] =
{
	// Unknown_000
	{},
	// Door_Raise_UR_All
	{ &precon::CanDoorRaise, &DoGenericDoor, LT_Use, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ delay_4sec ], doordir_open },
	// Door_Open_W1_Player
	{ &precon::IsPlayer, &DoGenericDoorOnce, LT_Walk, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Door_Close_W1_Player
	{ &precon::IsPlayer, &DoGenericDoorOnce, LT_Walk, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_close },
	// Door_Raise_W1_Player
	{ &precon::IsPlayer, &DoGenericDoorOnce, LT_Walk, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ delay_4sec ], doordir_open },
	// Floor_RaiseLowestCeiling_W1_Player
	{},
	// Ceiling_CrusherFast_W1_Player
	{},
	// Stairs_BuildBy8_S1_Player
	{},
	// Stairs_BuildBy8_W1_Player
	{},
	// Sector_Donut_S1_Player
	{},
	// Platform_DownWaitUp_W1_All
	{},
	// Exit_Normal_S1_Player
	{},
	// Light_SetBrightest_W1_Player
	{},
	// Light_SetTo255_W1_Player
	{},
	// Floor_Raise32ChangeTexture_S1_Player
	{},
	// Floor_Raise24ChangeTexture_S1_Player
	{},
	// Door_Close30Open_W1_Player
	{ &precon::IsPlayer, &DoGenericDoorOnce, LT_Walk, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ delay_30sec ], doordir_close },
	// Light_Strobe_W1_Player
	{},
	// Floor_RaiseNearest_S1_Player
	{},
	// Floor_LowerHighest_W1_Player
	{},
	// Floor_RaiseNearestChangeTexture_S1_Player
	{},
	// Platform_DownWaitUp_S1_Player
	{},
	// Floor_RaiseNearestChangeTexture_W1_Player
	{},
	// Floor_LowerLowest_S1_Player
	{},
	// Floor_RaiseLowestCeiling_GR_Player
	{},
	// Ceiling_Crusher_W1_Player
	{},
	// Door_RaiseBlue_UR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoor, LT_Use, LL_Blue, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ delay_4sec ], doordir_open },
	// Door_RaiseYellow_UR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoor, LT_Use, LL_Yellow, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ delay_4sec ], doordir_open },
	// Door_RaiseRed_UR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoor, LT_Use, LL_Red, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ delay_4sec ], doordir_open },
	// Door_Raise_S1_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitchOnce, LT_Switch, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ delay_4sec ], doordir_open },
	// Floor_RaiseByTexture_W1_Player
	{},
	// Door_Open_U1_Player
	{ &precon::IsPlayer, &DoGenericDoorOnce, LT_Use, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Door_OpenBlue_U1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoorOnce, LT_Use, LL_Blue, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Door_OpenRed_U1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoorOnce, LT_Use, LL_Red, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Door_OpenYellow_U1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoorOnce, LT_Use, LL_Yellow, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Light_SetTo35_W1_Player
	{},
	// Floor_LowerHighestFast_W1_Player
	{},
	// Floor_LowerLowestChangeTexture_NumericModel_W1_Player
	{},
	// Floor_LowerLowest_W1_Player
	{},
	// Teleport_W1_All
	{},
	// Sector_RaiseCeilingLowerFloor_W1_Player
	{},
	// Ceiling_LowerToFloor_S1_Player
	{},
	// Door_Close_SR_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitch, LT_Switch, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_close },
	// Ceiling_LowerToFloor_SR_Player
	{},
	// Ceiling_LowerCrush_Player
	{},
	// Floor_LowerHighest_SR_Player
	{},
	// Door_Open_GR_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitch, LT_Gun, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Floor_RaiseNearestChangeTexture_GR_Player
	{},
	// Scroll_WallTextureLeft_Always
	{},
	// Ceiling_Crusher_S1_Player
	{},
	// Door_Close_S1_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitchOnce, LT_Switch, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_close },
	// Exit_Secret_S1_Player
	{},
	// Exit_Normal_W1_Player
	{},
	// Platform_Perpetual_W1_Player
	{},
	// Platform_Stop_W1_Player
	{},
	// Floor_RaiseCrush_S1_Player
	{},
	// Floor_RaiseCrush_W1_Player
	{},
	// Ceiling_CrusherStop_W1_Player
	{},
	// Floor_Raise24_W1_Player
	{},
	// Floor_Raise24ChangeTexture_W1_Player
	{},
	// Floor_LowerLowest_SR_Player
	{},
	// Door_Open_SR_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitch, LT_Switch, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Platform_DownWaitUp_SR_Player
	{},
	// Door_Raise_SR_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitch, LT_Switch, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ delay_4sec ], doordir_open },
	// Floor_RaiseLowestCeiling_SR_player
	{},
	// Floor_RaiseCrush_SR_Player
	{},
	// Floor_Raise24ChangeTexture_SR_Player
	{},
	// Floor_Raise32ChangeTexture_SR_Player
	{},
	// Floor_RaiseNearestChangeTexture_SR_Player
	{},
	// Floor_RaiseNearest_SR_Player
	{},
	// Floor_LowerHighestFast_SR_Player
	{},
	// Floor_LowerHighestFast_S1_Player
	{},
	// Ceiling_LowerCrush_WR_Player
	{},
	// Ceiling_Crusher_WR_Player
	{},
	// Ceiling_CrusherStop_WR_Player
	{},
	// Door_Close_WR_Player
	{ &precon::IsPlayer, &DoGenericDoor, LT_Walk, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_close },
	// Door_Close30Open_WR_Player
	{ &precon::IsPlayer, &DoGenericDoor, LT_Walk, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ delay_30sec ], doordir_close },
	// Ceiling_CrusherFast_WR_Player
	{},
	// Unknown_078
	{},
	// Light_SetTo35_WR_Player
	{},
	// Light_SetBrightest_WR_Player
	{},
	// Light_SetTo255_WR_Player
	{},
	// Floor_LowerLowest_WR_Player
	{},
	// Floor_LowerHighest_WR_Player
	{},
	// Floor_LowerLowestChangeTexture_NumericModel_WR_Player
	{},
	// Unknown_085
	{},
	// Door_Open_WR_Player
	{ &precon::IsPlayer, &DoGenericDoor, LT_Walk, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Platform_Perpetual_WR_Player
	{},
	// Platform_DownWaitUp_WR_All
	{},
	// Platform_Stop_WR_Player
	{},
	// Door_Raise_WR_Player
	{ &precon::IsPlayer, &DoGenericDoor, LT_Walk, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ delay_4sec ], doordir_open },
	// Floor_RaiseLowestCeiling_WR_Player
	{},
	// Floor_Raise24_WR_Player
	{},
	// Floor_Raise24ChangeTexture_WR_Player
	{},
	// Floor_RaiseCrush_WR_Player
	{},
	// Floor_RaiseNearestChangeTexture_WR_Player
	{},
	// Floor_RaiseByTexture_WR_Player
	{},
	// Teleport_WR_All
	{},
	// Floor_LowerHighestFast_WR_Player
	{},
	// Door_OpenFastBlue_SR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoorSwitch, LT_Switch, LL_Blue, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Stairs_BuildBy16Fast_W1_Player
	{},
	// Floor_RaiseLowestCeiling_S1_Player
	{},
	// Floor_LowerHighest_S1_Player
	{},
	// Door_Open_S1_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitchOnce, LT_Switch, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Light_SetLowest_W1_Player
	{},
	// Door_RaiseFast_WR_Player
	{ &precon::IsPlayer, &DoGenericDoor, LT_Walk, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ delay_4sec ], doordir_open },
	// Door_OpenFast_WR_Player
	{ &precon::IsPlayer, &DoGenericDoor, LT_Walk, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_CloseFast_WR_Player
	{ &precon::IsPlayer, &DoGenericDoor, LT_Walk, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_close },
	// Door_RaiseFast_W1_Player
	{ &precon::IsPlayer, &DoGenericDoorOnce, LT_Walk, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ delay_4sec ], doordir_open },
	// Door_OpenFast_W1_Player
	{ &precon::IsPlayer, &DoGenericDoorOnce, LT_Walk, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_CloseFast_W1_Player
	{ &precon::IsPlayer, &DoGenericDoorOnce, LT_Walk, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_close },
	// Door_RaiseFast_S1_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitchOnce, LT_Switch, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ delay_4sec ], doordir_open },
	// Door_OpenFast_S1_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitchOnce, LT_Switch, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_CloseFast_S1_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitchOnce, LT_Switch, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_close },
	// Door_RaiseFast_SR_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitch, LT_Switch, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ delay_4sec ], doordir_open },
	// Door_OpenFast_SR_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitch, LT_Switch, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_CloseFast_SR_Player
	{ &precon::IsPlayer, &DoGenericDoorSwitch, LT_Switch, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_close },
	// Door_RaiseFast_UR_All
	{ &precon::IsPlayer, &DoGenericDoor, LT_Use, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ delay_4sec ], doordir_open },
	// Door_RaiseFast_U1_Player
	{ &precon::IsPlayer, &DoGenericDoorOnce, LT_Use, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ delay_4sec ], doordir_open },
	// Floor_RaiseNearest_W1_Player
	{},
	// Platform_DownWaitUpFast_WR_Player
	{},
	// Platform_DownWaitUpFast_W1_Player
	{},
	// Platform_DownWaitUpFast_S1_Player
	{},
	// Platform_DownWaitUpFast_SR_Player
	{},
	// Exit_Secret_W1_Player
	{},
	// Teleport_W1_Monsters
	{},
	// Teleport_WR_Monsters
	{},
	// Stairs_BuildBy16Fast_S1_Player
	{},
	// Floor_RaiseNearest_WR_Player
	{},
	// Floor_RaiseNearestFast_WR_Player
	{},
	// Floor_RaiseNearestFast_W1_Player
	{},
	// Floor_RaiseNearestFast_S1_Player
	{},
	// Floor_RaiseNearestFast_SR_Player
	{},
	// Door_OpenFastBlue_S1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoorSwitchOnce, LT_Switch, LL_Blue, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_OpenFastRed_SR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoorSwitch, LT_Switch, LL_Red, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_OpenFastRed_S1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoorSwitchOnce, LT_Switch, LL_Red, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_OpenFastYellow_SR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoorSwitch, LT_Switch, LL_Yellow, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_OpenFastYellow_S1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericDoorSwitch, LT_Switch, LL_Yellow, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Light_SetTo255_SR_Player
	{},
	// Light_SetTo35_SR_Player
	{},
	// Floor_Raise512_S1_Player
	{},
	// Ceiling_CrusherSilent_W1_Player
	{},
};

#pragma optimize( "", off )

lineaction_t* GetDoomLineAction( line_t* line )
{
	return (lineaction_t*)&DoomLineActions[ line->special ];
}

lineaction_t* GetBoomLineAction( line_t* line )
{
	return nullptr;
}

lineaction_t* GetMBFSkyLineAction( line_t* line )
{
	return nullptr;
}

lineaction_t* CreateBoomGeneralisedDoorAction( line_t* line )
{
	lineaction_t* action = (lineaction_t*)Z_MallocAs( lineaction_t, PU_LEVEL, nullptr );

	uint32_t trigger = line->special & Generic_Trigger_Mask;
	bool manualuse = trigger == Generic_Trigger_U1 || trigger == Generic_Trigger_UR;
	bool isswitch = trigger == Generic_Trigger_S1 || trigger == Generic_Trigger_SR
					|| trigger == Generic_Trigger_G1 || trigger == Generic_Trigger_GR;
	bool repeatable = ( line->special & Generic_Trigger_Repeatable ) == Generic_Trigger_Repeatable;
	uint32_t speed = ( line->special & Generic_Speed_Mask ) >> Generic_Speed_Shift;
	uint32_t kind = ( line->special & Door_Type_Mask );
	bool isdown = kind == Door_Type_CloseWaitOpen || kind == Door_Type_Close;
	bool isup = kind == Door_Type_OpenWaitClose || kind == Door_Type_Open;
	bool isdelayable = kind == Door_Type_CloseWaitOpen || kind == Door_Type_OpenWaitClose;
	bool allowmonster = ( line->special & Door_AllowMonsters_Yes ) == Door_AllowMonsters_Yes;
	uint32_t delay = isdelayable ? ( line->special & Door_Delay_Mask ) >> Door_Delay_Shift : 0;

	if( manualuse )
	{
		action->precondition = allowmonster ? &precon::CanDoorRaise : &precon::IsPlayer;
	}
	{
		action->precondition = allowmonster ? &precon::IsAnyThing : &precon::IsPlayer;
	}

	if( isswitch )
	{
		action->action = repeatable ? &DoGenericDoorSwitch : &DoGenericDoorSwitchOnce;
	}
	{
		action->action = repeatable ? &DoGenericDoor : &DoGenericDoorOnce;
	}
	action->trigger = constants::triggers[ trigger ];
	action->lock = LL_None;
	action->speed = constants::doorspeeds[ speed ];
	action->delay = constants::doordelay[ delay ];
	action->param1 = isup ? doordir_open
						: isdown ? doordir_close
							: doordir_none;

	return action;
}

lineaction_t* CreateBoomGeneralisedLineAction( line_t* line )
{
	if( line->special >= Generic_Door && line->special < Generic_Ceiling )
	{
		return CreateBoomGeneralisedDoorAction( line );
	}
	return nullptr;
}

DOOM_C_API lineaction_t* P_GetLineActionFor( line_t* line )
{
	return nullptr;
#if 0
	if( line->special >= DoomActions_Min && line->special < DoomActions_Max
		&& line->special != Unknown_078 
		&& line->special != Unknown_085 )
	{
		return GetDoomLineAction( line );
	}
	else if( remove_limits )
	{
		if( /*allow_boom_specials && */
			line->special >= BoomActions_Min && line->special < BoomActions_Max )
		{
			return GetBoomLineAction( line );
		}
		else if( /*allow_boom_specials && */
				line->special >= Generic_Min && line->special < Generic_Max )
		{
			return CreateBoomGeneralisedLineAction( line );
		}
		else if( /*( allow_mbf_specials || allow_mbf_sky_specials ) && */
				( line->special == Transfer_Sky_Always
					|| line->special == Transfer_SkyReversed_Always )
				)
		{
			return GetMBFSkyLineAction( line );
		}
	}

	return nullptr;
#endif
}

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

#include "dstrings.h"

#include "d_player.h"

#include "deh_str.h"

#include "p_spec.h"

#include "r_defs.h"

#include "s_sound.h"

#include "z_zone.h"

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

	static constexpr int32_t liftspeeds[] =
	{
		IntToFixed( 2 ),
		IntToFixed( 4 ),
		IntToFixed( 8 ),
		IntToFixed( 16 ),
	};

	static constexpr int32_t perpetualplatformspeeds[] =
	{
		IntToFixed( 1 ),
		IntToFixed( 2 ),
		IntToFixed( 4 ),
		IntToFixed( 8 ),
	};


	static constexpr int32_t liftdelay[] =
	{
		35,
		105,
		165,
		350,
	};

	static constexpr linetrigger_t triggers[] =
	{
		LT_Walk | LT_BothSides,
		LT_Walk | LT_BothSides,
		LT_Switch | LT_FrontSide,
		LT_Switch | LT_FrontSide,
		LT_Gun | LT_FrontSide,
		LT_Gun | LT_FrontSide,
		LT_Use | LT_BothSides,
		LT_Use | LT_BothSides,
	};

	static constexpr linelock_t locks_equivalent[] =
	{
		LL_AnyKey,
		LL_AnyRed,
		LL_AnyBlue,
		LL_AnyYellow,
		LL_AnyRed,
		LL_AnyBlue,
		LL_AnyYellow,
		LL_AllCardsOrSkulls,
	};

	static constexpr linelock_t locks_separate[] =
	{
		LL_AnyKey,
		LL_RedCard,
		LL_BlueCard,
		LL_YellowCard,
		LL_RedSkull,
		LL_BlueSkull,
		LL_YellowSkull,
		LL_AllKeys,
	};

	constexpr linetrigger_t sides[] =
	{
		LT_FrontSide,
		LT_BackSide,
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
		return ( lhs & LT_ActivationTypeMask ) == ( rhs & LT_ActivationTypeMask );
	}

	constexpr bool SideMatches( const linetrigger_t trigger, int32_t activationside )
	{
		return ( ( trigger & LT_SidesMask ) & constants::sides[ activationside ] ) != LT_None;
	}

	constexpr bool ValidPlayer( mobj_t* activator, const linetrigger_t trigger, int32_t activationside )
	{
		return activator->player != nullptr && SideMatches( trigger, activationside );
	}

	constexpr bool ValidMonster( mobj_t* activator )
	{
		return activator->player == nullptr;
	}

	constexpr bool ValidAnyActivator( mobj_t* activator, const linetrigger_t trigger, int32_t activationside )
	{
		return activator->player == nullptr || SideMatches( trigger, activationside );
	}

	constexpr bool NeverActivate( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return false;
	}

	constexpr bool IsAnyThing( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return ValidAnyActivator( activator, line->action->trigger, activationside )
			&& ActivationMatches( line->action->trigger, activationtype );
	}

	constexpr bool IsPlayer( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return ValidPlayer( activator, line->action->trigger, activationside ) && ActivationMatches( line->action->trigger, activationtype );
	}

	constexpr bool IsMonster( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return ValidMonster( activator );
	}

	constexpr bool ValidDoorActivator( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return activator->player == nullptr ? ( line->flags & ML_SECRET ) != ML_SECRET
											: SideMatches( line->action->trigger, activationside );
	}

	constexpr bool CanDoorRaise( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return ValidDoorActivator( line, activator, line->action->trigger, activationside )
			&& ActivationMatches( line->action->trigger, activationtype );
	}

	constexpr bool CanTeleportAll( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return SideMatches( line->action->trigger, activationside )
			&& ActivationMatches( line->action->trigger, activationtype );
	}

	constexpr bool CanTeleportMonster( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return ValidMonster( activator )
			&& SideMatches( line->action->trigger, activationside )
			&& ActivationMatches( line->action->trigger, activationtype );
	}

	constexpr bool HasExactKey( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
			&& activator->player->cards[ GetExactKeyFor( line->action->lock ) ];
	}

	constexpr bool HasAnyKeyOfColour( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
			&& ( activator->player->cards[ GetCardFor( line->action->lock ) ]
				|| activator->player->cards[ GetSkullFor( line->action->lock ) ] );
	}

	constexpr bool HasAnyCard( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
			&& ( activator->player->cards[ it_bluecard ]
				|| activator->player->cards[ it_yellowcard ]
				|| activator->player->cards[ it_redcard ]
				);
	}

	constexpr bool HasAnySkull( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
			&& ( activator->player->cards[ it_blueskull ]
				|| activator->player->cards[ it_yellowskull ]
				|| activator->player->cards[ it_redskull ]
				);
	}

	constexpr bool HasAnyKey( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
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

	constexpr bool HasAllColours( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
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

	constexpr bool HasAllCards( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
				&& activator->player->cards[ it_bluecard ]
				&& activator->player->cards[ it_yellowcard ]
				&& activator->player->cards[ it_redcard ];
	}

	constexpr bool HasAllSkulls( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
				&& activator->player->cards[ it_blueskull ]
				&& activator->player->cards[ it_yellowskull ]
				&& activator->player->cards[ it_redskull ];
	}

	constexpr bool HasAllCardsOrSkulls( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return  IsPlayer( line, activator, activationtype, activationside )
				&& ( ( activator->player->cards[ it_bluecard ]
						&& activator->player->cards[ it_yellowcard ]
						&& activator->player->cards[ it_redcard ] )
					|| ( activator->player->cards[ it_blueskull ]
						&& activator->player->cards[ it_yellowskull ]
						&& activator->player->cards[ it_redskull ] ) );
	}

	constexpr bool HasAllKeys( line_t* line, mobj_t* activator, linetrigger_t activationtype, int32_t activationside )
	{
		return IsPlayer( line, activator, activationtype, activationside )
				&& activator->player->cards[ it_bluecard ]
				&& activator->player->cards[ it_yellowcard ]
				&& activator->player->cards[ it_redcard ]
				&& activator->player->cards[ it_blueskull ]
				&& activator->player->cards[ it_yellowskull ]
				&& activator->player->cards[ it_redskull ];
	}

	static constexpr linepreconfunc_t LockLookup_Equivalent[] =
	{
		&HasAnyKey,
		&HasAnyKeyOfColour,
		&HasAnyKeyOfColour,
		&HasAnyKeyOfColour,
		&HasAnyKeyOfColour,
		&HasAnyKeyOfColour,
		&HasAnyKeyOfColour,
		&HasAllCards,
	};

	static constexpr linepreconfunc_t LockLookup_Separate[] =
	{
		&HasAnyKey,
		&HasExactKey,
		&HasExactKey,
		&HasExactKey,
		&HasExactKey,
		&HasExactKey,
		&HasExactKey,
		HasAllCardsOrSkulls,
	};}

enum doordelay_t
{
	doordelay_1sec,
	doordelay_4sec,
	doordelay_9sec,
	doordelay_30sec,
};

enum liftdelay_t
{
	liftdelay_1sec,
	liftdelay_3sec,
	liftdelay_5sec,
	liftdelay_10sec,
};

#define MakeGenericFunc( x, func ) \
struct x \
{ \
	static inline int32_t Perform( line_t* line, mobj_t* activator ) { return func ( line, activator ); } \
}

MakeGenericFunc( Door, EV_DoDoorGeneric );
MakeGenericFunc( Lift, EV_DoLiftGeneric );
MakeGenericFunc( PerpetualLiftStart, EV_DoPerpetualLiftGeneric );
MakeGenericFunc( PlatformStop, EV_StopAnyLiftGeneric );
MakeGenericFunc( Teleport, EV_DoTeleportGeneric );
MakeGenericFunc( Exit, EV_DoExitGeneric );
MakeGenericFunc( LightSet, EV_DoLightSetGeneric );

template< typename func >
static void DoGeneric( line_t* line, mobj_t* activator )
{
	func::Perform( line, activator );
}

template< typename func >
static void DoGenericOnce( line_t* line, mobj_t* activator )
{
	if( func::Perform( line, activator ) )
	{
		line->action = nullptr;
		line->special = 0;
	}
}

template< typename func >
static void DoGenericSwitch( line_t* line, mobj_t* activator )
{
	if( func::Perform( line, activator ) )
	{
		P_ChangeSwitchTexture( line, 1 );
	}
}

template< typename func >
static void DoGenericSwitchOnce( line_t* line, mobj_t* activator )
{
	if( func::Perform( line, activator ) )
	{
		P_ChangeSwitchTexture( line, 0 );
		line->action = nullptr;
		line->special = 0;
	}
}

constexpr lineaction_t builtinlineactions[ Actions_BuiltIn_Count ] =
{
	/* Doom specials */

	// Unknown_000
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Door_Raise_UR_All
	{ &precon::CanDoorRaise, &DoGeneric< Door >, LT_Use | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Door_Open_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Door_Close_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_close },
	// Door_Raise_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Floor_RaiseLowestCeiling_W1_Player
	{},
	// Crusher_Fast_W1_Player
	{},
	// Stairs_BuildBy8_S1_Player
	{},
	// Stairs_BuildBy8_W1_Player
	{},
	// Sector_Donut_S1_Player
	{},
	// Platform_DownWaitUp_W1_All
	{ &precon::IsAnyThing, &DoGenericOnce< Lift >, LT_Walk | LT_BothSides, LL_None, constants::liftspeeds[ Speed_Normal ], constants::liftdelay[ liftdelay_3sec ], pt_lowestneighborfloor },
	// Exit_Normal_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Exit >, LT_Switch | LT_FrontSide, LL_None, 0, 0, exit_normal },
	// Light_SetBrightest_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< LightSet >, LT_Walk | LT_BothSides, LL_None, 0, 0, lightset_highestsurround_firsttagged },
	// Light_SetTo255_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< LightSet >, LT_Walk | LT_BothSides, LL_None, 0, 0, lightset_value, 255 },
	// Platform_Raise32ChangeTexture_S1_Player
	{},
	// Platform_Raise24ChangeTexture_S1_Player
	{},
	// Door_Close30Open_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_30sec ], doordir_close },
	// Light_Strobe_W1_Player
	{},
	// Floor_RaiseNearest_S1_Player
	{},
	// Floor_LowerHighest_W1_Player
	{},
	// Platform_RaiseNearestChangeTexture_S1_Player
	{},
	// Platform_DownWaitUp_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Lift >, LT_Switch | LT_FrontSide, LL_None, constants::liftspeeds[ Speed_Normal ], constants::liftdelay[ liftdelay_3sec ], pt_lowestneighborfloor },
	// Platform_RaiseNearestChangeTexture_W1_Player
	{},
	// Floor_LowerLowest_S1_Player
	{},
	// Floor_RaiseLowestCeiling_GR_Player
	{},
	// Crusher_W1_Player
	{},
	// Door_RaiseBlue_UR_Player
	{ &precon::HasAnyKeyOfColour, &DoGeneric< Door >, LT_Use | LT_FrontSide, LL_Blue, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Door_RaiseYellow_UR_Player
	{ &precon::HasAnyKeyOfColour, &DoGeneric< Door >, LT_Use | LT_FrontSide, LL_Yellow, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Door_RaiseRed_UR_Player
	{ &precon::HasAnyKeyOfColour, &DoGeneric< Door >, LT_Use | LT_FrontSide, LL_Red, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Door_Raise_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Floor_RaiseByTexture_W1_Player
	{},
	// Door_Open_U1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_Use | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Door_OpenBlue_U1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericOnce< Door >, LT_Use | LT_FrontSide, LL_Blue, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Door_OpenRed_U1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericOnce< Door >, LT_Use | LT_FrontSide, LL_Red, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Door_OpenYellow_U1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericOnce< Door >, LT_Use | LT_FrontSide, LL_Yellow, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Light_SetTo35_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< LightSet >, LT_Walk | LT_BothSides, LL_None, 0, 0, lightset_value, 35 },
	// Floor_LowerHighestFast_W1_Player
	{},
	// Floor_LowerLowestChangeTexture_NumericModel_W1_Player
	{},
	// Floor_LowerLowest_W1_Player
	{},
	// Teleport_Thing_W1_All
	{ &precon::CanTeleportAll, &DoGenericOnce< Teleport >, LT_Walk | LT_BothSides, LL_None },
	// Ceiling_RaiseHighestCeiling_W1_Player
	{},
	// Ceiling_LowerToFloor_S1_Player
	{},
	// Door_Close_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_close },
	// Ceiling_LowerToFloor_SR_Player
	{},
	// Ceiling_LowerTo8AboveFloor_W1_Player
	{},
	// Floor_LowerHighest_SR_Player
	{},
	// Door_Open_GR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_Gun, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Platform_RaiseNearestChangeTexture_G1_Player
	{},
	// Scroll_WallTextureLeft_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Crusher_S1_Player
	{},
	// Door_Close_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_close },
	// Exit_Secret_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Exit >, LT_Switch | LT_FrontSide, LL_None, 0, 0, exit_secret },
	// Exit_Normal_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Exit >, LT_Walk | LT_BothSides, LL_None, 0, 0, exit_normal },
	// Platform_Perpetual_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< PerpetualLiftStart >, LT_Walk | LT_BothSides, LL_None, constants::perpetualplatformspeeds[ Speed_Slow ], constants::liftdelay[ liftdelay_3sec ] },
	// Platform_Stop_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< PlatformStop >, LT_Walk | LT_BothSides, LL_None },
	// Floor_RaiseCrush_S1_Player
	{},
	// Floor_RaiseCrush_W1_Player
	{},
	// Crusher_Stop_W1_Player
	{},
	// Floor_Raise24_W1_Player
	{},
	// Floor_Raise24ChangeTexture_W1_Player
	{},
	// Floor_LowerLowest_SR_Player
	{},
	// Door_Open_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Platform_DownWaitUp_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Lift >, LT_Switch | LT_FrontSide, LL_None, constants::liftspeeds[ Speed_Normal ], constants::liftdelay[ liftdelay_3sec ], pt_lowestneighborfloor },
	// Door_Raise_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Floor_RaiseLowestCeiling_SR_player
	{},
	// Floor_RaiseCrush_SR_Player
	{},
	// Platform_Raise24ChangeTexture_SR_Player
	{},
	// Platform_Raise32ChangeTexture_SR_Player
	{},
	// Floor_RaiseNearestChangeTexture_SR_Player
	{},
	// Floor_RaiseNearest_SR_Player
	{},
	// Floor_LowerHighestFast_SR_Player
	{},
	// Floor_LowerHighestFast_S1_Player
	{},
	// Ceiling_LowerTo8AboveFloor_WR_Player
	{},
	// Crusher_WR_Player
	{},
	// Crusher_Stop_WR_Player
	{},
	// Door_Close_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_close },
	// Door_Close30Open_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_30sec ], doordir_close },
	// Crusher_Fast_WR_Player
	{},
	// Doom: Unknown_078
	// Boom: Floor_ChangeTexture_NumericModel_SR_Player
	{},
	// Light_SetTo35_WR_Player
	{ &precon::IsPlayer, &DoGeneric< LightSet >, LT_Walk | LT_BothSides, LL_None, 0, 0, lightset_value, 35 },
	// Light_SetBrightest_WR_Player
	{ &precon::IsPlayer, &DoGeneric< LightSet >, LT_Walk | LT_BothSides, LL_None, 0, 0, lightset_highestsurround_firsttagged },
	// Light_SetTo255_WR_Player
	{ &precon::IsPlayer, &DoGeneric< LightSet >, LT_Walk | LT_BothSides, LL_None, 0, 0, lightset_value, 35 },
	// Floor_LowerLowest_WR_Player
	{},
	// Floor_LowerHighest_WR_Player
	{},
	// Floor_LowerLowestChangeTexture_NumericModel_WR_Player
	{},
	// Doom: Unknown_085
	// Boom: Scroll_WallTextureRight_Always
	{},
	// Door_Open_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Slow ], 0, doordir_open },
	// Platform_Perpetual_WR_Player
	{ &precon::IsPlayer, &DoGeneric< PerpetualLiftStart >, LT_Walk | LT_BothSides, LL_None, constants::perpetualplatformspeeds[ Speed_Slow ], constants::liftdelay[ liftdelay_3sec ] },
	// Platform_DownWaitUp_WR_All
	{ &precon::IsAnyThing, &DoGeneric< Lift >, LT_Walk | LT_BothSides, LL_None, constants::liftspeeds[ Speed_Normal ], constants::liftdelay[ liftdelay_3sec ], pt_lowestneighborfloor },
	// Platform_Stop_WR_Player
	{ &precon::IsPlayer, &DoGeneric< PlatformStop >, LT_Walk | LT_BothSides, LL_None },
	// Door_Raise_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Floor_RaiseLowestCeiling_WR_Player
	{},
	// Floor_Raise24_WR_Player
	{},
	// Floor_Raise24ChangeTexture_WR_Player
	{},
	// Floor_RaiseCrush_WR_Player
	{},
	// Platform_RaiseNearestChangeTexture_WR_Player
	{},
	// Floor_RaiseByTexture_WR_Player
	{},
	// Teleport_Thing_WR_All
	{ &precon::CanTeleportAll, &DoGeneric< Teleport >, LT_Walk | LT_BothSides, LL_None },
	// Floor_LowerHighestFast_WR_Player
	{},
	// Door_OpenFastBlue_SR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitch< Door >, LT_Switch | LT_FrontSide, LL_Blue, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Stairs_BuildBy16Fast_W1_Player
	{},
	// Floor_RaiseLowestCeiling_S1_Player
	{},
	// Floor_LowerHighest_S1_Player
	{},
	// Door_Open_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Light_SetLowest_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< LightSet >, LT_Walk | LT_BothSides, LL_None, 0, 0, lightset_lowestsurround },
	// Door_RaiseFast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Door_OpenFast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_CloseFast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_close },
	// Door_RaiseFast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Door_OpenFast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_CloseFast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_Walk | LT_BothSides, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_close },
	// Door_RaiseFast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Door_OpenFast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_CloseFast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_close },
	// Door_RaiseFast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Door_OpenFast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_CloseFast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_Switch | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Fast ], 0, doordir_close },
	// Door_RaiseFast_UR_All
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_Use | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Door_RaiseFast_U1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_Use | LT_FrontSide, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], doordir_open },
	// Floor_RaiseNearest_W1_Player
	{},
	// Platform_DownWaitUpFast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Lift >, LT_Walk | LT_BothSides, LL_None, constants::liftspeeds[ Speed_Fast ], constants::liftdelay[ liftdelay_3sec ], pt_lowestneighborfloor },
	// Platform_DownWaitUpFast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Lift >, LT_Walk | LT_BothSides, LL_None, constants::liftspeeds[ Speed_Fast ], constants::liftdelay[ liftdelay_3sec ], pt_lowestneighborfloor },
	// Platform_DownWaitUpFast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Lift >, LT_Switch | LT_FrontSide, LL_None, constants::liftspeeds[ Speed_Fast ], constants::liftdelay[ liftdelay_3sec ], pt_lowestneighborfloor },
	// Platform_DownWaitUpFast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Lift >, LT_Switch | LT_FrontSide, LL_None, constants::liftspeeds[ Speed_Fast ], constants::liftdelay[ liftdelay_3sec ], pt_lowestneighborfloor },
	// Exit_Secret_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Exit >, LT_Walk | LT_BothSides, LL_None, 0, 0, exit_secret },
	// Teleport_Thing_W1_Monsters
	{ &precon::CanTeleportMonster, &DoGenericOnce< Teleport >, LT_Walk | LT_BothSides, LL_None },
	// Teleport_Thing_WR_Monsters
	{ &precon::CanTeleportMonster, &DoGeneric< Teleport >, LT_Walk | LT_BothSides, LL_None },
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
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitchOnce< Door >, LT_Switch | LT_FrontSide, LL_Blue, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_OpenFastRed_SR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitch< Door >, LT_Switch | LT_FrontSide, LL_Red, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_OpenFastRed_S1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitchOnce< Door >, LT_Switch | LT_FrontSide, LL_Red, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_OpenFastYellow_SR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitch< Door >, LT_Switch | LT_FrontSide, LL_Yellow, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Door_OpenFastYellow_S1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitchOnce< Door >, LT_Switch | LT_FrontSide, LL_Yellow, constants::doorspeeds[ Speed_Fast ], 0, doordir_open },
	// Light_SetTo255_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< LightSet >, LT_Switch | LT_FrontSide, LL_None, 0, 0, lightset_value, 255 },
	// Light_SetTo35_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< LightSet >, LT_Switch | LT_FrontSide, LL_None, 0, 0, lightset_value, 35 },
	// Floor_Raise512_S1_Player
	{},
	// Crusher_Silent_W1_Player
	{},
	// Floor_Raise512_W1_Player
	{},
	// Platform_Raise24ChangeTexture_W1_Player
	{},
	// Platform_Raise32ChangeTexture_W1_Player
	{},
	// Ceiling_LowerToFloor_W1_Player
	{},
	// Sector_Donut_W1_Player
	{},
	// Floor_Raise512_WR_Player
	{},
	// Platform_Raise24ChangeTexture_WR_Player
	{},
	// Platform_Raise32ChangeTexture_WR_Player
	{},
	// Crusher_Silent_WR_Player
	{},
	// Ceiling_RaiseHighestCeiling_WR_Player
	{},
	// Ceiling_LowerToFloor_WR_Player
	{},
	// Floor_ChangeTexture_W1_Player
	{},
	// Floor_ChangeTexture_WR_Player
	{},
	// Sector_Donut_WR_Player
	{},
	// Light_Strobe_WR_Player
	{},
	// Light_SetLowest_WR_Player
	{ &precon::IsPlayer, &DoGeneric< LightSet >, LT_Walk | LT_BothSides, LL_None, 0, 0, lightset_lowestsurround },
	// Floor_RaiseByTexture_S1_Player
	{},
	// Floor_LowerLowestChangeTexture_NumericModel_S1_Player
	{},
	// Floor_Raise24ChangeTexture_S1_Player
	{},
	// Floor_Raise24_S1_Player
	{},
	// Platform_Perpetual_S1_Player
	{},
	// Platform_Stop_S1_Player
	{},
	// Crusher_Fast_S1_Player
	{},
	// Crusher_Silent_S1_Player
	{},
	// Ceiling_RaiseHighestCeiling_S1_Player
	{},
	// Ceiling_LowerTo8AboveFloor_S1_Player
	{},
	// Crusher_Stop_S1_Player
	{},
	// Light_SetBrightest_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< LightSet >, LT_Switch | LT_FrontSide, LL_None, 0, 0, lightset_highestsurround_firsttagged },
	// Light_SetTo35_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< LightSet >, LT_Switch | LT_FrontSide, LL_None, 0, 0, lightset_value, 35 },
	// Light_SetTo255_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< LightSet >, LT_Switch | LT_FrontSide, LL_None, 0, 0, lightset_value, 255 },
	// Light_Strobe_S1_Player
	{},
	// Light_SetLowest_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< LightSet >, LT_Switch | LT_FrontSide, LL_None, 0, 0, lightset_lowestsurround },
	// Teleport_Thing_S1_All
	{},
	// Door_Close30Open_S1_Player
	{},
	// Floor_RaiseByTexture_SR_Player
	{},
	// Floor_LowerLowestChangeTexture_NumericModel_SR_Player
	{},
	// Floor_Raise512_SR_Player
	{},
	// Floor_Raise24ChangeTexture_SR_Player
	{},
	// Floor_Raise24_SR_Player
	{},
	// Platform_Perpetual_SR_Player
	{},
	// Platform_Stop_SR_Player
	{},
	// Crusher_Fast_SR_Player
	{},
	// Crusher_SR_Player
	{},
	// Crusher_Silent_SR_Player
	{},
	// Ceiling_RaiseHighestCeiling_SR_Player
	{},
	// Ceiling_LowerTo8AboveFloor_SR_Player
	{},
	// Crusher_Stop_SR_Playe
	{},
	// Floor_ChangeTexture_S1_Player
	{},
	// Floor_ChangeTexture_SR_Player
	{},
	// Sector_Donut_SR_Player
	{},
	// Light_SetBrightest_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< LightSet >, LT_Switch | LT_FrontSide, LL_None, 0, 0, lightset_highestsurround_firsttagged },
	// Light_Strobe_SR_Player
	{},
	// Light_SetLowest_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< LightSet >, LT_Switch | LT_FrontSide, LL_None, 0, 0, lightset_lowestsurround },
	// Teleport_Thing_SR_All
	{},
	// Door_Close30Open_SR_Player
	{},
	// Exit_Normal_G1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Exit >, LT_Gun | LT_FrontSide, LL_None, 0, 0, exit_normal },
	// Exit_Secret_G1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Exit >, LT_Gun | LT_FrontSide, LL_None, 0, 0, exit_secret },
	// Ceiling_LowerLowerstCeiling_W1_Player
	{},
	// Ceiling_LowerHighestFloor_W1_Player
	{},
	// Ceiling_LowerLowerstCeiling_WR_Player
	{},
	// Ceiling_LowerHighestFloor_WR_Player
	{},
	// Ceiling_LowerLowerstCeiling_S1_Player
	{},
	// Ceiling_LowerHighestFloor_S1_Player
	{},
	// Ceiling_LowerLowerstCeiling_SR_Player
	{},
	// Ceiling_LowerHighestFloor_SR_Player
	{},
	// Teleport_ThingSilentPreserve_W1_All
	{},
	// Teleport_ThingSilentPreserve_WR_All
	{},
	// Teleport_ThingSilentPreserve_S1_All
	{},
	// Teleport_ThingSilentPreserve_SR_All
	{},
	// Platform_RaiseInstantCeiling_SR_Player
	{},
	// Platform_RaiseInstantCeiling_WR_Player
	{},
	// Transfer_FloorLighting_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_CeilingTexture_Accelerative_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_FloorTexture_Accelerative_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_FloorObjects_Accelerative_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_FloorTextureObjects_Accelerative_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_WallTextureBySector_Accelerative_Always
	{},
	// Floor_LowerNearest_W1_Player
	{},
	// Floor_LowerNearest_WR_Player
	{},
	// Floor_LowerNearest_S1_Player
	{},
	// Floor_LowerNearest_SR_Player
	{},
	// Transfer_Friction_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Transfer_WindByLength_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Transfer_CurrentByLength_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Transfer_WindOrCurrentByPoint_Always
	{},
	// Elevator_Up_W1_Player
	{},
	// Elevator_Up_WR_Player
	{},
	// Elevator_Up_S1_Player
	{},
	// Elevator_Up_SR_Player
	{},
	// Elevator_Down_W1_Player
	{},
	// Elevator_Down_WR_Player
	{},
	// Elevator_Down_S1_Player
	{},
	// Elevator_Down_SR_Player
	{},
	// Elevator_Call_W1_Player
	{},
	// Elevator_Call_WR_Player
	{},
	// Elevator_Call_S1_Player
	{},
	// Elevator_Call_SR_Player
	{},
	// Floor_ChangeTexture_NumericModel_W1_Player
	{},
	// Floor_ChangeTexture_NumericModel_WR_Player
	{},
	// Floor_ChangeTexture_NumericModel_S1_Player
	{},
	// Transfer_Properties_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Teleport_LineSilentPreserve_W1_All
	{},
	// Teleport_LineSilentPreserve_WR_All
	{},
	// Scroll_CeilingTexture_Displace_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_FloorTexture_Displace_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_FloorObjects_Displace_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_FloorTextureObjects_Displace_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_WallTextureBySector_Displace_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_CeilingTexture_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_FloorTexture_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_FloorObjects_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_FloorTextureObjects_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_WallTextureBySector_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Scroll_WallTextureByOffset_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Stairs_BuildBy8_WR_Player
	{},
	// Stairs_BuildBy16Fast_WR_Player
	{},
	// Stairs_BuildBy8_SR_Player
	{},
	// Stairs_BuildBy16Fast_SR_Player
	{},
	// Texture_Translucent_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Transfer_CeilingLighting_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Teleport_LineSilentReversed_W1_All
	{},
	// Teleport_LineSilentReversed_WR_All
	{},
	// Teleport_LineSilentReversed_W1_Monsters
	{},
	// Teleport_LineSilentReversed_WR_Monsters
	{},
	// Teleport_LineSilentPreserve_W1_Monsters
	{},
	// Teleport_LineSilentPreserve_WR_Monsters
	{},
	// Teleport_ThingSilent_W1_Monsters
	{},
	// Teleport_ThingSilent_WR_Monsters
	{},
	// Unknown_270
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Transfer_Sky_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Transfer_SkyReversed_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
};

// Another ugly table
constexpr const char* doorlockreason[] =
{
	nullptr, PD_BLUEK, PD_YELLOWK, nullptr, PD_REDK, nullptr, nullptr, nullptr, nullptr, // Any colour
	nullptr, PD_BOOM_BLUECARD_DOOR, PD_BOOM_YELLOWCARD_DOOR, nullptr, PD_BOOM_REDCARD_DOOR, nullptr, nullptr, nullptr, nullptr, // Cards
	nullptr, PD_BOOM_BLUESKULL_DOOR, PD_BOOM_YELLOWSKULL_DOOR, nullptr, PD_BOOM_REDSKULL_DOOR, nullptr, nullptr, nullptr, nullptr, // Skulls
};

constexpr const char* switchlockreason[] =
{
	nullptr, PD_BLUEO, PD_YELLOWO, nullptr, PD_REDO, nullptr, nullptr, nullptr, // Any colour
	nullptr, PD_BOOM_BLUECARD_SWITCH, PD_BOOM_YELLOWCARD_SWITCH, nullptr, PD_BOOM_REDCARD_SWITCH, nullptr, nullptr, nullptr, // Cards
	nullptr, PD_BOOM_BLUESKULL_SWITCH, PD_BOOM_YELLOWSKULL_SWITCH, nullptr, PD_BOOM_REDSKULL_SWITCH, nullptr, nullptr, nullptr, // Skulls
};

void lineaction_s::DisplayLockReason( player_t* player )
{
	if( player == &players[ consoleplayer ] )
	{
		bool door = ( trigger & LT_AnimatedActivationTypeMask ) == LT_Use;
		int32_t reason = lock & LL_TypeMask;
		player->message = DEH_String( door ? doorlockreason[ reason ] : switchlockreason[ reason ] );
		S_StartSound( nullptr, sfx_oof );
	}
}

lineaction_t* GetBuiltInAction( int32_t special )
{
	return (lineaction_t*)&builtinlineactions[ special ];
}

lineaction_t* CreateBoomGeneralisedLiftAction( line_t* line )
{
	lineaction_t* action = (lineaction_t*)Z_MallocAs( lineaction_t, PU_LEVEL, nullptr );

	uint32_t trigger = line->special & Generic_Trigger_Mask;
	bool isswitch = trigger == Generic_Trigger_S1 || trigger == Generic_Trigger_SR
					|| trigger == Generic_Trigger_G1 || trigger == Generic_Trigger_GR;
	bool repeatable = ( line->special & Generic_Trigger_Repeatable ) == Generic_Trigger_Repeatable;
	uint32_t speed = ( line->special & Generic_Speed_Mask ) >> Generic_Speed_Shift;

	bool allowmonster = ( line->special & Lift_AllowMonsters_Yes ) == Lift_AllowMonsters_Yes;
	uint32_t delay = ( line->special & Lift_Delay_Mask ) >> Lift_Delay_Shift;
	uint32_t target = ( line->special & Lift_Target_Mask ) >> Lift_Target_Shift;

	action->precondition = allowmonster ? &precon::IsAnyThing : &precon::IsPlayer;
	if( isswitch )
	{
		action->action = repeatable ? &DoGenericSwitch< Lift > : &DoGenericSwitchOnce< Lift >;
	}
	else
	{
		action->action = repeatable ? &DoGeneric< Lift > : &DoGenericOnce< Lift >;
	}

	action->trigger = constants::triggers[ trigger ];
	action->lock = LL_None;
	action->speed = constants::liftspeeds[ speed ];
	action->delay = constants::liftdelay[ delay ];
	action->param1 = target;

	return action;
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
	uint32_t delay = ( line->special & Door_Delay_Mask ) >> Door_Delay_Shift;

	if( manualuse )
	{
		action->precondition = allowmonster ? &precon::CanDoorRaise : &precon::IsPlayer;
	}
	{
		action->precondition = allowmonster ? &precon::IsAnyThing : &precon::IsPlayer;
	}

	if( isswitch )
	{
		action->action = repeatable ? &DoGenericSwitch< Door > : &DoGenericSwitchOnce< Door >;
	}
	else
	{
		action->action = repeatable ? &DoGeneric< Door > : &DoGenericOnce< Door >;
	}
	action->trigger = constants::triggers[ trigger ];
	action->lock = LL_None;
	action->speed = constants::doorspeeds[ speed ];
	action->delay = isdelayable ? constants::doordelay[ delay ] : 0;
	action->param1 = isup ? doordir_open
						: isdown ? doordir_close
							: doordir_none;

	return action;
}

lineaction_t* CreateBoomGeneralisedLockedDoorAction( line_t* line )
{
	lineaction_t* action = (lineaction_t*)Z_MallocAs( lineaction_t, PU_LEVEL, nullptr );

	uint32_t trigger = line->special & Generic_Trigger_Mask;
	bool isswitch = trigger == Generic_Trigger_S1 || trigger == Generic_Trigger_SR
					|| trigger == Generic_Trigger_G1 || trigger == Generic_Trigger_GR;
	bool repeatable = ( line->special & Generic_Trigger_Repeatable ) == Generic_Trigger_Repeatable;
	uint32_t speed = ( line->special & Generic_Speed_Mask ) >> Generic_Speed_Shift;

	uint32_t kind = ( line->special & LockedDoor_Type_Mask );
	bool isdelayable = kind == LockedDoor_Type_OpenWaitClose;

	uint32_t lockkind = ( line->special & LockedDoor_Lock_Mask ) >> LockedDoor_Lock_Shift;

	doombool typeagnostic = ( line->special & LockedDoor_TypeAgnostic_Yes ) == LockedDoor_TypeAgnostic_Yes;

	action->precondition = typeagnostic? precon::LockLookup_Equivalent[ lockkind ] : precon::LockLookup_Separate[ lockkind ];
	if( isswitch )
	{
		action->action = repeatable ? &DoGenericSwitch< Door > : &DoGenericSwitchOnce< Door >;
	}
	else
	{
		action->action = repeatable ? &DoGeneric< Door > : &DoGenericOnce< Door >;
	}
	action->trigger = constants::triggers[ trigger ];
	action->lock = typeagnostic ? constants::locks_equivalent[ lockkind ] : constants::locks_separate[ lockkind ];
	action->speed = constants::doorspeeds[ speed ];
	action->delay = isdelayable ? constants::doordelay[ Door_Delay_4Seconds >> Door_Delay_Shift ] : 0;
	action->param1 = doordir_open;

	return action;
}

lineaction_t* CreateBoomGeneralisedLineAction( line_t* line )
{
	if( line->special >= Generic_Lift && line->special < Generic_LockedDoor )
	{
		return CreateBoomGeneralisedLiftAction( line );
	}
	else if( line->special >= Generic_LockedDoor && line->special < Generic_Door )
	{
		return CreateBoomGeneralisedLockedDoorAction( line );
	}
	else if( line->special >= Generic_Door && line->special < Generic_Ceiling )
	{
		return CreateBoomGeneralisedDoorAction( line );
	}
	return nullptr;
}

DOOM_C_API lineaction_t* P_GetLineActionFor( line_t* line )
{
	if( !remove_limits )
	{
		return nullptr;
	}

	if( line->special >= DoomActions_Min && line->special < DoomActions_Max )
	{
		return ( !remove_limits && ( line->special == Unknown_078 || line->special == Unknown_085 ) )
			? GetBuiltInAction( 0 )
			: GetBuiltInAction( line->special );
	}
	else if( remove_limits )
	{
		if( /*allow_boom_specials && */
			line->special >= BoomActions_Min && line->special < BoomActions_Max )
		{
			return GetBuiltInAction( line->special );
		}
		else if( /*( allow_mbf_specials || allow_mbf_sky_specials ) && */
				( line->special == Transfer_Sky_Always
					|| line->special == Transfer_SkyReversed_Always )
				)
		{
			return GetBuiltInAction( line->special );
		}
		else if( /*allow_boom_specials && */
				line->special >= Generic_Min && line->special < Generic_Max )
		{
			return CreateBoomGeneralisedLineAction( line );
		}
	}

	return nullptr;
}

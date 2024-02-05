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
#include "p_local.h"

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

	static constexpr int32_t vanillaraisespeed = DoubleToFixed( 0.5 );
	static constexpr int32_t slowstairsspeed = DoubleToFixed( 0.25 );

	static constexpr int32_t floorspeeds[] =
	{
		IntToFixed( 1 ),
		IntToFixed( 2 ),
		IntToFixed( 4 ),
		IntToFixed( 8 ),
	};

	static constexpr int32_t stairspeeds[] =
	{
		DoubleToFixed( 0.25 ),
		DoubleToFixed( 0.5 ),
		IntToFixed( 2 ),
		IntToFixed( 4 ),
	};

	static constexpr int32_t ceilingspeeds[] =
	{
		IntToFixed( 1 ),
		IntToFixed( 2 ),
		IntToFixed( 4 ),
		IntToFixed( 8 ),
	};

	static constexpr int32_t crushingspeeds[] =
	{
		DoubleToFixed( 0.125 ),
		DoubleToFixed( 0.25 ),
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
		LT_WalkBoth,
		LT_WalkBoth,
		LT_SwitchFront,
		LT_SwitchFront,
		LT_GunFront,
		LT_GunFront,
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
	};
}

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
MakeGenericFunc( VanillaRaise, EV_DoVanillaPlatformRaiseGeneric );
MakeGenericFunc( Floor, EV_DoFloorGeneric );
MakeGenericFunc( Donut, EV_DoDonutGeneric );
MakeGenericFunc( Ceiling, EV_DoCeilingGeneric );
MakeGenericFunc( Crusher, EV_DoCrusherGeneric );
MakeGenericFunc( CeilingStop, EV_StopAnyCeilingGeneric );
MakeGenericFunc( BoomFloorAndCeiling, EV_DoBoomFloorCeilingGeneric );
MakeGenericFunc( Elevator, EV_DoElevatorGeneric );
MakeGenericFunc( Stairs, EV_DoStairsGeneric );
MakeGenericFunc( Teleport, EV_DoTeleportGeneric );
MakeGenericFunc( Exit, EV_DoExitGeneric );
MakeGenericFunc( LightSet, EV_DoLightSetGeneric );
MakeGenericFunc( LightStrobe, EV_DoLightStrobeGeneric );

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

	if( !remove_limits ) // fix_w1s1_lines_clearing_on_no_result
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

	if( !remove_limits ) // fix_w1s1_lines_clearing_on_no_result
	{
		line->action = nullptr;
		line->special = 0;
	}
}

static void DoVanillaW1Teleport( line_t* line, mobj_t* activator )
{
	int32_t side = P_PointOnLineSide( activator->x, activator->y, line );
	if( side == 1 ) // Was previously on front side
	{
		DoGenericOnce< Teleport >( line, activator );
	}

	if( !remove_limits ) // fix_w1s1_lines_clearing_on_no_result
	{
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
	{ &precon::CanDoorRaise, &DoGeneric< Door >, LT_UseFront, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], sd_open, door_raiselower },
	// Door_Open_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Slow ], 0, sd_open, door_noraise },
	// Door_Close_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Slow ], 0, sd_close, door_noraise },
	// Door_Raise_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], sd_open, door_noraise },
	// Floor_RaiseLowestCeiling_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Crusher_Fast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Crusher >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Normal ], 0, constants::ceilingspeeds[ Speed_Normal ], cs_movement },
	// Stairs_BuildBy8_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Stairs >, LT_SwitchFront, LL_None, constants::slowstairsspeed, 0, IntToFixed( 8 ), 0, 0 },
	// Stairs_BuildBy8_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Stairs >, LT_WalkBoth, LL_None, constants::slowstairsspeed, 0, IntToFixed( 8 ), 0, 0 },
	// Sector_Donut_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Donut >, LT_SwitchFront, LL_None, constants::vanillaraisespeed, 0 },
	// Platform_DownWaitUp_W1_All
	{ &precon::IsAnyThing, &DoGenericOnce< Lift >, LT_WalkBoth, LL_None, constants::liftspeeds[ Speed_Normal ], constants::liftdelay[ liftdelay_3sec ], stt_lowestneighborfloor },
	// Exit_Normal_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Exit >, LT_SwitchFront, LL_None, 0, 0, exit_normal },
	// Light_SetBrightest_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< LightSet >, LT_WalkBoth, LL_None, 0, 0, lightset_highestsurround_firsttagged },
	// Light_SetTo255_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< LightSet >, LT_WalkBoth, LL_None, 0, 0, lightset_value, 255 },
	// Platform_Raise32ChangeTexture_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< VanillaRaise >, LT_SwitchFront, LL_None, constants::vanillaraisespeed, 0, stt_nosearch, IntToFixed( 32 ) },
	// Platform_Raise24ChangeTexture_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< VanillaRaise >, LT_SwitchFront, LL_None, constants::vanillaraisespeed, 0, stt_nosearch, IntToFixed( 24 ) },
	// Door_Close30Open_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_30sec ], sd_close, door_noraise },
	// Light_Strobe_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< LightStrobe >, LT_WalkBoth, LL_None, 0, 0 },
	// Floor_RaiseNearest_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nexthighestneighborfloor, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_LowerHighest_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_highestneighborfloor_noaddifmatch, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Platform_RaiseNearestChangeTexture_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< VanillaRaise >, LT_SwitchFront, LL_None, constants::vanillaraisespeed, 0, stt_nexthighestneighborfloor, 0 },
	// Platform_DownWaitUp_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Lift >, LT_SwitchFront, LL_None, constants::liftspeeds[ Speed_Normal ], constants::liftdelay[ liftdelay_3sec ], stt_lowestneighborfloor },
	// Platform_RaiseNearestChangeTexture_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< VanillaRaise >, LT_WalkBoth, LL_None, constants::vanillaraisespeed, 0, stt_nexthighestneighborfloor, 0 },
	// Floor_LowerLowest_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_RaiseLowestCeiling_GR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_GunFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Crusher_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Crusher >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, constants::crushingspeeds[ Speed_Slow ], cs_movement },
	// Door_RaiseBlue_UR_Player
	{ &precon::HasAnyKeyOfColour, &DoGeneric< Door >, LT_UseFront, LL_Blue, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], sd_open, door_raiselower },
	// Door_RaiseYellow_UR_Player
	{ &precon::HasAnyKeyOfColour, &DoGeneric< Door >, LT_UseFront, LL_Yellow, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], sd_open, door_raiselower },
	// Door_RaiseRed_UR_Player
	{ &precon::HasAnyKeyOfColour, &DoGeneric< Door >, LT_UseFront, LL_Red, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], sd_open, door_raiselower },
	// Door_Raise_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], sd_open, door_noraise },
	// Floor_RaiseByTexture_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_shortestlowertexture, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Door_Open_U1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_UseFront, LL_None, constants::doorspeeds[ Speed_Slow ], 0, sd_open, door_noraise },
	// Door_OpenBlue_U1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericOnce< Door >, LT_UseFront, LL_Blue, constants::doorspeeds[ Speed_Slow ], 0, sd_open, door_noraise },
	// Door_OpenRed_U1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericOnce< Door >, LT_UseFront, LL_Red, constants::doorspeeds[ Speed_Slow ], 0, sd_open, door_noraise },
	// Door_OpenYellow_U1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericOnce< Door >, LT_UseFront, LL_Yellow, constants::doorspeeds[ Speed_Slow ], 0, sd_open, door_noraise },
	// Light_SetTo35_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< LightSet >, LT_WalkBoth, LL_None, 0, 0, lightset_value, 35 },
	// Floor_LowerHighestFast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Fast ], 0, stt_highestneighborfloor_noaddifmatch, sd_down, IntToFixed( 8 ), sct_none, scm_trigger, sc_nocrush },
	// Floor_LowerLowestChangeTexture_NumericModel_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_shortestlowertexture, sd_down, 0, sct_copyboth, scm_numeric, sc_nocrush },
	// Floor_LowerLowest_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Teleport_Thing_W1_All
	{ &precon::CanTeleportAll, &DoVanillaW1Teleport, LT_WalkBoth, LL_None, 0, 0, tt_tothing },
	// Ceiling_RaiseHighestCeiling_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Ceiling >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_highestneighborceiling, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerToFloor_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Ceiling >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_floor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Door_Close_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Slow ], 0, sd_close, door_noraise },
	// Ceiling_LowerToFloor_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Ceiling >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_floor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerTo8AboveFloor_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Ceiling >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_floor, sd_down, IntToFixed( 8 ), sct_none, scm_trigger, sc_nocrush },
	// Floor_LowerHighest_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_highestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Door_Open_GR_All
	{ &precon::IsAnyThing, &DoGenericSwitch< Door >, LT_GunFront, LL_None, constants::doorspeeds[ Speed_Slow ], 0, sd_open, door_noraise },
	// Platform_RaiseNearestChangeTexture_G1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< VanillaRaise >, LT_GunFront, LL_None, constants::vanillaraisespeed, 0, stt_nexthighestneighborfloor, 0 },
	// Scroll_WallTextureLeft_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Crusher_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Crusher >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, constants::crushingspeeds[ Speed_Slow ], cs_movement },
	// Door_Close_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Slow ], 0, sd_close, door_noraise },
	// Exit_Secret_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Exit >, LT_SwitchFront, LL_None, 0, 0, exit_secret },
	// Exit_Normal_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Exit >, LT_WalkBoth, LL_None, 0, 0, exit_normal },
	// Platform_Perpetual_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< PerpetualLiftStart >, LT_WalkBoth, LL_None, constants::perpetualplatformspeeds[ Speed_Slow ], constants::liftdelay[ liftdelay_3sec ] },
	// Platform_Stop_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< PlatformStop >, LT_WalkBoth, LL_None },
	// Floor_RaiseCrush_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_up, IntToFixed( -8 ), sct_none, scm_trigger, sc_crush },
	// Floor_RaiseCrush_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_up, IntToFixed( -8 ), sct_none, scm_trigger, sc_crush },
	// Crusher_Stop_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< CeilingStop >, LT_WalkBoth, LL_None, 0, 0 },
	// Floor_Raise24_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_up, IntToFixed( 24 ), sct_none, scm_trigger, sc_nocrush },
	// Floor_Raise24ChangeTexture_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_up, IntToFixed( 24 ), sct_copyboth, scm_trigger, sc_nocrush },
	// Floor_LowerLowest_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Door_Open_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Slow ], 0, sd_open, door_noraise },
	// Platform_DownWaitUp_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Lift >, LT_SwitchFront, LL_None, constants::liftspeeds[ Speed_Normal ], constants::liftdelay[ liftdelay_3sec ], stt_lowestneighborfloor },
	// Door_Raise_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], sd_open, door_noraise },
	// Floor_RaiseLowestCeiling_SR_player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_RaiseCrush_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_up, IntToFixed( -8 ), sct_none, scm_trigger, sc_crush },
	// Platform_Raise24ChangeTexture_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< VanillaRaise >, LT_SwitchFront, LL_None, constants::vanillaraisespeed, 0, stt_nosearch, IntToFixed( 24 ) },
	// Platform_Raise32ChangeTexture_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< VanillaRaise >, LT_SwitchFront, LL_None, constants::vanillaraisespeed, 0, stt_nosearch, IntToFixed( 32 ) },
	// Platform_RaiseNearestChangeTexture_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< VanillaRaise >, LT_SwitchFront, LL_None, constants::vanillaraisespeed, 0, stt_nexthighestneighborfloor, 0 },
	// Floor_RaiseNearest_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nexthighestneighborfloor, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_LowerHighestFast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Fast ], 0, stt_highestneighborfloor_noaddifmatch, sd_down, IntToFixed( 8 ), sct_none, scm_trigger, sc_nocrush },
	// Floor_LowerHighestFast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Fast ], 0, stt_highestneighborfloor_noaddifmatch, sd_down, IntToFixed( 8 ), sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerTo8AboveFloor_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Ceiling >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_floor, sd_down, IntToFixed( 8 ), sct_none, scm_trigger, sc_nocrush },
	// Crusher_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Crusher >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, constants::crushingspeeds[ Speed_Slow ], cs_movement },
	// Crusher_Stop_WR_Player
	{ &precon::IsPlayer, &DoGeneric< CeilingStop >, LT_WalkBoth, LL_None, 0, 0 },
	// Door_Close_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Slow ], 0, sd_close, door_noraise },
	// Door_Close30Open_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_30sec ], sd_close, door_noraise },
	// Crusher_Fast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Crusher >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Normal ], 0, constants::ceilingspeeds[ Speed_Normal ], cs_movement },
	// Doom: Unknown_078
	// Boom: Floor_ChangeTexture_NumericModel_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_down, 0, sct_copyboth, scm_numeric, sc_nocrush },
	// Light_SetTo35_WR_Player
	{ &precon::IsPlayer, &DoGeneric< LightSet >, LT_WalkBoth, LL_None, 0, 0, lightset_value, 35 },
	// Light_SetBrightest_WR_Player
	{ &precon::IsPlayer, &DoGeneric< LightSet >, LT_WalkBoth, LL_None, 0, 0, lightset_highestsurround_firsttagged },
	// Light_SetTo255_WR_Player
	{ &precon::IsPlayer, &DoGeneric< LightSet >, LT_WalkBoth, LL_None, 0, 0, lightset_value, 255 },
	// Floor_LowerLowest_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_LowerHighest_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_highestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_LowerLowestChangeTexture_NumericModel_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborfloor, sd_down, 0, sct_copyboth, scm_numeric, sc_nocrush },
	// Doom: Unknown_085
	// Boom: Scroll_WallTextureRight_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Door_Open_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Slow ], 0, sd_open, door_noraise },
	// Platform_Perpetual_WR_Player
	{ &precon::IsPlayer, &DoGeneric< PerpetualLiftStart >, LT_WalkBoth, LL_None, constants::perpetualplatformspeeds[ Speed_Slow ], constants::liftdelay[ liftdelay_3sec ] },
	// Platform_DownWaitUp_WR_All
	{ &precon::IsAnyThing, &DoGeneric< Lift >, LT_WalkBoth, LL_None, constants::liftspeeds[ Speed_Normal ], constants::liftdelay[ liftdelay_3sec ], stt_lowestneighborfloor },
	// Platform_Stop_WR_Player
	{ &precon::IsPlayer, &DoGeneric< PlatformStop >, LT_WalkBoth, LL_None },
	// Door_Raise_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_4sec ], sd_open, door_noraise },
	// Floor_RaiseLowestCeiling_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_Raise24_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_up, IntToFixed( 24 ), sct_none, scm_trigger, sc_nocrush },
	// Floor_Raise24ChangeTexture_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_up, IntToFixed( 24 ), sct_copyboth, scm_trigger, sc_nocrush },
	// Floor_RaiseCrush_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_up, IntToFixed( -8 ), sct_none, scm_trigger, sc_crush },
	// Platform_RaiseNearestChangeTexture_WR_Player
	{ &precon::IsPlayer, &DoGeneric< VanillaRaise >, LT_WalkBoth, LL_None, constants::vanillaraisespeed, 0, stt_nexthighestneighborfloor, 0 },
	// Floor_RaiseByTexture_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_shortestlowertexture, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Teleport_Thing_WR_All
	{ &precon::CanTeleportAll, &DoGeneric< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_tothing },
	// Floor_LowerHighestPlus8Fast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Fast ], 0, stt_highestneighborfloor, sd_down, IntToFixed( 8 ), sct_none, scm_trigger, sc_nocrush },
	// Door_OpenFastBlue_SR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitch< Door >, LT_SwitchFront, LL_Blue, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Stairs_BuildBy16Fast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Stairs >, LT_WalkBoth, LL_None, constants::stairspeeds[ Speed_Turbo ], 0, IntToFixed( 16 ), 0, 1 },
	// Floor_RaiseLowestCeiling_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_LowerHighest_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_highestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Door_Open_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Slow ], 0, sd_open, door_noraise },
	// Light_SetLowest_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< LightSet >, LT_WalkBoth, LL_None, 0, 0, lightset_lowestsurround },
	// Door_RaiseFast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], sd_open, door_noraise },
	// Door_OpenFast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Door_CloseFast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Fast ], 0, sd_close, door_noraise },
	// Door_RaiseFast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], sd_open, door_noraise },
	// Door_OpenFast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Door_CloseFast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_WalkBoth, LL_None, constants::doorspeeds[ Speed_Fast ], 0, sd_close, door_noraise },
	// Door_RaiseFast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], sd_open, door_noraise },
	// Door_OpenFast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Door_CloseFast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Fast ], 0, sd_close, door_noraise },
	// Door_RaiseFast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], sd_open, door_noraise },
	// Door_OpenFast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Door_CloseFast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Fast ], 0, sd_close, door_noraise },
	// Door_RaiseFast_UR_All
	{ &precon::CanDoorRaise, &DoGeneric< Door >, LT_UseFront, LL_None, constants::doorspeeds[ Speed_Fast ], constants::doordelay[ doordelay_4sec ], sd_open, door_raiselower },
	// Door_OpenFast_U1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Door >, LT_UseFront, LL_None, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Floor_RaiseNearest_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nexthighestneighborfloor, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Platform_DownWaitUpFast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Lift >, LT_WalkBoth, LL_None, constants::liftspeeds[ Speed_Fast ], constants::liftdelay[ liftdelay_3sec ], stt_lowestneighborfloor },
	// Platform_DownWaitUpFast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Lift >, LT_WalkBoth, LL_None, constants::liftspeeds[ Speed_Fast ], constants::liftdelay[ liftdelay_3sec ], stt_lowestneighborfloor },
	// Platform_DownWaitUpFast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Lift >, LT_SwitchFront, LL_None, constants::liftspeeds[ Speed_Fast ], constants::liftdelay[ liftdelay_3sec ], stt_lowestneighborfloor },
	// Platform_DownWaitUpFast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Lift >, LT_SwitchFront, LL_None, constants::liftspeeds[ Speed_Fast ], constants::liftdelay[ liftdelay_3sec ], stt_lowestneighborfloor },
	// Exit_Secret_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Exit >, LT_WalkBoth, LL_None, 0, 0, exit_secret },
	// Teleport_Thing_W1_Monsters
	{ &precon::CanTeleportMonster, &DoVanillaW1Teleport, LT_WalkBoth, LL_None, 0, 0, tt_tothing },
	// Teleport_Thing_WR_Monsters
	{ &precon::CanTeleportMonster, &DoGeneric< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_tothing },
	// Stairs_BuildBy16Fast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Stairs >, LT_SwitchFront, LL_None, constants::stairspeeds[ Speed_Turbo ], 0, IntToFixed( 16 ), 0, 1 },
	// Floor_RaiseNearest_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nexthighestneighborfloor, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_RaiseNearestFast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Fast ], 0, stt_nexthighestneighborfloor, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_RaiseNearestFast_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Fast ], 0, stt_nexthighestneighborfloor, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_RaiseNearestFast_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Fast ], 0, stt_nexthighestneighborfloor, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_RaiseNearestFast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Fast ], 0, stt_nexthighestneighborfloor, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Door_OpenFastBlue_S1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitchOnce< Door >, LT_SwitchFront, LL_Blue, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Door_OpenFastRed_SR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitch< Door >, LT_SwitchFront, LL_Red, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Door_OpenFastRed_S1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitchOnce< Door >, LT_SwitchFront, LL_Red, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Door_OpenFastYellow_SR_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitch< Door >, LT_SwitchFront, LL_Yellow, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Door_OpenFastYellow_S1_Player
	{ &precon::HasAnyKeyOfColour, &DoGenericSwitchOnce< Door >, LT_SwitchFront, LL_Yellow, constants::doorspeeds[ Speed_Fast ], 0, sd_open, door_noraise },
	// Light_SetTo255_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< LightSet >, LT_SwitchFront, LL_None, 0, 0, lightset_value, 255 },
	// Light_SetTo35_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< LightSet >, LT_SwitchFront, LL_None, 0, 0, lightset_value, 35 },
	// Floor_Raise512_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_up, IntToFixed( 512 ), sct_none, scm_trigger, sc_nocrush },

	/* Boom specials */

	// Crusher_Silent_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Crusher >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, constants::crushingspeeds[ Speed_Slow ], cs_endstop },
	// Floor_Raise512_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_up, IntToFixed( 512 ), sct_none, scm_trigger, sc_nocrush },
	// Platform_Raise24ChangeTexture_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< VanillaRaise >, LT_WalkFront, LL_None, constants::vanillaraisespeed, 0, stt_nosearch, IntToFixed( 24 ) },
	// Platform_Raise32ChangeTexture_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< VanillaRaise >, LT_WalkFront, LL_None, constants::vanillaraisespeed, 0, stt_nosearch, IntToFixed( 32 ) },
	// Ceiling_LowerToFloor_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Ceiling >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_floor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Sector_Donut_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Donut >, LT_WalkBoth, LL_None, constants::vanillaraisespeed, 0 },
	// Floor_Raise512_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_up, IntToFixed( 512 ), sct_none, scm_trigger, sc_nocrush },
	// Platform_Raise24ChangeTexture_WR_Player
	{ &precon::IsPlayer, &DoGeneric< VanillaRaise >, LT_WalkFront, LL_None, constants::vanillaraisespeed, 0, stt_nosearch, IntToFixed( 24 ) },
	// Platform_Raise32ChangeTexture_WR_Player
	{ &precon::IsPlayer, &DoGeneric< VanillaRaise >, LT_WalkFront, LL_None, constants::vanillaraisespeed, 0, stt_nosearch, IntToFixed( 32 ) },
	// Crusher_Silent_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Crusher >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, constants::crushingspeeds[ Speed_Slow ], cs_endstop },
	// Ceiling_RaiseHighestCeiling_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Ceiling >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_highestneighborceiling, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerToFloor_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Ceiling >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_floor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Floor_ChangeTexture_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_down, 0, sct_copyboth, scm_trigger, sc_nocrush },
	// Floor_ChangeTexture_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_down, 0, sct_copyboth, scm_trigger, sc_nocrush },
	// Sector_Donut_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Donut >, LT_WalkBoth, LL_None, constants::vanillaraisespeed, 0 },
	// Light_Strobe_WR_Player
	{ &precon::IsPlayer, &DoGeneric< LightStrobe >, LT_WalkBoth, LL_None, 0, 0 },
	// Light_SetLowest_WR_Player
	{ &precon::IsPlayer, &DoGeneric< LightSet >, LT_WalkBoth, LL_None, 0, 0, lightset_lowestsurround },
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
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Crusher >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Normal ], 0, constants::ceilingspeeds[ Speed_Normal ], cs_movement },
	// Crusher_Silent_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Crusher >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, constants::crushingspeeds[ Speed_Slow ], cs_endstop },
	// FloorCeiling_RaiseHighestOrLowerLowest_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< BoomFloorAndCeiling >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_highestneighborceiling, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerTo8AboveFloor_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Ceiling >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_floor, sd_down, IntToFixed( 8 ), sct_none, scm_trigger, sc_nocrush },
	// Crusher_Stop_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< CeilingStop >, LT_SwitchFront, LL_None, 0, 0 },
	// Light_SetBrightest_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< LightSet >, LT_SwitchFront, LL_None, 0, 0, lightset_highestsurround_firsttagged },
	// Light_SetTo35_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< LightSet >, LT_SwitchFront, LL_None, 0, 0, lightset_value, 35 },
	// Light_SetTo255_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< LightSet >, LT_SwitchFront, LL_None, 0, 0, lightset_value, 255 },
	// Light_Strobe_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< LightStrobe >, LT_SwitchFront, LL_None, 0, 0 },
	// Light_SetLowest_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< LightSet >, LT_SwitchFront, LL_None, 0, 0, lightset_lowestsurround },
	// Teleport_Thing_S1_All
	{ &precon::CanTeleportAll, &DoGenericSwitchOnce< Teleport >, LT_SwitchFront, LL_None, 0, 0, tt_tothing },
	// Door_Close30Open_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_30sec ], sd_close, door_noraise },
	// Floor_RaiseByTexture_SR_Player
	{},
	// Floor_LowerLowestChangeTexture_NumericModel_SR_Player
	{},
	// Floor_Raise512_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_up, IntToFixed( 512 ), sct_none, scm_trigger, sc_nocrush },
	// Floor_Raise24ChangeTexture_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_up, IntToFixed( 24 ), sct_copyboth, scm_trigger, sc_nocrush },
	// Floor_Raise24_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_up, IntToFixed( 24 ), sct_none, scm_trigger, sc_nocrush },
	// Platform_Perpetual_SR_Player
	{},
	// Platform_Stop_SR_Player
	{},
	// Crusher_Fast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Crusher >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Normal ], 0, constants::ceilingspeeds[ Speed_Normal ], cs_movement },
	// Crusher_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Crusher >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, constants::crushingspeeds[ Speed_Slow ], cs_movement },
	// Crusher_Silent_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Crusher >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, constants::crushingspeeds[ Speed_Slow ], cs_endstop },
	// FloorCeiling_RaiseHighestOrLowerLowest_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitch< BoomFloorAndCeiling >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_highestneighborceiling, sd_up, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerTo8AboveFloor_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Ceiling >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_floor, sd_down, IntToFixed( 8 ), sct_none, scm_trigger, sc_nocrush },
	// Crusher_Stop_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< CeilingStop >, LT_SwitchFront, LL_None, 0, 0 },
	// Floor_ChangeTexture_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_down, 0, sct_copyboth, scm_trigger, sc_nocrush },
	// Floor_ChangeTexture_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_down, 0, sct_copyboth, scm_trigger, sc_nocrush },
	// Sector_Donut_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Donut >, LT_SwitchFront, LL_None, constants::vanillaraisespeed, 0 },
	// Light_SetBrightest_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< LightSet >, LT_SwitchFront, LL_None, 0, 0, lightset_highestsurround_firsttagged },
	// Light_Strobe_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< LightStrobe >, LT_SwitchFront, LL_None, 0, 0 },
	// Light_SetLowest_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< LightSet >, LT_SwitchFront, LL_None, 0, 0, lightset_lowestsurround },
	// Teleport_Thing_SR_All
	{ &precon::CanTeleportAll, &DoGenericSwitch< Teleport >, LT_SwitchFront, LL_None, 0, 0, tt_tothing },
	// Door_Close30Open_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Door >, LT_SwitchFront, LL_None, constants::doorspeeds[ Speed_Slow ], constants::doordelay[ doordelay_30sec ], sd_close, door_noraise },
	// Exit_Normal_G1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Exit >, LT_GunFront, LL_None, 0, 0, exit_normal },
	// Exit_Secret_G1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Exit >, LT_GunFront, LL_None, 0, 0, exit_secret },
	// Ceiling_LowerLowerstCeiling_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Ceiling >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerHighestFloor_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Ceiling >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_highestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerLowerstCeiling_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Ceiling >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerHighestFloor_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Ceiling >, LT_WalkBoth, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_highestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerLowerstCeiling_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Ceiling >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerHighestFloor_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Ceiling >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_highestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerLowerstCeiling_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Ceiling >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_lowestneighborceiling, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Ceiling_LowerHighestFloor_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Ceiling >, LT_SwitchFront, LL_None, constants::ceilingspeeds[ Speed_Slow ], 0, stt_highestneighborfloor, sd_down, 0, sct_none, scm_trigger, sc_nocrush },
	// Teleport_ThingSilentPreserve_W1_All
	{ &precon::CanTeleportAll, &DoGenericOnce< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_tothing | tt_preserveangle | tt_silent },
	// Teleport_ThingSilentPreserve_WR_All
	{ &precon::CanTeleportAll, &DoGeneric< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_tothing | tt_preserveangle | tt_silent },
	// Teleport_ThingSilentPreserve_S1_All
	{ &precon::CanTeleportAll, &DoGenericSwitchOnce< Teleport >, LT_SwitchFront, LL_None, 0, 0, tt_tothing | tt_preserveangle | tt_silent },
	// Teleport_ThingSilentPreserve_SR_All
	{ &precon::CanTeleportAll, &DoGenericSwitch< Teleport >, LT_SwitchFront, LL_None, 0, 0, tt_tothing | tt_preserveangle | tt_silent },
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
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Elevator_Up_W1_Player
	{ &precon::IsAnyThing, &DoGenericOnce< Elevator >, LT_WalkBoth, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_nexthighestneighborfloor, sd_up },
	// Elevator_Up_WR_Player
	{ &precon::IsAnyThing, &DoGeneric< Elevator >, LT_WalkBoth, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_nexthighestneighborfloor, sd_up },
	// Elevator_Up_S1_Player
	{ &precon::IsAnyThing, &DoGenericSwitchOnce< Elevator >, LT_SwitchFront, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_nexthighestneighborfloor, sd_up },
	// Elevator_Up_SR_Player
	{ &precon::IsAnyThing, &DoGenericSwitch< Elevator >, LT_SwitchFront, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_nexthighestneighborfloor, sd_up },
	// Elevator_Down_W1_Player
	{ &precon::IsAnyThing, &DoGenericOnce< Elevator >, LT_WalkBoth, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_nextlowestneighborfloor, sd_down },
	// Elevator_Down_WR_Player
	{ &precon::IsAnyThing, &DoGeneric< Elevator >, LT_WalkBoth, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_nextlowestneighborfloor, sd_down },
	// Elevator_Down_S1_Player
	{ &precon::IsAnyThing, &DoGenericSwitchOnce< Elevator >, LT_SwitchFront, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_nextlowestneighborfloor, sd_down },
	// Elevator_Down_SR_Player
	{ &precon::IsAnyThing, &DoGenericSwitch< Elevator >, LT_SwitchFront, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_nextlowestneighborfloor, sd_down },
	// Elevator_Call_W1_Player
	{ &precon::IsAnyThing, &DoGenericOnce< Elevator >, LT_WalkBoth, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_lineactivator, sd_none },
	// Elevator_Call_WR_Player
	{ &precon::IsAnyThing, &DoGeneric< Elevator >, LT_WalkBoth, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_lineactivator, sd_none },
	// Elevator_Call_S1_Player
	{ &precon::IsAnyThing, &DoGenericSwitchOnce< Elevator >, LT_SwitchFront, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_lineactivator, sd_none },
	// Elevator_Call_SR_Player
	{ &precon::IsAnyThing, &DoGenericSwitch< Elevator >, LT_SwitchFront, LL_None, constants::liftspeeds[ Speed_Normal ], 0, stt_lineactivator, sd_none },
	// Floor_ChangeTexture_NumericModel_W1_Player
	{ &precon::IsPlayer, &DoGenericOnce< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_down, 0, sct_copyboth, scm_numeric, sc_nocrush },
	// Floor_ChangeTexture_NumericModel_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Floor >, LT_WalkBoth, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_down, 0, sct_copyboth, scm_numeric, sc_nocrush },
	// Floor_ChangeTexture_NumericModel_S1_Player
	{ &precon::IsPlayer, &DoGenericSwitchOnce< Floor >, LT_SwitchFront, LL_None, constants::floorspeeds[ Speed_Slow ], 0, stt_nosearch, sd_down, 0, sct_copyboth, scm_numeric, sc_nocrush },
	// Transfer_Properties_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Teleport_LineSilentPreserve_W1_All
	{ &precon::CanTeleportAll, &DoGenericOnce< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_toline | tt_preserveangle | tt_silent },
	// Teleport_LineSilentPreserve_WR_All
	{ &precon::CanTeleportAll, &DoGeneric< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_toline | tt_preserveangle | tt_silent },
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
	{ &precon::IsPlayer, &DoGeneric< Stairs >, LT_WalkBoth, LL_None, constants::slowstairsspeed, 0, IntToFixed( 8 ), 0, 0 },
	// Stairs_BuildBy16Fast_WR_Player
	{ &precon::IsPlayer, &DoGeneric< Stairs >, LT_WalkBoth, LL_None, constants::stairspeeds[ Speed_Turbo ], 0, IntToFixed( 16 ), 0, 0 },
	// Stairs_BuildBy8_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Stairs >, LT_SwitchFront, LL_None, constants::slowstairsspeed, 0, IntToFixed( 8 ), 0, 0 },
	// Stairs_BuildBy16Fast_SR_Player
	{ &precon::IsPlayer, &DoGenericSwitch< Stairs >, LT_SwitchFront, LL_None, constants::stairspeeds[ Speed_Turbo ], 0, IntToFixed( 16 ), 0, 0 },
	// Texture_Translucent_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Transfer_CeilingLighting_Always
	{ &precon::NeverActivate, nullptr, LT_None, LL_None },
	// Teleport_LineSilentReversed_W1_All
	{ &precon::CanTeleportAll, &DoGenericOnce< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_toline | tt_reverseangle | tt_silent },
	// Teleport_LineSilentReversed_WR_All
	{ &precon::CanTeleportAll, &DoGeneric< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_toline | tt_reverseangle | tt_silent },
	// Teleport_LineSilentReversed_W1_Monsters
	{ &precon::CanTeleportMonster, &DoGenericOnce< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_toline | tt_reverseangle | tt_silent },
	// Teleport_LineSilentReversed_WR_Monsters
	{ &precon::CanTeleportMonster, &DoGeneric< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_toline | tt_reverseangle | tt_silent },
	// Teleport_LineSilentPreserve_W1_Monsters
	{ &precon::CanTeleportMonster, &DoGenericOnce< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_toline | tt_preserveangle | tt_silent },
	// Teleport_LineSilentPreserve_WR_Monsters
	{ &precon::CanTeleportMonster, &DoGeneric< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_toline | tt_preserveangle | tt_silent },
	// Teleport_ThingSilent_W1_Monsters
	{ &precon::CanTeleportMonster, &DoGenericOnce< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_tothing | tt_silent },
	// Teleport_ThingSilent_WR_Monsters
	{ &precon::CanTeleportMonster, &DoGeneric< Teleport >, LT_WalkFront, LL_None, 0, 0, tt_tothing | tt_silent },

	/* MBF specials */

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

static lineaction_t* GetBuiltInAction( int32_t special )
{
	return (lineaction_t*)&builtinlineactions[ special ];
}

template< typename _ty >
static void SetActionActivators( lineaction_t* action, uint32_t special, bool allowmonster )
{
	uint32_t trigger = special & Generic_Trigger_Mask;
	bool isswitch = trigger == Generic_Trigger_S1 || trigger == Generic_Trigger_SR
					|| trigger == Generic_Trigger_G1 || trigger == Generic_Trigger_GR;
	bool repeatable = ( special & Generic_Trigger_Repeatable ) == Generic_Trigger_Repeatable;

	action->precondition = allowmonster ? &precon::IsAnyThing : &precon::IsPlayer;
	if( isswitch )
	{
		action->action = repeatable ? &DoGenericSwitch< _ty > : &DoGenericSwitchOnce< _ty >;
	}
	else
	{
		action->action = repeatable ? &DoGeneric< _ty > : &DoGenericOnce< _ty >;
	}
	action->trigger = constants::triggers[ trigger ];
}

static lineaction_t* CreateBoomGeneralisedStairsAction( line_t* line )
{
	constexpr fixed_t targetdistance[] =
	{
		IntToFixed( -4 ),				// Stairs_Target_4
		IntToFixed( -8 ),				// Stairs_Target_8
		IntToFixed( -16 ),				// Stairs_Target_16
		IntToFixed( -24 ),				// Stairs_Target_24
		IntToFixed( 4 ),				// Stairs_Target_4 with Stairs_Direction_Up
		IntToFixed( 8 ),				// Stairs_Target_8 with Stairs_Direction_Up
		IntToFixed( 16 ),				// Stairs_Target_16 with Stairs_Direction_Up
		IntToFixed( 24 ),				// Stairs_Target_24 with Stairs_Direction_Up
	};

	lineaction_t* action = (lineaction_t*)Z_MallocAs( lineaction_t, PU_LEVEL, nullptr );

	bool allowmonster	= ( line->special & Stairs_AllowMonsters_Mask ) == Stairs_AllowMonsters_Mask;
	bool ignoretexture	= ( line->special & Stairs_IgnoreTexture_Mask ) == Stairs_IgnoreTexture_Yes;
	uint32_t target		= ( line->special & Stairs_WithMinusTargets_Mask ) >> Stairs_Target_Shift;
	uint32_t speed		= ( line->special & Generic_Speed_Mask ) >> Generic_Speed_Shift;

	SetActionActivators< Stairs >( action, line->special, allowmonster );

	action->lock = LL_None;
	action->speed = constants::stairspeeds[ speed ];
	action->delay = 0;
	action->param1 = targetdistance[ target ];
	action->param2 = ignoretexture;
	action->param3 = false; // Crush

	return action;
}

static lineaction_t* CreateBoomGeneralisedCrusherAction( line_t* line )
{
	lineaction_t* action = (lineaction_t*)Z_MallocAs( lineaction_t, PU_LEVEL, nullptr );

	bool allowmonster	= ( line->special & Crusher_AllowMonsters_Mask ) == Crusher_AllowMonsters_Yes;
	uint32_t speed		= ( line->special & Generic_Speed_Mask ) >> Generic_Speed_Shift;
	bool silent			= ( line->special & Crusher_Silent_Mask ) == Crusher_Silent_Yes;

	SetActionActivators< Ceiling >( action, line->special, allowmonster );
	action->lock = LL_None;
	action->speed = constants::ceilingspeeds[ speed ];
	action->delay = 0;
	action->param1 = constants::crushingspeeds[ speed ];
	action->param2 = silent ? cs_none : cs_movement;

	return action;
}

static lineaction_t* CreateBoomGeneralisedCeilingAction( line_t* line )
{
	constexpr uint32_t targetmapping[] =
	{
		stt_highestneighborfloor,		// Ceiling_Target_HighestNeighborFloor
		stt_lowestneighborfloor,		// Ceiling_Target_LowestNeighborFloor
		stt_nexthighestneighborfloor,	// Ceiling_Target_NearestNeighborFloor
		stt_lowestneighborceiling,		// Ceiling_Target_LowestNeighborCeiling
		stt_floor,						// Ceiling_Target_Floor
		stt_shortestlowertexture,		// Ceiling_Target_ShortestLowerTexture
		stt_nosearch,					// Ceiling_Target_24
		stt_nosearch,					// Ceiling_Target_32
	};

	constexpr fixed_t targetdistance[] =
	{
		0,								// Ceiling_Target_HighestNeighborFloor
		0,								// Ceiling_Target_LowestNeighborFloor
		0,								// Ceiling_Target_NearestNeighborFloor
		0,								// Ceiling_Target_LowestNeighborCeiling
		0,								// Ceiling_Target_Ceiling
		0,								// Ceiling_Target_ShortestLowerTexture
		IntToFixed( 24 ),				// Ceiling_Target_24
		IntToFixed( 32 ),				// Ceiling_Target_32
	};

	lineaction_t* action = (lineaction_t*)Z_MallocAs( lineaction_t, PU_LEVEL, nullptr );

	bool allowmonster	= ( line->special & Ceiling_AllowMonsters_Yes ) == Ceiling_AllowMonsters_Yes;
	uint32_t target		= ( line->special & Ceiling_Target_Mask ) >> Ceiling_Target_Shift;
	uint32_t direction	= ( line->special & Ceiling_Direction_Mask ) == Ceiling_Direction_Down ? sd_down : sd_up;
	uint32_t speed		= ( line->special & Generic_Speed_Mask ) >> Generic_Speed_Shift;
	uint32_t change		= ( line->special & Ceiling_Change_Mask ) >> Ceiling_Change_Shift;

	SetActionActivators< Ceiling >( action, line->special, allowmonster );

	action->lock = LL_None;
	action->speed = constants::ceilingspeeds[ speed ];
	action->delay = 0;
	action->param1 = targetmapping[ target ];
	action->param2 = direction;
	action->param3 = targetdistance[ target ];
	action->param4 = change;
	action->param5 = ( line->special & Ceiling_Crush_Mask ) == Ceiling_Crush_Yes;
	action->param6 = ( line->special & Ceiling_Model_Mask ) == Ceiling_Model_Numeric;

	return action;
}

static lineaction_t* CreateBoomGeneralisedFloorAction( line_t* line )
{
	constexpr uint32_t targetmapping[] =
	{
		stt_highestneighborfloor,		// Floor_Target_HighestNeighborFloor
		stt_lowestneighborfloor,		// Floor_Target_LowestNeighborFloor
		stt_nexthighestneighborfloor,	// Floor_Target_NearestNeighborFloor
		stt_lowestneighborceiling,		// Floor_Target_LowestNeighborCeiling
		stt_ceiling,					// Floor_Target_Ceiling
		stt_shortestlowertexture,		// Floor_Target_ShortestLowerTexture
		stt_nosearch,					// Floor_Target_24
		stt_nosearch,					// Floor_Target_32
	};

	constexpr fixed_t targetdistance[] =
	{
		0,								// Floor_Target_HighestNeighborFloor
		0,								// Floor_Target_LowestNeighborFloor
		0,								// Floor_Target_NearestNeighborFloor
		0,								// Floor_Target_LowestNeighborCeiling
		0,								// Floor_Target_Ceiling
		0,								// Floor_Target_ShortestLowerTexture
		IntToFixed( 24 ),				// Floor_Target_24
		IntToFixed( 32 ),				// Floor_Target_32
	};

	lineaction_t* action = (lineaction_t*)Z_MallocAs( lineaction_t, PU_LEVEL, nullptr );

	bool allowmonster	= ( line->special & Floor_AllowMonsters_Yes ) == Floor_AllowMonsters_Yes;
	uint32_t target		= ( line->special & Floor_Target_Mask ) >> Floor_Target_Shift;
	uint32_t direction	= ( line->special & Floor_Direction_Mask ) == Floor_Direction_Down ? sd_down : sd_up;
	uint32_t speed		= ( line->special & Generic_Speed_Mask ) >> Generic_Speed_Shift;
	uint32_t change		= ( line->special & Floor_Change_Mask ) >> Floor_Change_Shift;

	SetActionActivators< Floor >( action, line->special, allowmonster );

	action->lock = LL_None;
	action->speed = constants::floorspeeds[ speed ];
	action->delay = 0;
	action->param1 = targetmapping[ target ];
	action->param2 = direction;
	action->param3 = targetdistance[ target ];
	action->param4 = change;
	action->param5 = ( line->special & Floor_Crush_Mask ) == Floor_Crush_Yes;
	action->param6 = ( line->special & Floor_Model_Mask ) == Floor_Model_Numeric;

	return action;
}

static lineaction_t* CreateBoomGeneralisedLiftAction( line_t* line )
{
	constexpr uint32_t targetmapping[] =
	{
		stt_lowestneighborfloor,
		stt_nextlowestneighborfloor,
		stt_lowestneighborceiling,
		stt_perpetual,
	};

	lineaction_t* action = (lineaction_t*)Z_MallocAs( lineaction_t, PU_LEVEL, nullptr );

	bool allowmonster	= ( line->special & Lift_AllowMonsters_Yes ) == Lift_AllowMonsters_Yes;
	uint32_t delay		= ( line->special & Lift_Delay_Mask ) >> Lift_Delay_Shift;
	uint32_t target		= targetmapping[ ( line->special & Lift_Target_Mask ) >> Lift_Target_Shift ];
	uint32_t speed		= ( line->special & Generic_Speed_Mask ) >> Generic_Speed_Shift;

	if( target == stt_perpetual )
	{
		SetActionActivators< PerpetualLiftStart >( action, line->special, allowmonster );
	}
	else
	{
		SetActionActivators< Lift >( action, line->special, allowmonster );
	}

	action->lock = LL_None;
	action->speed = constants::liftspeeds[ speed ];
	action->delay = constants::liftdelay[ delay ];
	action->param1 = target;

	return action;
}

static lineaction_t* CreateBoomGeneralisedDoorAction( line_t* line )
{
	lineaction_t* action = (lineaction_t*)Z_MallocAs( lineaction_t, PU_LEVEL, nullptr );

	uint32_t trigger	= line->special & Generic_Trigger_Mask;
	bool manualuse		= trigger == Generic_Trigger_U1 || trigger == Generic_Trigger_UR;
	bool isswitch		= trigger == Generic_Trigger_S1 || trigger == Generic_Trigger_SR
							|| trigger == Generic_Trigger_G1 || trigger == Generic_Trigger_GR;
	bool repeatable		= ( line->special & Generic_Trigger_Repeatable ) == Generic_Trigger_Repeatable;
	uint32_t speed		= ( line->special & Generic_Speed_Mask ) >> Generic_Speed_Shift;

	uint32_t kind		= ( line->special & Door_Type_Mask );
	bool isdown			= kind == Door_Type_CloseWaitOpen || kind == Door_Type_Close;
	bool isup			= kind == Door_Type_OpenWaitClose || kind == Door_Type_Open;
	bool isdelayable	= kind == Door_Type_CloseWaitOpen || kind == Door_Type_OpenWaitClose;
	bool allowmonster	= ( line->special & Door_AllowMonsters_Yes ) == Door_AllowMonsters_Yes;
	uint32_t delay		= ( line->special & Door_Delay_Mask ) >> Door_Delay_Shift;

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
	action->param1 = isup ? sd_open
						: isdown ? sd_close
							: sd_none;
	action->param2 = door_noraise;

	return action;
}

static lineaction_t* CreateBoomGeneralisedLockedDoorAction( line_t* line )
{
	lineaction_t* action = (lineaction_t*)Z_MallocAs( lineaction_t, PU_LEVEL, nullptr );

	uint32_t trigger	= line->special & Generic_Trigger_Mask;
	bool isswitch		= trigger == Generic_Trigger_S1 || trigger == Generic_Trigger_SR
							|| trigger == Generic_Trigger_G1 || trigger == Generic_Trigger_GR;
	bool repeatable		= ( line->special & Generic_Trigger_Repeatable ) == Generic_Trigger_Repeatable;
	uint32_t speed		= ( line->special & Generic_Speed_Mask ) >> Generic_Speed_Shift;

	uint32_t kind		= ( line->special & LockedDoor_Type_Mask );
	bool isdelayable	= kind == LockedDoor_Type_OpenWaitClose;

	uint32_t lockkind	= ( line->special & LockedDoor_Lock_Mask ) >> LockedDoor_Lock_Shift;

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
	action->param1 = sd_open;

	return action;
}

static lineaction_t* CreateBoomGeneralisedLineAction( line_t* line )
{
	if( line->special >= Generic_Stairs && line->special < Generic_Lift )
	{
		return CreateBoomGeneralisedStairsAction( line );
	}
	else if( line->special >= Generic_Lift && line->special < Generic_LockedDoor )
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
	else if( line->special >= Generic_Ceiling && line->special < Generic_Floor )
	{
		return CreateBoomGeneralisedCeilingAction( line );
	}
	else if( line->special >= Generic_Floor && line->special < Generic_Max )
	{
		return CreateBoomGeneralisedFloorAction( line );
	}
	return nullptr;
}

DOOM_C_API lineaction_t* P_GetLineActionFor( line_t* line )
{
#if 1
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
		// else if( false /* allow_mbf21_specials */ )
		// {
		// }
		else if( /*allow_boom_specials && */
				line->special >= Generic_Min && line->special < Generic_Max )
		{
			return CreateBoomGeneralisedLineAction( line );
		}
	}
#endif

	return nullptr;
}

//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//	All the new generic action handlers. Based off the original code,
//  does everything it can to retain vanilla compatibility when given
//  the correct inputs
//

#include "doomdef.h"
#include "doomstat.h"

#include "d_gamesim.h"

#include "g_game.h"

#include "i_log.h"

#include "m_bbox.h"
#include "m_random.h"

#include "p_local.h"
#include "p_lineaction.h"
#include "p_sectoraction.h"
#include "p_spec.h"

#include "s_sound.h"
#include "sounds.h"

#include "w_wad.h"

#pragma optimize( "", off )

extern "C"
{
	ceiling_t*	activeceilingshead = nullptr;
	plat_t*		activeplatshead = nullptr;
}

void Rotate( const angle_t angle, const fixed_t& x, const fixed_t& y, fixed_t& outx, fixed_t& outy )
{
	uint32_t fine = FINEANGLE( angle );
	fixed_t diffsine = finesine[ fine ];
	fixed_t diffcosine = finecosine[ fine ];

	outx = FixedMul( x, diffcosine ) - FixedMul( y, diffsine );
	outy = FixedMul( x, diffsine ) + FixedMul( y, diffcosine );
}

fixed_t P_FindShortestLowerTexture( sector_t* sector )
{
	fixed_t lowestheight = INT_MAX;
	int32_t mintextureindex = fix.shortest_lower_texture_line ? 1 : 0; // fix_shortest_lower_texture_line

	for( line_t* secline : Lines( *sector ) )
	{
		if( secline->frontside && secline->frontside->bottomtexture >= mintextureindex )
		{
			lowestheight = M_MIN( lowestheight, texturelookup[ secline->frontside->bottomtexture ]->height );
		}
		if( secline->backside && secline->backside->bottomtexture >= mintextureindex )
		{
			lowestheight = M_MIN( lowestheight, texturelookup[ secline->backside->bottomtexture ]->height );
		}
	}

	return lowestheight;
}

void P_FindMinMaxLightSurrounding( sector_t& sector, fixed_t& outmin, fixed_t& outmax )
{
	outmin = INT_MAX;
	outmax = -INT_MAX;

	for( line_t* line : Lines( sector ) )
	{
		sector_t* other = line->frontsector->index == sector.index ? line->backsector : line->frontsector;
		if( other != nullptr )
		{
			outmin = M_MIN( outmin, other->lightlevel );
			outmax = M_MAX( outmax, other->lightlevel );
		}
	}
}

int32_t PerformOperationOnSectors( line_t* line, mobj_t* activator
								, std::function< bool( sector_t& ) >&& precon
								, std::function< int32_t( line_t*, mobj_t*, sector_t& ) >&& createfunc )
{
	int32_t createdcount = 0;
	if( line->action->AnimatedActivationType() == LT_Use )
	{
		if( line->backsector == nullptr )
		{
			I_Error("PerformOperationOnSectors: Dx special type on 1-sided linedef");
			return 0;
		}
		if( precon( *line->backsector ) )
		{
			createdcount = createfunc( line, activator, *line->backsector );
		}
	}
	else
	{
		for( sector_t& sector : Sectors() )
		{
			if( sector.tag == line->tag && precon( sector ) )
			{
				createdcount += createfunc( line, activator, sector );
			}
		}
	}

	return createdcount;
}

// =================
//     CEILINGS
// =================

void P_AddActiveCeilingGeneric( ceiling_t* ceiling )
{
	ceiling->nextactive = activeceilingshead;
	ceiling->prevactive = nullptr;
	if( activeceilingshead )
	{
		activeceilingshead->prevactive = ceiling;
	}
	activeceilingshead = ceiling;
}

void P_RemoveActiveCeilingGeneric( ceiling_t* ceiling )
{
	for( ceiling_t* currceiling = activeceilingshead; currceiling != nullptr; currceiling = currceiling->nextactive )
	{
		if( ceiling == currceiling )
		{
			if( ceiling == activeceilingshead )
			{
				activeceilingshead = activeceilingshead->nextactive;
			}
			if( ceiling->prevactive )
			{
				ceiling->prevactive->nextactive = ceiling->nextactive;
			}
			if( ceiling->nextactive )
			{
				ceiling->nextactive->prevactive = ceiling->prevactive;
			}

			P_RemoveThinker( &ceiling->thinker );
			ceiling->sector->CeilingSpecial() = nullptr;
			return;
		}
	}
	I_Error ( "P_RemoveActiveceiling: can't find ceiling!" );
}

void P_ActivateInStasisCeilingsGeneric( int32_t tag )
{
	for( ceiling_t* currceiling = activeceilingshead; currceiling != nullptr; currceiling = currceiling->nextactive )
	{
		if( currceiling->tag == tag && currceiling->direction == sd_none )
		{
			currceiling->direction = currceiling->olddirection;
			currceiling->thinker.function.Enable();
		}
	}
}

DOOM_C_API void T_MoveCeilingGeneric( ceiling_t* ceiling )
{
	result_e res = ok;
	bool sectorsound = false;
	
	switch(ceiling->direction)
	{
	case sd_none:
	default:
		break;

	case sd_up:
		// UP
		res = T_MovePlane( ceiling->sector, ceiling->speed, ceiling->topheight, false, 1, ceiling->direction );
		sectorsound = !( leveltime & 7 );
		break;

	case sd_down:
		res = T_MovePlane(ceiling->sector, ceiling->speed, ceiling->bottomheight, ceiling->crush, 1, ceiling->direction );
		sectorsound = !( leveltime & 7 );
		break;
	}

	if( sectorsound && ( ceiling->sound & cs_movement ) )
	{
		S_StartSound( &ceiling->sector->soundorg, sfx_stnmov );
	}

	if( res == pastdest )
	{
		if( sectorsound && ( ceiling->sound & cs_endstop ) )
		{
			S_StartSound( &ceiling->sector->soundorg, sfx_pstop );
		}

		if( ceiling->perpetual )
		{
			ceiling->direction = ceiling->direction == sd_down ? sd_up : sd_down;
			ceiling->speed = ceiling->normalspeed;
		}
		else
		{
			if( ceiling->newspecial >= 0 )
			{
				ceiling->sector->special = ceiling->newspecial;
			}
			if( ceiling->newtexture >= 0 )
			{
				ceiling->sector->ceilingpic = ceiling->newtexture;
			}
			P_RemoveActiveCeilingGeneric( ceiling );
		}
	}
	else
	{
		if( res == crushed )
		{
			ceiling->speed = ceiling->crushingspeed;
		}
	}
}

DOOM_C_API int32_t EV_StopAnyCeilingGeneric( line_t* line, mobj_t* activator )
{
	for( ceiling_t* currceil = activeceilingshead; currceil != nullptr; currceil = currceil->nextactive )
	{
		if( currceil->sector->tag == line->tag && currceil->direction != sd_none )
		{
			currceil->thinker.function.Disable();
			currceil->olddirection = currceil->direction;
			currceil->direction = sd_none;
		}
	}

	return 1;
}

DOOM_C_API int32_t EV_DoCrusherGeneric( line_t* line, mobj_t* activator )
{
	P_ActivateInStasisCeilingsGeneric( line->tag );

	return PerformOperationOnSectors( line, activator
									, []( sector_t& sector ) -> bool { return sector.CeilingSpecial() == nullptr; }
									, []( line_t* line, mobj_t* activator, sector_t& sector ) -> int32_t
	{
		ceiling_t* ceiling = (ceiling_t*)Z_MallocZero( sizeof(ceiling_t), PU_LEVSPEC, 0 );
		P_AddThinker( &ceiling->thinker );
		P_AddActiveCeilingGeneric( ceiling );
		ceiling->sector = &sector;
		ceiling->sector->CeilingSpecial() = ceiling;
		ceiling->thinker.function = &T_MoveCeilingGeneric;
		ceiling->type = genericCeiling;
		ceiling->crush = true;
		ceiling->direction = sd_down;
		ceiling->speed = line->action->speed;
		ceiling->normalspeed = line->action->speed;
		ceiling->crushingspeed = line->action->param1;
		ceiling->sound = line->action->param2;
		ceiling->perpetual = true;
		ceiling->tag = sector.tag;
		ceiling->topheight = sector.ceilingheight;
		ceiling->bottomheight = sector.floorheight + IntToFixed( 8 );
		ceiling->newspecial = -1;
		ceiling->newtexture = -1;

		return 1;
	} );
}

DOOM_C_API int32_t EV_DoCeilingGeneric( line_t* line, mobj_t* activator )
{
	return PerformOperationOnSectors( line, activator
									, []( sector_t& sector ) -> bool { return sector.CeilingSpecial() == nullptr; }
									, []( line_t* line, mobj_t* activator, sector_t& sector ) -> int32_t
	{
		ceiling_t* ceiling = (ceiling_t*)Z_MallocZero( sizeof(ceiling_t), PU_LEVSPEC, 0 );
		P_AddThinker( &ceiling->thinker );
		P_AddActiveCeilingGeneric( ceiling );
		ceiling->sector = &sector;
		ceiling->sector->CeilingSpecial() = ceiling;
		ceiling->thinker.function = &T_MoveCeilingGeneric;
		ceiling->type = genericCeiling;
		ceiling->crush = line->action->param6 == sc_crush;
		ceiling->direction = line->action->param2;
		ceiling->speed = line->action->speed;
		ceiling->normalspeed = line->action->speed;
		ceiling->crushingspeed = line->action->speed >> 3;
		ceiling->sound = cs_movement;
		ceiling->perpetual = false;
		ceiling->tag = sector.tag;

		fixed_t& heighttarget = ceiling->direction == sd_down ? ceiling->bottomheight : ceiling->topheight;

		switch( (sectortargettype_t)line->action->param1 )
		{
		case stt_highestneighborfloor:
		case stt_highestneighborfloor_noaddifmatch:
			heighttarget = P_FindHighestFloorSurrounding( &sector );
			break;

		case stt_lowestneighborfloor:
			heighttarget = M_MIN( P_FindLowestFloorSurrounding( &sector ), sector.floorheight );
			break;

		case stt_nexthighestneighborfloor:
			heighttarget = P_FindNextHighestFloor( &sector ); // This will preserve a vanilla bug
			break;

		case stt_nextlowestneighborfloor:
			heighttarget = P_FindNextLowestFloorSurrounding( &sector );
			break;

		case stt_highestneighborceiling:
			heighttarget = P_FindHighestCeilingSurrounding( &sector );
			break;

		case stt_lowestneighborceiling:
			heighttarget = P_FindLowestCeilingSurrounding( &sector );
			break;

		case stt_nexthighestneighborceiling:
			heighttarget = P_FindNextHighestCeilingSurrounding( &sector );
			break;

		case stt_nextlowestneighborceiling:
			heighttarget = P_FindNextLowestCeilingSurrounding( &sector );
			break;

		case stt_floor:
			heighttarget = sector.floorheight;
			break;

		case stt_ceiling:
			I_LogAddEntryVar( Log_Error, "EV_DoCeilingGeneric: Line %d is trying to move a sector's ceiling to the ceiling", line->index );
			break;

		case stt_shortestlowertexture:
			heighttarget = sector.ceilingheight + IntToFixed( P_FindShortestLowerTexture( &sector ) * ceiling->direction );
			break;

		case stt_perpetual:
			I_LogAddEntryVar( Log_Error, "EV_DoCeilingGeneric: Line %d is trying start a perpetual ceiling, try EV_DoCrusherGeneric", line->index );
			break;

		case stt_nosearch:
		default:
			heighttarget = sector.ceilingheight;
			break;
		}

		heighttarget += (fixed_t)line->action->param3;

		ceiling->newspecial = -1;
		ceiling->newtexture = -1;

		auto SetSpecial = []( sector_t& sourcesector
							, ceiling_t* target
							, sectorchangetype_t type
							, sector_t& setfrom )
		{
			if( target->direction == sd_up )
			{
				target->newspecial = type == sct_zerospecial
									? 0
									: type == sct_copyboth
										? setfrom.special
										: -1;
				target->newtexture = ( type == sct_copytexture || type == sct_copyboth )
									? setfrom.ceilingpic
									: -1;
			}
			else
			{
				sourcesector.special = type == sct_zerospecial
										? 0
										: type == sct_copyboth
											? setfrom.special
											: sourcesector.special;
				sourcesector.ceilingpic = ( type == sct_copytexture || type == sct_copyboth )
										? setfrom.ceilingpic
										: sourcesector.ceilingpic;
			}
		};

		if( line->action->param4 != sct_none )
		{
			if( line->action->param5 == scm_numeric )
			{
				// The search functions above already find the correct sector.
				// We should really cache the result that way...
				for( line_t* secline : Lines( sector ) )
				{
					if( secline->TwoSided() )
					{
						sector_t& setfrom = secline->frontsector->index == sector.index
											? *secline->backsector
											: *secline->frontsector;
						if( setfrom.ceilingheight == heighttarget )
						{
							SetSpecial( sector, ceiling, (sectorchangetype_t)line->action->param4, setfrom );
							break;
						}
					}
				}
			}
			else // Trigger model
			{
				if( line->frontsector == nullptr )
				{
					I_LogAddEntryVar( Log_Error, "EV_DoCeilingGeneric: Line %d is using a trigger model without a front sector", line->index );
				}
				else
				{
					SetSpecial( sector, ceiling, (sectorchangetype_t)line->action->param4, *line->frontsector );
				}
			}
		}

		return 1;
	} );
}

// =================
//       DOORS
// =================

constexpr int32_t DoorSoundFor( sectordir_t dir, doombool blaze )
{
	constexpr sfxenum_t doorsounds[] =
	{
		sfx_dorcls,
		sfx_None,
		sfx_doropn,
		sfx_bdcls,
		sfx_None,
		sfx_bdopn,
	};

	return doorsounds[ (dir + 1) + (blaze ? 3 : 0) ];
}

DOOM_C_API void T_VerticalDoorGeneric( vldoor_t* door )
{
	if( door->idletics > 0 )
	{
		if( --door->idletics > 0 )
		{
			return;
		}

		if( door->direction != sd_none )
		{
			S_StartSound( &door->sector->soundorg, DoorSoundFor( door->direction, door->blazing ) );
		}
	}

	result_e	res;

	auto DoLight = []( int32_t tag, fixed_t level )
	{
		for( sector_t& sector : Sectors() )
		{
			if( sector.tag == tag )
			{
				sector.lightlevel = level;
				sector.lastactivetic = gametic;
			}
		}
	};

	switch(door->direction)
	{
	case sd_none:
		// WAITING
		if (!--door->topcountdown)
		{
			door->direction = door->nextdirection;
			door->nextdirection = sd_none;
			S_StartSound( &door->sector->soundorg, DoorSoundFor( door->direction, door->blazing ) );
		}
		break;

	case sd_close:
		// DOWN
		res = T_MovePlane( door->sector, door->speed, door->sector->floorheight, false, 1, door->direction );

		if (res == pastdest)
		{
			door->direction = sd_none;
			if( door->lighttag > 0 )
			{
				DoLight( door->lighttag, door->lightmin );
			}

			if( door->topcountdown > 0 )
			{
				door->nextdirection = sd_open;
			}
			else
			{
				door->sector->CeilingSpecial() = nullptr;
				P_RemoveThinker (&door->thinker);  // unlink and free
				if( door->blazing )
				{
					S_StartSound( &door->sector->soundorg, sfx_bdcls );
				}
			}
		}
		else if( res == crushed )
		{
			if( !door->keepclosingoncrush )
			{
				door->topcountdown = door->dontrecloseoncrush ? 0 : door->topwait;
				door->direction = sd_open;
				S_StartSound(&door->sector->soundorg, sfx_doropn);
			}
		}

		if( res != pastdest && !comp.door_tagged_light_is_abrupt && door->lighttag > 0 )
		{
			fixed_t percent = FixedDiv( door->sector->ceilingheight - door->sector->floorheight, door->topheight - door->sector->floorheight );
			fixed_t lightminfixed = IntToFixed( door->lightmin );
			fixed_t lightmaxfixed = IntToFixed( door->lightmax );

			DoLight( door->lighttag, FixedToInt( FixedLerp( lightminfixed, lightmaxfixed, percent ) ) );
		}
		break;
	
	case sd_open:
		// UP
		res = T_MovePlane( door->sector, door->speed, door->topheight, false, 1, door->direction );
	
		if (res == pastdest)
		{
			door->direction = sd_none;
			if( door->lighttag > 0 )
			{
				DoLight( door->lighttag, door->lightmax );
			}

			if( door->topcountdown > 0 )
			{
				door->nextdirection = sd_close;
			}
			else
			{
				door->sector->CeilingSpecial() = nullptr;
				P_RemoveThinker( &door->thinker ); // unlink and free
			}
		}
		else if( !comp.door_tagged_light_is_abrupt && door->lighttag > 0 )
		{
			fixed_t percent = FixedDiv( door->sector->ceilingheight - door->sector->floorheight, door->topheight - door->sector->floorheight );
			fixed_t lightminfixed = IntToFixed( door->lightmin );
			fixed_t lightmaxfixed = IntToFixed( door->lightmax );

			DoLight( door->lighttag, FixedToInt( FixedLerp( lightminfixed, lightmaxfixed, percent ) ) );
		}
		break;

	default:
		break;
	}
}

DOOM_C_API int32_t EV_DoDoorGeneric( line_t* line, mobj_t* activator )
{
	if( line->action->AnimatedActivationType() == LT_Use && !line->backsector )
	{
		I_Error("EV_DoDoorGeneric: Dx special type on 1-sided linedef");
		return 0;
	}

	doombool raising = line->action->AnimatedActivationType() == LT_Use
					&& (sectordir_t)line->action->param1 == sd_open;
	if( raising )
	{
		if( (doorraise_t)line->action->param2 == door_raiselower
			&& line->backsector->CeilingSpecial() )
		{
			vldoor_t* door = thinker_cast< vldoor_t >( line->backsector->CeilingSpecial() );
			if( door == nullptr )
			{
				// Vanilla would stomp over values regardless. We'll just crash here for now.
				I_Error("EV_DoDoorGeneric: Dx activating on a sector with a non-door thinker");
				return 0;
			}

			if( door->direction == sd_close )
			{
				door->direction = sd_open;
				door->topcountdown = door->topwait;
			}
			else if( activator->player != nullptr )
			{
				door->direction = sd_close;
				door->topcountdown = 0;
			}

			return 1;
		}
	}

	return PerformOperationOnSectors( line, activator
									, []( sector_t& sector ) -> bool { return sector.CeilingSpecial() == nullptr; }
									, []( line_t* line, mobj_t* activator, sector_t& sector ) -> int32_t
	{
		vldoor_t* door = (vldoor_t*)Z_MallocZero( sizeof(vldoor_t), PU_LEVSPEC, 0 );
		P_AddThinker (&door->thinker);
		door->sector = &sector;
		door->sector->CeilingSpecial() = door;
		door->thinker.function = T_VerticalDoorGeneric;
		door->topwait = line->action->delay;
		door->topcountdown = door->topwait;
		door->speed = line->action->speed;
		door->direction = (sectordir_t)line->action->param1;
		door->nextdirection = sd_none;
		door->blazing = door->speed >= (VDOORSPEED * 4);
		door->keepclosingoncrush = ( door->direction == sd_close && door->topwait == 0 );
		door->dontrecloseoncrush = ( door->direction == sd_close && door->topwait != 0 );
		door->lighttag = 0;

		if( sim.door_tagged_light )
		{
			if( line->action->AnimatedActivationType() == LT_Use
				&& line->tag != 0
				&& line->frontsector != nullptr
				&& line->backsector != nullptr )
			{
				door->lighttag = line->tag;
				P_FindMinMaxLightSurrounding( *line->backsector, door->lightmin, door->lightmax );
			}
		}

		if( door->direction == sd_close && door->topwait > 0 )
		{
			door->topheight = sector.ceilingheight;
		}
		else
		{
			door->topheight = P_FindLowestCeilingSurrounding( &sector );
			door->topheight -= IntToFixed( 4 );
		}

		if( door->direction == sd_close || door->topheight != sector.ceilingheight )
		{
			S_StartSound( &door->sector->soundorg, DoorSoundFor( door->direction, door->blazing ) );
		}

		return 1;
	} );
}

// Not really meant to be used for anything other than the vanilla sector specials
DOOM_C_API int32_t EV_DoDelayedDoorGeneric( sector_t* sector, sectordir_t dir, int32_t delaybeforestart, int32_t topdelay, fixed_t speed )
{
	vldoor_t* door = (vldoor_t*)Z_MallocZero( sizeof(vldoor_t), PU_LEVSPEC, 0 );
	P_AddThinker (&door->thinker);
	door->sector = sector;
	door->sector->CeilingSpecial() = door;
	door->thinker.function = T_VerticalDoorGeneric;
	door->topwait = topdelay;
	door->topcountdown = topdelay;
	door->topheight = dir == sd_down ? sector->ceilingheight
									: P_FindLowestCeilingSurrounding( sector ) - IntToFixed( 4 );
	door->speed = speed;
	door->direction = dir;
	door->nextdirection = sd_none;
	door->blazing = door->speed >= ( VDOORSPEED * 4 );
	door->keepclosingoncrush = false;
	door->dontrecloseoncrush = topdelay == 0;
	door->lighttag = 0;
	door->idletics = delaybeforestart;

	return 1;
}

// =================
//     ELEVATORS
// =================

DOOM_C_API void T_MoveElevatorGeneric( elevator_t* elevator )
{
	fixed_t oldfloor = elevator->sector->floorheight;
	fixed_t oldceil = elevator->sector->ceilingheight;

	result_e floorres = T_MovePlane( elevator->sector, elevator->speed, elevator->floordest, false, 0, elevator->direction );
	result_e ceilres = T_MovePlane( elevator->sector, elevator->speed, elevator->ceilingdest, false, 1, elevator->direction );

	if( floorres != ceilres )
	{
		elevator->sector->floorheight = oldfloor;
		elevator->sector->ceilingheight = oldceil;
		return;
	}

	if( floorres == pastdest && ceilres == pastdest )
	{
		S_StartSound( &elevator->sector->soundorg, sfx_pstop );
		P_RemoveThinker( &elevator->thinker );
		elevator->sector->FloorSpecial() = nullptr;
		elevator->sector->CeilingSpecial() = nullptr;
		return;
	}

	if( !( leveltime & 7 ) )
	{
		S_StartSound( &elevator->sector->soundorg, sfx_stnmov );
	}
}

DOOM_C_API int32_t EV_DoElevatorGeneric( line_t* line, mobj_t* activator )
{
	return PerformOperationOnSectors( line, activator
									, []( sector_t& sector ) -> bool { return sector.FloorSpecial() == nullptr && sector.CeilingSpecial() == nullptr; }
									, []( line_t* line, mobj_t* activator, sector_t& sector ) -> int32_t
	{
		elevator_t* elevator = (elevator_t*)Z_MallocZero( sizeof(elevator_t), PU_LEVSPEC, 0 );
		P_AddThinker( &elevator->thinker );
		elevator->sector = &sector;
		elevator->sector->FloorSpecial() = elevator;
		elevator->sector->CeilingSpecial() = elevator;
		elevator->thinker.function = &T_MoveElevatorGeneric;
		elevator->speed = line->action->speed;
		elevator->direction = (sectordir_t)line->action->param2;

		fixed_t dist = 0;
		switch( line->action->param1 )
		{
		case stt_nexthighestneighborfloor:
			dist = P_FindNextHighestFloorSurrounding( &sector ) - sector.floorheight;
			break;
		case stt_nextlowestneighborfloor:
			dist = P_FindNextLowestFloorSurrounding( &sector ) - sector.floorheight;
			break;
		case stt_lineactivator:
			dist = line->frontsector->floorheight - sector.floorheight;
			elevator->direction = dist >= 0 ? sd_up : sd_down;
			break;
		}

		elevator->floordest = sector.floorheight + dist;
		elevator->ceilingdest = sector.ceilingheight + dist;

		S_StartSound( &sector.soundorg, sfx_stnmov );

		return 1;
	} );
}

// =================
//      FLOORS
// =================

DOOM_C_API void T_MoveFloorGeneric(floormove_t* floor)
{
	result_e	res;
	
	res = T_MovePlane(floor->sector,
						floor->speed,
						floor->floordestheight,
						floor->crush,0,floor->direction);

	if( !( leveltime & 7 ) )
	{
		S_StartSound( &floor->sector->soundorg, sfx_stnmov );
	}

	if( res == pastdest )
	{
		floor->sector->FloorSpecial() = nullptr;
		if( floor->newspecial >= 0 )
		{
			floor->sector->special = floor->newspecial;
		}
		if( floor->texture >= 0 )
		{
			floor->sector->floorpic = floor->texture;
		}

		P_RemoveThinker( &floor->thinker );

		S_StartSound( &floor->sector->soundorg, sfx_pstop );
	}
}

DOOM_C_API int32_t EV_DoStairsGeneric( line_t* line, mobj_t* activator )
{
	return PerformOperationOnSectors( line, activator
									, []( sector_t& sector ) -> bool { return sector.FloorSpecial() == nullptr; }
									, []( line_t* line, mobj_t* activator, sector_t& sector ) -> int32_t
	{
		ACTIONPARAM_FIXED( line, StairSize, 1 );
		ACTIONPARAM_BOOL( line, IgnoreBackTexture, 2 );
		ACTIONPARAM_BOOL( line, Crush, 3 );
		ACTIONPARAM_BOOL( line, IncrementHeightBeforeSpecialCheck, 4 );

		floormove_t* floor = (floormove_t*)Z_MallocZero( sizeof(floormove_t), PU_LEVSPEC, 0 );
		P_AddThinker( &floor->thinker );
		floor->sector = &sector;
		floor->sector->FloorSpecial() = floor;
		floor->thinker.function = &T_MoveFloorGeneric;
		floor->type = genericFloor;
		floor->speed = line->action->speed;
		floor->crush = Crush;
		floor->newspecial = -1;
		floor->texture = -1;

		fixed_t height = sector.floorheight + StairSize;
		floor->floordestheight = height;
		floor->direction = StairSize >= 0 ? sd_up : sd_down;

		int32_t createdcount = 1;
		int16_t texture = sector.floorpic;

		sector_t* currsector = &sector;
		while( currsector != nullptr )
		{
			sector_t* searchsector = currsector;
			currsector = nullptr;

			for( line_t* searchline : Lines( *searchsector ) )
			{
				if( ( searchline->flags & ML_TWOSIDED ) != ML_TWOSIDED
					|| searchline->frontsector->index != searchsector->index
					|| ( !IgnoreBackTexture && searchline->backsector->floorpic != texture )
					|| ( !IncrementHeightBeforeSpecialCheck && searchline->backsector->FloorSpecial() != nullptr )
					)
				{
					continue;
				}

				height += StairSize;

				// Redundant if IncrementHeightBeforeSpecialCheck is false, but it's a
				// compare anyway so no point in doing two compares when you can do one.
				if( searchline->backsector->FloorSpecial() != nullptr )
				{
					continue;
				}

				++createdcount;
				currsector = searchline->backsector;

				floormove_t* nextfloor = (floormove_t*)Z_MallocZero( sizeof(floormove_t), PU_LEVSPEC, 0 );
				P_AddThinker( &nextfloor->thinker );
				nextfloor->sector = searchline->backsector;
				nextfloor->sector->FloorSpecial() = nextfloor;
				nextfloor->thinker.function = &T_MoveFloorGeneric;
				nextfloor->type = genericFloor;
				nextfloor->direction = floor->direction;
				nextfloor->speed = line->action->speed;
				nextfloor->crush = line->action->param3;
				nextfloor->newspecial = -1;
				nextfloor->texture = -1;
				nextfloor->floordestheight = height;

				break;
			}
		}

		return createdcount;
	} );

}

DOOM_C_API int32_t EV_DoDonutGeneric( line_t* line, mobj_t* activator )
{
	return PerformOperationOnSectors( line, activator
									, []( sector_t& sector ) -> bool { return sector.FloorSpecial() == nullptr; }
									, []( line_t* line, mobj_t* activator, sector_t& holesector ) -> int32_t
	{
		sector_t* donutsector = getNextSector( holesector.lines[ 0 ], &holesector );
		if( donutsector == nullptr
			|| ( fix.donut_multiple_sector_thinkers && donutsector->FloorSpecial() != nullptr ) )
		{
			return 0;
		}

		for( line_t* targetline : Lines( *donutsector ) )
		{
			if( targetline->backsector == nullptr
				|| targetline->backsector->index == holesector.index )
			{
				continue;
			}

			sector_t*& target = targetline->backsector;

			floormove_t* donut = (floormove_t*)Z_MallocZero( sizeof(floormove_t), PU_LEVSPEC, 0 );
			P_AddThinker( &donut->thinker );
			donut->sector = donutsector;
			donut->sector->FloorSpecial() = donut;
			donut->thinker.function = &T_MoveFloorGeneric;
			donut->type = genericFloor;
			donut->crush = false;
			donut->direction = sd_up;
			donut->speed = line->action->speed;
			donut->floordestheight = target->floorheight;
			donut->newspecial = 0;
			donut->texture = target->floorpic;

			floormove_t* hole = (floormove_t*)Z_MallocZero( sizeof(floormove_t), PU_LEVSPEC, 0 );
			P_AddThinker( &hole->thinker );
			hole->sector = &holesector;
			hole->sector->FloorSpecial() = hole;
			hole->thinker.function = &T_MoveFloorGeneric;
			hole->type = genericFloor;
			hole->crush = false;
			hole->direction = sd_down;
			hole->speed = line->action->speed;
			hole->floordestheight = target->floorheight;
			hole->newspecial = -1;
			hole->texture = -1;

			return 2;
		}

		return 0;
	} );
}

DOOM_C_API int32_t EV_DoFloorGeneric( line_t* line, mobj_t* activator )
{
	return PerformOperationOnSectors( line, activator
									, []( sector_t& sector ) -> bool { return sector.FloorSpecial() == nullptr; }
									, []( line_t* line, mobj_t* activator, sector_t& sector ) -> int32_t
	{
		floormove_t* floor = (floormove_t*)Z_MallocZero( sizeof(floormove_t), PU_LEVSPEC, 0 );
		P_AddThinker( &floor->thinker );
		floor->sector = &sector;
		floor->sector->FloorSpecial() = floor;
		floor->thinker.function = &T_MoveFloorGeneric;
		floor->type = genericFloor;
		floor->crush = line->action->param6 == sc_crush;
		floor->direction = line->action->param2;
		floor->speed = line->action->speed;

		switch( (sectortargettype_t)line->action->param1 )
		{
		case stt_highestneighborfloor:
		case stt_highestneighborfloor_noaddifmatch:
			floor->floordestheight = P_FindHighestFloorSurrounding( &sector );
			break;

		case stt_lowestneighborfloor:
			floor->floordestheight = M_MIN( P_FindLowestFloorSurrounding( &sector ), sector.floorheight );
			break;

		case stt_nexthighestneighborfloor:
			floor->floordestheight = P_FindNextHighestFloor( &sector ); // This will preserve a vanilla bug
			break;

		case stt_nextlowestneighborfloor:
			floor->floordestheight = P_FindNextLowestFloorSurrounding( &sector );
			break;

		case stt_highestneighborceiling:
			floor->floordestheight = P_FindHighestCeilingSurrounding( &sector );
			if( sim.boom_sector_targets )
			{
				floor->floordestheight = M_MAX( floor->floordestheight, sector.ceilingheight );
			}
			break;

		case stt_lowestneighborceiling:
			floor->floordestheight = P_FindLowestCeilingSurrounding( &sector );
			floor->floordestheight = M_MIN( floor->floordestheight, sector.ceilingheight );
			break;

		case stt_nexthighestneighborceiling:
			floor->floordestheight = P_FindNextHighestCeilingSurrounding( &sector );
			break;

		case stt_nextlowestneighborceiling:
			floor->floordestheight = P_FindNextLowestCeilingSurrounding( &sector );
			break;

		case stt_floor:
			I_LogAddEntryVar( Log_Error, "EV_DoFloorGeneric: Line %d is trying to move a sector's floor to the floor", line->index );
			break;

		case stt_ceiling:
			floor->floordestheight = sector.ceilingheight;
			break;

		case stt_shortestlowertexture:
			floor->floordestheight = sector.floorheight + IntToFixed( P_FindShortestLowerTexture( &sector ) * floor->direction );
			break;

		case stt_perpetual:
			I_LogAddEntryVar( Log_Error, "EV_DoFloorGeneric: Line %d is trying start a perpetual platform, try using EV_DoPerpetualLiftGeneric instead", line->index );
			break;

		case stt_nosearch:
		default:
			floor->floordestheight = sector.floorheight;
			break;
		}

		if( line->action->param1 != stt_highestneighborfloor_noaddifmatch
			|| floor->floordestheight != sector.floorheight )
		{
			floor->floordestheight += (fixed_t)line->action->param3;
		}

		floor->newspecial = -1;
		floor->texture = -1;

		auto SetSpecial = []( sector_t& sourcesector
							, floormove_t* target
							, sectorchangetype_t type
							, sector_t& setfrom )
		{
			if( target->direction == sd_down )
			{
				target->newspecial = type == sct_zerospecial
									? 0
									: type == sct_copyboth
										? setfrom.special
										: -1;
				target->texture =	( type == sct_copytexture || type == sct_copyboth )
									? setfrom.floorpic
									: -1;
			}
			else
			{
				sourcesector.special = type == sct_zerospecial
										? 0
										: type == sct_copyboth
											? setfrom.special
											: sourcesector.special;
				sourcesector.floorpic = ( type == sct_copytexture || type == sct_copyboth )
										? setfrom.floorpic
										: sourcesector.floorpic;
			}
		};

		if( line->action->param4 != sct_none )
		{
			if( line->action->param5 == scm_numeric )
			{
				// The search functions above already find the correct sector.
				// We should really cache the result that way...
				for( line_t* secline : Lines( sector ) )
				{
					if( secline->TwoSided() )
					{
						sector_t& setfrom = secline->frontsector->index == sector.index
											? *secline->backsector
											: *secline->frontsector;
						if( setfrom.floorheight == floor->floordestheight )
						{
							SetSpecial( sector, floor, (sectorchangetype_t)line->action->param4, setfrom );
							break;
						}
					}
				}
			}
			else // Trigger model
			{
				if( line->frontsector == nullptr )
				{
					I_LogAddEntryVar( Log_Error, "EV_DoFloorGeneric: Line %d is using a trigger model without a front sector", line->index );
				}
				else
				{
					SetSpecial( sector, floor, (sectorchangetype_t)line->action->param4, *line->frontsector );
				}
			}
		}

		return 1;
	} );
}

// =================
//      LIGHTS
// =================

static void SetDirect( sector_t& sector, int32_t& lightval )
{
	sector.lightlevel = lightval;
}

static void SetHighestFirstTagged( sector_t& sector, int32_t& lightval )
{
	if( !lightval )
	{
		for( line_t* line : Lines( sector ) )
		{
			sector_t* nextsector = getNextSector( line, &sector );
			if( nextsector )
			{
				lightval = M_MAX( lightval, nextsector->lightlevel );
			}
		}
	}

	sector.lightlevel = lightval;
}

static void SetLowest( sector_t& sector, int32_t& lightval )
{
	lightval = sector.lightlevel;
	for( line_t* line : Lines( sector ) )
	{
		sector_t* nextsector = getNextSector( line, &sector );
		if( nextsector )
		{
			lightval = M_MIN( lightval, nextsector->lightlevel );
		}
	}

	sector.lightlevel = lightval;
}

static void SetHighest( sector_t& sector, int32_t& lightval )
{
	lightval = sector.lightlevel;
	for( line_t* line : Lines( sector ) )
	{
		sector_t* nextsector = getNextSector( line, &sector );
		if( nextsector )
		{
			lightval = M_MAX( lightval, nextsector->lightlevel );
		}
	}

	sector.lightlevel = lightval;
}

using lightsetfunc_t = void(*)( sector_t&, int32_t& );
constexpr lightsetfunc_t lightsetfuncs[] =
{
	&SetDirect,
	&SetHighestFirstTagged,
	&SetLowest,
	&SetHighest
};

DOOM_C_API int32_t EV_DoLightSetGeneric( line_t* line, mobj_t* activator )
{
	int32_t lightval = line->action->param1 == lightset_value ? line->action->param2 : 0;

	int32_t createdcount = 0;
	for( sector_t& sector : Sectors() )
	{
		if( sector.tag == line->tag )
		{
			++createdcount;
			lightsetfuncs[ line->action->param1 ]( sector, lightval );
			sector.lastactivetic = gametic;
		}
	}
	return createdcount;
}

DOOM_C_API int32_t EV_DoLightStrobeGeneric( line_t* line, mobj_t* activator )
{
	EV_StartLightStrobing( line );

	return 1;
}

// =================
//     PLATFORMS
// =================

void P_AddActivePlatGeneric( plat_t* plat )
{
	plat->nextactive = activeplatshead;
	plat->prevactive = nullptr;
	if( activeplatshead )
	{
		activeplatshead->prevactive = plat;
	}
	activeplatshead = plat;
}

void P_RemoveActivePlatGeneric( plat_t* plat )
{
	for( plat_t* currplat = activeplatshead; currplat != nullptr; currplat = currplat->nextactive )
	{
		if( plat == currplat )
		{
			if( plat == activeplatshead )
			{
				activeplatshead = activeplatshead->nextactive;
			}
			if( plat->prevactive )
			{
				plat->prevactive->nextactive = plat->nextactive;
			}
			if( plat->nextactive )
			{
				plat->nextactive->prevactive = plat->prevactive;
			}

			P_RemoveThinker( &plat->thinker );
			plat->sector->FloorSpecial() = nullptr;
			return;
		}
	}
	I_Error ( "P_RemoveActivePlatGeneric: can't find plat!" );
}

void P_ActivateInStasisPlatsGeneric( int tag )
{
	for( plat_t* currplat = activeplatshead; currplat != nullptr; currplat = currplat->nextactive )
	{
		if( currplat->tag == tag && currplat->status == in_stasis )
		{
			currplat->status = currplat->oldstatus;
			currplat->thinker.function.Enable();
		}
	}
}

DOOM_C_API void T_RaisePlatGeneric( plat_t* plat )
{
	result_e res = ok;
	
	switch(plat->status)
	{
	case up:
		res = T_MovePlane(plat->sector,
					plat->speed,
					plat->high,
					plat->crush,0,1);
					
		if (plat->type == raiseAndChange
			|| plat->type == raiseToNearestAndChange)
		{
			if ( !( leveltime & 7 ) )
			{
				S_StartSound(&plat->sector->soundorg, sfx_stnmov);
			}
		}

		if (res == crushed && (!plat->crush))
		{
			plat->count = plat->wait;
			plat->status = down;
			S_StartSound(&plat->sector->soundorg, sfx_pstart);
		}
		else if (res == pastdest)
		{
			plat->count = plat->wait;
			plat->status = waiting;
			S_StartSound(&plat->sector->soundorg, sfx_pstop);

			if(plat->type != perpetualRaise)
			{
				P_RemoveActivePlatGeneric(plat);
			}
		}
		break;

	case down:
		res = T_MovePlane(plat->sector,plat->speed,plat->low,false,0,-1);

		if (res == pastdest)
		{
			plat->count = plat->wait;
			plat->status = waiting;
			S_StartSound(&plat->sector->soundorg, sfx_pstop);
		}
		break;

	case waiting:
		if (!--plat->count)
		{
			if (plat->sector->floorheight == plat->low)
			{
				plat->status = up;
			}
			else
			{
				plat->status = down;
			}
			S_StartSound(&plat->sector->soundorg, sfx_pstart);
		}

	case in_stasis:
		break;
	}
}

DOOM_C_API int32_t EV_DoVanillaPlatformRaiseGeneric( line_t* line, mobj_t* activator )
{
	return PerformOperationOnSectors( line, activator
									, []( sector_t& sector ) -> bool { return sector.FloorSpecial() == nullptr; }
									, []( line_t* line, mobj_t* activator, sector_t& sector ) -> int32_t
	{
		plat_t* plat = (plat_t*)Z_MallocZero( sizeof(plat_t), PU_LEVSPEC, 0 );
		P_AddThinker( &plat->thinker );
		
		plat->sector = &sector;
		plat->sector->FloorSpecial() = plat;
		plat->thinker.function = T_RaisePlatGeneric;
		plat->crush = false;
		plat->tag = line->tag;
		plat->speed = line->action->speed;
		plat->wait = line->action->delay;
		plat->status = up;

		switch( line->action->param1 )
		{
		case stt_nexthighestneighborfloor:
			sector.floorpic = line->frontside->sector->floorpic;
			sector.special = 0;
			plat->type = raiseToNearestAndChange;
			plat->high = P_FindNextHighestFloorSurrounding( &sector );
			break;
		case stt_ceiling:
			plat->type = genericPlatform;
			plat->high = sector.ceilingheight;
			if( plat->speed == INT_MAX )
			{
				plat->speed = (sector.ceilingheight - sector.floorheight);
			}
			break;
		case stt_nosearch:
			sector.floorpic = line->frontside->sector->floorpic;
			plat->type = raiseAndChange;
			plat->high = sector.floorheight;
			break;

		default:
			break;
		}

		plat->high += line->action->param2;
		plat->low = sector.floorheight;

		S_StartSound( &sector.soundorg, sfx_stnmov );

		P_AddActivePlatGeneric( plat );

		return 1;
	} );
}

DOOM_C_API int32_t EV_DoPerpetualLiftGeneric( line_t* line, mobj_t* activator )
{
	P_ActivateInStasisPlatsGeneric( line->tag );

	return PerformOperationOnSectors( line, activator
									, []( sector_t& sector ) -> bool { return sector.FloorSpecial() == nullptr; }
									, []( line_t* line, mobj_t* activator, sector_t& sector ) -> int32_t
	{
		plat_t* plat = (plat_t*)Z_MallocZero( sizeof(plat_t), PU_LEVSPEC, 0 );
		P_AddThinker( &plat->thinker );
	
		plat->type = perpetualRaise;
		plat->sector = &sector;
		plat->sector->FloorSpecial() = plat;
		plat->thinker.function = T_RaisePlatGeneric;
		plat->crush = false;
		plat->tag = line->tag;
		plat->speed = line->action->speed;
		plat->wait = line->action->delay;
		plat->status = (plat_e)( P_Random() & 1 );

		plat->low = M_MIN( P_FindLowestFloorSurrounding( &sector ), sector.floorheight );
		plat->high = M_MAX( P_FindHighestFloorSurrounding( &sector ), sector.floorheight );

		S_StartSound( &sector.soundorg, sfx_pstart );

		P_AddActivePlatGeneric( plat );

		return 1;
	} );
}

DOOM_C_API int32_t EV_DoLiftGeneric( line_t* line, mobj_t* activator )
{
	return PerformOperationOnSectors( line, activator
									, []( sector_t& sector ) -> bool { return sector.FloorSpecial() == nullptr; }
									, []( line_t* line, mobj_t* activator, sector_t& sector ) -> int32_t
	{
		plat_t* plat = (plat_t*)Z_MallocZero( sizeof(plat_t), PU_LEVSPEC, 0 );
		P_AddThinker( &plat->thinker );
		
		plat->type = downWaitUpStay;
		plat->sector = &sector;
		plat->sector->FloorSpecial() = plat;
		plat->thinker.function = T_RaisePlatGeneric;
		plat->crush = false;
		plat->tag = line->tag;
		plat->speed = line->action->speed;
		plat->wait = line->action->delay;
		plat->status = down;

		switch( line->action->param1 )
		{
		case stt_lowestneighborfloor:
			plat->low = M_MIN( P_FindLowestFloorSurrounding( &sector ), sector.floorheight );
			break;

		case stt_nextlowestneighborfloor:
			plat->low = P_FindNextLowestFloorSurrounding( &sector );
			break;

		case stt_lowestneighborceiling:
			plat->low = P_FindLowestCeilingSurrounding( &sector );
			if( sim.boom_sector_targets )
			{
				plat->low = M_MIN( plat->low, sector.ceilingheight );
			}
			break;

		default:
			break;
		}

		plat->low = M_MIN( plat->low, sector.floorheight );
		plat->high = sector.floorheight;

		S_StartSound( &sector.soundorg, sfx_pstart );

		P_AddActivePlatGeneric( plat );

		return 1;
	} );
}

DOOM_C_API int32_t EV_StopAnyLiftGeneric( line_t* line, mobj_t* activator )
{
	for( plat_t* currplat = activeplatshead; currplat != nullptr; currplat = currplat->nextactive )
	{
		if( currplat->tag == line->tag && currplat->status != in_stasis )
		{
			currplat->oldstatus = currplat->status;
			currplat->status = in_stasis;
			currplat->thinker.function.Disable();
		}
	}

	return 1;
}

// =================
//     TELEPORTS
// =================

DOOM_C_API bool EV_DoTeleportGenericThing( line_t* line, mobj_t* activator, teleporttype_e anglebehavior, fixed_t& outtargetx, fixed_t& outtargety, fixed_t& outtargetz, angle_t& outtargetangle )
{
	if( activator->flags & MF_MISSILE )
	{
		return false;
	}

	for( sector_t& sector : Sectors() )
	{
		if ( sector.tag == line->tag )
		{
			for( mobj_t* targetmobj = sector.nosectorthinglist; targetmobj != nullptr; targetmobj = targetmobj->nosectornext )
			{
				if( targetmobj->type != MT_TELEPORTMAN )
				{
					continue;
				}

				if( P_TeleportMove( activator, targetmobj->x, targetmobj->y ) )
				{
					outtargetx				= targetmobj->x;
					outtargety				= targetmobj->y;
					outtargetangle			= targetmobj->angle;

					angle_t diff = targetmobj->angle - line->angle + ( anglebehavior == tt_reverseangle ? ANG180 : 0 );
					fixed_t momx = activator->momx;
					fixed_t momy = activator->momy;
					Rotate( diff, momx, momy, activator->momx, activator->momy );

					diff = activator->angle - line->angle;
					switch( anglebehavior )
					{
					case tt_setangle:
						outtargetangle = targetmobj->angle;
						break;
					case tt_preserveangle:
						outtargetangle = targetmobj->angle + diff;
						break;
					case tt_reverseangle:
						outtargetangle = targetmobj->angle + diff + ANG180;
						break;
					default:
						break;
					}

					if( gameversion != exe_final
						&& !comp.finaldoom_teleport_z )
					{
						outtargetz = activator->floorz;
						activator->z = outtargetz;
					}
					else
					{
						outtargetz = activator->z;
					}

					return true;
				}

				break;
			}
		}
	}

	return false;
}

DOOM_C_API bool EV_DoTeleportGenericLine( line_t* line, mobj_t* activator, teleporttype_e anglebehavior, fixed_t& outtargetx, fixed_t& outtargety, fixed_t& outtargetz, angle_t& outtargetangle )
{
	for( line_t& targetline : Lines() )
	{
		if( targetline.index != line->index
			&& targetline.tag == line->tag
			&& targetline.special == line->special )
		{
			fixed_t sourcemidx = line->v1->x + ( line->dx >> 1 );
			fixed_t sourcemidy = line->v1->y + ( line->dy >> 1 );

			fixed_t mobjtosourcex = activator->x - sourcemidx;
			fixed_t mobjtosourcey = activator->y - sourcemidy;

			angle_t extraangle = anglebehavior == tt_reverseangle ? 0 : ANG180;

			angle_t diff = targetline.angle - line->angle + extraangle;

			fixed_t targetmidx = targetline.v1->x + ( targetline.dx >> 1 );
			fixed_t targetmidy = targetline.v1->y + ( targetline.dy >> 1 );

			fixed_t destx;
			fixed_t desty;
			Rotate( diff, mobjtosourcex, mobjtosourcey, destx, desty );
			destx += targetmidx;
			desty += targetmidy;

			fixed_t sourcez = activator->z - activator->floorz;

			if( P_TeleportMove( activator, destx, desty ) )
			{
				outtargetx		= destx;
				outtargety		= desty;
				outtargetz		= sourcez + activator->floorz;
				outtargetangle	= activator->angle + diff;

				activator->z = outtargetz;

				fixed_t momx = activator->momx;
				fixed_t momy = activator->momy;

				Rotate( diff, momx, momy, activator->momx, activator->momy );

				return true;
			}

			break;
		}
	}

	return false;
}

DOOM_C_API int32_t EV_DoTeleportGeneric( line_t* line, mobj_t* activator )
{
	fixed_t oldx = activator->x;
	fixed_t oldy = activator->y;
	fixed_t oldz = activator->z;
	bool teleported = false;

	fixed_t targetx = 0;
	fixed_t targety = 0;
	fixed_t targetz = 0;
	angle_t targetangle = 0;

	switch( line->action->param1 & tt_targetmask )
	{
	case tt_tothing:
		teleported = EV_DoTeleportGenericThing( line, activator, (teleporttype_t)( line->action->param1 & tt_anglemask ), targetx, targety, targetz, targetangle );
		break;

	case tt_toline:
		teleported = EV_DoTeleportGenericLine( line, activator, (teleporttype_t)( line->action->param1 & tt_anglemask ), targetx, targety, targetz, targetangle );
		break;
	}

	if( teleported )
	{
		if( activator->player )
		{
			activator->player->viewz = activator->z + activator->player->viewheight;
		}

		if( ( line->action->param1 & tt_silent ) != tt_silent )
		{
			// spawn teleport fog at source and destination
			mobj_t* fog = P_SpawnMobj( oldx, oldy, oldz, MT_TFOG );
			S_StartSound( fog, sfx_telept );

			angle_t angle = targetangle >> ANGLETOFINESHIFT;
			fog = P_SpawnMobj( targetx + 20 * finecosine[angle], targety + 20 * finesine[angle], activator->z, MT_TFOG );

			// emit sound, where?
			S_StartSound( fog, sfx_telept );
		
			// don't move for a bit
			if( activator->player )
			{
				activator->reactiontime = 18;
			}

			activator->momx = activator->momy = activator->momz = 0;
		}

		activator->angle = targetangle;
		activator->teleporttic = gametic;
	}

	return teleported ? 1 : 0;
}

// =================
// MUSINFO and music lines
// =================

DOOM_C_API void T_MusInfo( musinfo_t* info )
{
	if( info->ticsleft > 0 && --info->ticsleft == 0 )
	{
		if( current_map->music_lump[ info->nexttrack ].val
			&& *current_map->music_lump[ info->nexttrack ].val != 0 )
		{
			S_ChangeMusicLump( &current_map->music_lump[ info->nexttrack ], true );
		}
		info->thinker.Disable();
	}
}

DOOM_C_API int32_t EV_DoMusicSwitchGeneric( line_t* line, mobj_t* activator )
{
	int32_t side = P_PointOnLineSide( activator->x, activator->y, line );
	if( ( line->action->param1 & mt_walk ) == mt_walk )
	{
		side = side ? 0 : 1;
	}

	int32_t looping = !!( line->action->param1 & mt_loop );

	if( side == 0 ) // Activating from front side
	{
		S_ChangeMusicLumpIndex( line->frontside->toptextureindex, looping );
	}
	else
	{
		S_ChangeMusicLumpIndex( line->frontside->bottomtextureindex, looping );
	}

	return 1;
}

// =================
//    SECTOR TINT
// =================

DOOM_C_API int32_t EV_DoSectorTintGeneric( line_t* line, mobj_t* activator )
{
	int32_t applied = 0;

	if( line->frontside )
	{
		int32_t side = P_PointOnLineSide( activator->x, activator->y, line );
		if( ( line->action->param1 & mt_walk ) == mt_walk )
		{
			side = side ? 0 : 1;
		}

		byte* colormap = nullptr;
		if( side == 0 ) // Activating from front side
		{
			colormap = ( !line->frontside->toptexture && line->frontside->toptextureindex >= 0 )
						? R_GetColormapForNum( line->frontside->toptextureindex )
						: nullptr;
		}
		else
		{
			colormap = ( !line->frontside->bottomtexture && line->frontside->bottomtextureindex >= 0 )
						? R_GetColormapForNum( line->frontside->bottomtextureindex )
						: nullptr;
		}

		for( sector_t& sector : Sectors() )
		{
			if( sector.tag == line->tag )
			{
				sector.colormap = colormap;
				++applied;
			}
		}
	}

	return applied;
}

// =================
//  EVERYTHING ELSE
// =================

DOOM_C_API int32_t EV_DoBoomFloorCeilingGeneric( line_t* line, mobj_t* activator )
{
	lineaction_t dummyaction = *line->action;
	line_t dummyline = *line;
	dummyline.action = &dummyaction;
	dummyaction.param1 = stt_lowestneighborfloor;
	dummyaction.param2 = sd_down;

	int32_t ceilingres = EV_DoCeilingGeneric( line, activator );

	if( !ceilingres )
	{
		return EV_DoFloorGeneric( &dummyline, activator );
	}

	return ceilingres;
}

DOOM_C_API int32_t EV_DoExitGeneric( line_t* line, mobj_t* activator )
{
	bool reset = ( line->action->param1 & exit_resetinventory ) == exit_resetinventory;
	( line->action->param1 & exit_secret ) ? G_SecretExitLevel( reset ) : G_ExitLevel( reset );

	return 1;
}

// =================
//  SECTOR ACTIONS
// =================

constexpr fixed_t FlatScrollScale = FixedDiv( 1, 32 );
constexpr fixed_t AccelScale = DoubleToFixed( 0.09375 );

INLINE void T_ScrollCeilingTexture( scroller_t* scroller )
{
	for( sector_t* sector : std::span( scroller->sectors, scroller->sectorcount ) )
	{
		sector->ceiloffsetx += scroller->scrollx;
		sector->ceiloffsety += scroller->scrolly;
		sector->lastactivetic = gametic;
	}
}

INLINE void T_ScrollFloorTexture( scroller_t* scroller )
{
	for( sector_t* sector : std::span( scroller->sectors, scroller->sectorcount ) )
	{
		sector->flooroffsetx += scroller->scrollx;
		sector->flooroffsety += scroller->scrolly;
		sector->lastactivetic = gametic;
	}
}

INLINE void T_ScrollWallTexture( scroller_t* scroller )
{
	fixed_t scrollx = scroller->scrollx;
	fixed_t scrolly = scroller->scrolly;
	angle_t boomhack = 0;
	// Why does Boom special case this
	if( scroller->controlline && scroller->controlline->special == Scroll_WallTextureBySector_Always )
	{
		boomhack = ANG180;
	}

	for( line_t* line : std::span( scroller->lines, scroller->linecount ) )
	{
		if( scroller->controlline )
		{
			angle_t diff = line->angle - scroller->controlline->angle - ANG90 + boomhack;
			Rotate( diff, scroller->scrollx, scroller->scrolly, scrollx, scrolly );
		}

		line->frontside->textureoffset += scrollx;
		line->frontside->rowoffset += scrolly;

		if( scroller->BothSides() && line->backside )
		{
			line->backside->textureoffset += scrollx;
			line->backside->rowoffset += scrolly;
		}
	}
}

INLINE void T_CarryObjects( scroller_t* scroller )
{
	for( sector_t* sector : std::span( scroller->sectors, scroller->sectorcount ) )
	{
		P_BlockThingsIteratorVertical( iota( sector->blockbox[ BOXLEFT ], sector->blockbox[ BOXRIGHT ] + 1 ),
								iota( sector->blockbox[ BOXBOTTOM ], sector->blockbox[ BOXTOP ] + 1 ),
								[ scroller, sector ]( mobj_t* mobj ) -> bool
		{
			if( mobj->flags & MF_NOCLIP )
			{
				return true;
			}

			fixed_t scrollheight = sector->FloorEffectHeight();

			bool cancarry = false;
			int32_t carryshift = 0;
			if( scroller->CarryType() & st_current )
			{
				cancarry |= ( mobj->z == scrollheight
								&& !( mobj->flags & MF_NOGRAVITY ) )
							|| mobj->z < scrollheight;
			}
			if( ( scroller->CarryType() & st_wind )
				&& ( sector->special & SectorWind_Mask ) == SectorWind_Yes )
			{
				cancarry |= mobj->z >= scrollheight;
				if( mobj->z > scrollheight ) carryshift = 1;
			}

			if( cancarry && P_MobjOverlapsSector( sector, mobj ) )
			{
				mobj->momx += FixedMul( ( scroller->scrollx >> carryshift ), AccelScale );
				mobj->momy += FixedMul( ( scroller->scrolly >> carryshift ), AccelScale );
			}

			return true;
		} );
	}
}

INLINE void T_PushObjects( scroller_t* scroller )
{
	for( mobj_t* point : std::span( scroller->controlpoints, scroller->pointcount ) )
	{
		if( ( point->subsector->sector->special & SectorWind_Mask ) != SectorWind_Yes )
		{
			continue;
		}

		P_BlockThingsIteratorVertical( point->x, point->y, scroller->mag,
									[ scroller, point ]( mobj_t* mobj ) -> bool
		{
			if( !( mobj->flags & MF_NOCLIP )
				&& P_CheckSight( point, mobj ) )
			{
				fixed_t dx = mobj->x - point->x;
				fixed_t dy = mobj->y - point->y;

				fixed_t dist = P_AproxDistance( dx, dy );
				if( dist < scroller->mag )
				{
					fixed_t force = FixedDiv( IntToFixed( 1 ), dist );
					if( point->type == MT_PULL )
					{
						force = -force;
					}
					mobj->momx += FixedMul( dx, force );
					mobj->momy += FixedMul( dy, force );
				}
			}

			return true;
		} );
	}
}

INLINE void T_UpdateDisplacement( scroller_t* scroller )
{
	sector_t*& control = scroller->controlsector;

	fixed_t height = control->ceilingheight - control->floorheight;
	fixed_t diff = scroller->controlheight - height;

	scroller->scrollx = FixedMul( scroller->magx, diff ) >> scroller->speedshift;
	scroller->scrolly = FixedMul( scroller->magy, diff ) >> scroller->speedshift;

	scroller->controlheight = height;
}

INLINE void T_UpdateAccelerative( scroller_t* scroller )
{
	sector_t*& control = scroller->controlsector;

	fixed_t height = control->ceilingheight - control->floorheight;
	fixed_t diff = scroller->controlheight - height;

	scroller->scrollx += FixedMul( scroller->magx, diff ) >> scroller->speedshift;
	scroller->scrolly += FixedMul( scroller->magy, diff ) >> scroller->speedshift;

	scroller->controlheight = height;
}

DOOM_C_API void T_ScrollerGeneric( scroller_t* scroller )
{
	switch( scroller->SpeedType() )
	{
	case st_displacement:
		T_UpdateDisplacement( scroller );
		break;

	case st_accelerative:
		T_UpdateAccelerative( scroller );
		break;

	default:
		break;
	}

	switch( scroller->ScrollType() )
	{
	case st_ceiling:
		T_ScrollCeilingTexture( scroller );
		T_ScrollWallTexture( scroller );
		break;

	case st_floor:
		T_ScrollFloorTexture( scroller );
		T_ScrollWallTexture( scroller );
		break;

	case st_wall:
		T_ScrollWallTexture( scroller );
		break;

	default:
		break;
	}

	if( scroller->CarryType() & st_nopointcarry )
	{
		T_CarryObjects( scroller );
	}
	else if( scroller->CarryType() & st_point )
	{
		T_PushObjects( scroller );
	}
}

constexpr scrolltype_t SelectScrollType( int32_t special )
{
	switch( special )
	{
	case Scroll_CeilingTexture_Displace_Always:
	case Scroll_CeilingTexture_Always:
	case Scroll_CeilingTexture_Accelerative_Always:
		return st_ceiling;

	case Scroll_FloorTexture_Displace_Always:
	case Scroll_FloorTexture_Always:
	case Scroll_FloorTexture_Accelerative_Always:
	case Scroll_FloorTextureObjects_Displace_Always:
	case Scroll_FloorTextureObjects_Always:
	case Scroll_FloorTextureObjects_Accelerative_Always:
		return st_floor;

	case Scroll_WallTextureLeft_Always:
	case Scroll_WallTextureRight_Always:
	case Scroll_WallTextureBySector_Accelerative_Always:
	case Scroll_WallTextureBySector_Displace_Always:
	case Scroll_WallTextureBySector_Always:
	case Scroll_WallTextureByOffset_Always:
	case Scroll_WallTextureBySectorDiv8_Always:
	case Scroll_WallTextureBySectorDiv8_Displacement_Always:
	case Scroll_WallTextureBySectorDiv8_Accelerative_Always:
	case Scroll_WallTextureBothSides_Left_Always:
	case Scroll_WallTextureBothSides_Right_Always:
	case Scroll_WallTextureBothSides_SectorDiv8_Always:
	case Scroll_WallTextureBothSides_SectorDiv8Displacement_Always:
	case Scroll_WallTextureBothSides_SectorDiv8Accelerative_Always:
		return st_wall;

	default:
		break;
	}

	return st_none;
}

constexpr scrolltype_t SelectCarryType( int32_t special )
{
	switch( special )
	{
	case Scroll_FloorObjects_Displace_Always:
	case Scroll_FloorObjects_Always:
	case Scroll_FloorObjects_Accelerative_Always:
	case Scroll_FloorTextureObjects_Displace_Always:
	case Scroll_FloorTextureObjects_Always:
	case Scroll_FloorTextureObjects_Accelerative_Always:
	case Transfer_CurrentByLength_Always:
		return st_current;

	case Transfer_WindByLength_Always:
		return st_wind;

	case Transfer_WindOrCurrentByPoint_Always:
		return st_point;
		break;

	default:
		break;
	}

	return st_none;
}

constexpr scrolltype_t SelectSpeedType( int32_t special )
{
	switch( special )
	{
	case Scroll_CeilingTexture_Displace_Always:
	case Scroll_FloorTexture_Displace_Always:
	case Scroll_FloorObjects_Displace_Always:
	case Scroll_FloorTextureObjects_Displace_Always:
	case Scroll_WallTextureBySector_Displace_Always:
	case Scroll_WallTextureBySectorDiv8_Displacement_Always:
	case Scroll_WallTextureBothSides_SectorDiv8Accelerative_Always:
		return st_displacement;

	case Scroll_CeilingTexture_Accelerative_Always:
	case Scroll_FloorTexture_Accelerative_Always:
	case Scroll_FloorObjects_Accelerative_Always:
	case Scroll_FloorTextureObjects_Accelerative_Always:
	case Scroll_WallTextureBySector_Accelerative_Always:
	case Scroll_WallTextureBySectorDiv8_Accelerative_Always:
	case Scroll_WallTextureBothSides_SectorDiv8Displacement_Always:
		return st_accelerative;

	default:
		break;
	}

	return st_none;
}

constexpr scrolltype_t SelectScroller( int32_t special )
{
	return SelectScrollType( special )
		| SelectCarryType( special )
		| SelectSpeedType( special );
}

constexpr int32_t ScrollerSpeedShift( int32_t special )
{
	switch( special )
	{
	case Scroll_WallTextureBySectorDiv8_Always:
	case Scroll_WallTextureBySectorDiv8_Displacement_Always:
	case Scroll_WallTextureBySectorDiv8_Accelerative_Always:
	case Scroll_WallTextureBothSides_SectorDiv8_Always:
	case Scroll_WallTextureBothSides_SectorDiv8Displacement_Always:
	case Scroll_WallTextureBothSides_SectorDiv8Accelerative_Always:
		return 3;

	default:
		break;
	}

	return 0;
}

DOOM_C_API int32_t P_SpawnSectorScroller( line_t* line )
{
	scrolltype_t type = SelectScroller( line->special );
	if( type == st_none )
	{
		return 0;
	}

	int32_t createdcount = 0;
	if( type & st_wall )
	{
		for( line_t& wall : Lines() )
		{
			if( wall.tag == line->tag )
			{
				++createdcount;
			}
		}
	}
	else
	{
		for( sector_t& sector : Sectors() )
		{
			if( sector.tag == line->tag )
			{
				++createdcount;
			}
		}
	}

	if( createdcount > 0 )
	{
		scroller_t* scroller = (scroller_t*)Z_MallocZero( sizeof(scroller_t), PU_LEVSPEC, 0 );
		P_AddThinker( &scroller->thinker );
		scroller->thinker.function = &T_ScrollerGeneric;
		scroller->type				= type;
		scroller->magx				= FixedMul( line->dx, FlatScrollScale );
		scroller->magy				= FixedMul( line->dy, FlatScrollScale );
		scroller->controlline		= line;
		scroller->speedshift		= ScrollerSpeedShift( line->special );

		if( ( scroller->CarryType() & st_point ) != st_none )
		{
			scroller->mag = P_AproxDistance( line->dx, line->dy );

			std::vector< mobj_t* > points;
			for( sector_t& sector : Sectors() )
			{
				if( sector.tag == line->tag )
				{
					for( mobj_t* mobj : NoSectorMobjs( sector ) )
					{
						if( mobj->type == MT_PUSH || mobj->type == MT_PULL )
						{
							points.push_back( mobj );
						}
					}
				}
			}

			createdcount = (int32_t)points.size();
			if( createdcount > 0 )
			{
				size_t size = sizeof( mobj_t* ) * points.size();
				scroller->controlpoints = (mobj_t**)Z_Malloc( size, PU_LEVSPEC, nullptr );
				memcpy( scroller->controlpoints, points.data(), size );
				scroller->pointcount = (int32_t)points.size();
			}

		}
		else
		{
			if( ( scroller->SpeedType() & ( st_accelerative | st_displacement ) ) != st_none )
			{
				if( line->frontsector == nullptr )
				{
					I_Error( "P_SpawnSectorScroller: non-static scroller line %d has no front sector", line->index );
				}
				scroller->controlsector		= line->frontsector;
				scroller->controlheight		= line->frontsector->ceilingheight - line->frontsector->floorheight;
				scroller->scrollx			= 0;
				scroller->scrolly			= 0;
			}
			else
			{
				scroller->scrollx			= FixedMul( line->dx, FlatScrollScale ) >> scroller->speedshift;
				scroller->scrolly			= FixedMul( line->dy, FlatScrollScale ) >> scroller->speedshift;
			}

			if( type & st_wall )
			{
				scroller->mag = FixedMul( P_AproxDistance( line->dx, line->dy ), IntToFixed( 2 ) );
				scroller->linecount = createdcount;
				scroller->lines = (line_t**)Z_Malloc( sizeof( line_t* ) * createdcount, PU_LEVSPEC, nullptr );
				createdcount = 0;
				for( line_t& wall : Lines() )
				{
					if( wall.tag == line->tag )
					{
						scroller->lines[ createdcount++ ] = &wall;
					}
				}
			}
			else
			{
				scroller->sectorcount = createdcount;
				scroller->sectors = (sector_t**)Z_Malloc( sizeof( sector_t* ) * createdcount, PU_LEVSPEC, nullptr );

				createdcount = 0;
				for( sector_t& sector : Sectors() )
				{
					if( sector.tag == line->tag )
					{
						scroller->sectors[ createdcount++ ] = &sector;
					}
				}
			}
		}
	}

	return createdcount;
}

DOOM_C_API int32_t P_SpawnLineScrollers( int32_t left, int32_t leftboth, int32_t right, int32_t rightboth, int32_t offset )
{
	scroller_t* leftscroller = nullptr;
	scroller_t* leftbothscroller = nullptr;
	scroller_t* rightscroller = nullptr;
	scroller_t* rightbothscroller = nullptr;
	
	if( left > 0 )
	{
		leftscroller = (scroller_t*)Z_MallocZero( sizeof(scroller_t), PU_LEVSPEC, 0 );
		P_AddThinker( &leftscroller->thinker );
		leftscroller->thinker.function = &T_ScrollerGeneric;
		leftscroller->type = st_wall;
		leftscroller->scrollx = FRACUNIT;
		leftscroller->linecount = left;
		leftscroller->lines = (line_t**)Z_Malloc( sizeof( line_t* ) * left, PU_LEVSPEC, nullptr );
	}

	if( leftboth > 0 )
	{
		leftbothscroller = (scroller_t*)Z_MallocZero( sizeof(scroller_t), PU_LEVSPEC, 0 );
		P_AddThinker( &leftbothscroller->thinker );
		leftbothscroller->thinker.function = &T_ScrollerGeneric;
		leftbothscroller->type = st_wall | st_bothsides;
		leftbothscroller->scrollx = FRACUNIT;
		leftbothscroller->linecount = leftboth;
		leftbothscroller->lines = (line_t**)Z_Malloc( sizeof( line_t* ) * leftboth, PU_LEVSPEC, nullptr );
	}

	if( right > 0 )
	{
		rightscroller = (scroller_t*)Z_MallocZero( sizeof(scroller_t), PU_LEVSPEC, 0 );
		P_AddThinker( &rightscroller->thinker );
		rightscroller->thinker.function = &T_ScrollerGeneric;
		rightscroller->type = st_wall;
		rightscroller->scrollx = -FRACUNIT;
		rightscroller->linecount = right;
		rightscroller->lines = (line_t**)Z_Malloc( sizeof( line_t* ) * right, PU_LEVSPEC, nullptr );
	}

	int32_t leftcount = 0;
	int32_t leftbothcount = 0;
	int32_t rightcount = 0;
	int32_t rightbothcount = 0;

	for( line_t& line : Lines() )
	{
		switch( line.special )
		{
		case Scroll_WallTextureLeft_Always:
			leftscroller->lines[ leftcount++ ] = &line;
			break;
			
		case Scroll_WallTextureBothSides_Left_Always:
			leftbothscroller->lines[ leftbothcount++ ] = &line;
			break;

		case Scroll_WallTextureRight_Always:
			rightscroller->lines[ rightcount++ ] = &line;
			break;

		case Scroll_WallTextureBothSides_Right_Always:
			rightbothscroller->lines[ rightbothcount++ ] = &line;
			break;

		case Scroll_WallTextureByOffset_Always:
			{
				scroller_t* offsetscroller = (scroller_t*)Z_MallocZero( sizeof(scroller_t), PU_LEVSPEC, 0 );
				P_AddThinker( &offsetscroller->thinker );
				offsetscroller->thinker.function = &T_ScrollerGeneric;
				offsetscroller->type = st_wall;
				offsetscroller->scrollx = -line.frontside->textureoffset;
				offsetscroller->scrolly = line.frontside->rowoffset;
				offsetscroller->linecount = 1;
				offsetscroller->lines = (line_t**)Z_Malloc( sizeof( line_t* ), PU_LEVSPEC, nullptr );
				offsetscroller->lines[ 0 ] = &line;
			}
			break;
		}
	}

	return 1;
}

void SetupSectorOffset( line_t& line )
{
	auto ScrollFloor = []( line_t& line, sector_t& sector )
	{
		sector.flooroffsetx += line.dx;
		sector.flooroffsety += line.dy;
	};

	auto ScrollCeiling = []( line_t& line, sector_t& sector )
	{
		sector.ceiloffsetx += line.dx;
		sector.ceiloffsety += line.dy;
	};

	auto ScrollBoth = [&ScrollFloor, &ScrollCeiling]( line_t& line, sector_t& sector )
	{
		ScrollFloor( line, sector );
		ScrollCeiling( line, sector );
	};

	auto RotateFloor = []( line_t& line, sector_t& sector )
	{
		sector.floorrotation += line.angle;
	};

	auto RotateCeiling = []( line_t& line, sector_t& sector )
	{
		sector.ceilrotation += line.angle;
	};

	auto RotateBoth = [&RotateFloor, &RotateCeiling]( line_t& line, sector_t& sector )
	{
		RotateFloor( line, sector );
		RotateCeiling( line, sector );
	};

	auto EmptyFunctor = []( line_t& line, sector_t& sector ) { };
 
	using functor_t = std::function< void( line_t&, sector_t& ) >;

	functor_t offsetfunctor;
	functor_t rotatefunctor;

	switch( line.special )
	{
	case Offset_FloorTexture_Always:
		offsetfunctor = ScrollFloor;
		rotatefunctor = EmptyFunctor;
		break;

	case Offset_CeilingTexture_Always:
		offsetfunctor = ScrollCeiling;
		rotatefunctor = EmptyFunctor;
		break;

	case Offset_FloorCeilingTexture_Always:
		offsetfunctor = ScrollBoth;
		rotatefunctor = EmptyFunctor;
		break;

	case Rotate_FloorTexture_Always:
		offsetfunctor = EmptyFunctor;
		rotatefunctor = RotateFloor;
		break;

	case Rotate_CeilingTexture_Always:
		offsetfunctor = EmptyFunctor;
		rotatefunctor = RotateCeiling;
		break;

	case Rotate_FloorCeilingTexture_Always:
		offsetfunctor = EmptyFunctor;
		rotatefunctor = RotateBoth;
		break;

	case OffsetRotate_FloorTexture_Always:
		offsetfunctor = ScrollFloor;
		rotatefunctor = RotateFloor;
		break;

	case OffsetRotate_CeilingTexture_Always:
		offsetfunctor = ScrollCeiling;
		rotatefunctor = RotateCeiling;
		break;

	case OffsetRotate_FloorCeilingTexture_Always:
		offsetfunctor = ScrollBoth;
		rotatefunctor = RotateBoth;
		break;

	default:
		break;
	}

	for( sector_t& sector : Sectors() )
	{
		if( sector.tag == line.tag )
		{
			offsetfunctor( line, sector );
			rotatefunctor( line, sector );
			prevsectors[ sector.index ].flooroffsetx	= currsectors[ sector.index ].flooroffsetx		= FixedToRendFixed( sector.flooroffsetx );
			prevsectors[ sector.index ].flooroffsety	= currsectors[ sector.index ].flooroffsety		= FixedToRendFixed( sector.flooroffsety );
			prevsectors[ sector.index ].ceiloffsetx		= currsectors[ sector.index ].ceiloffsetx		= FixedToRendFixed( sector.ceiloffsetx );
			prevsectors[ sector.index ].ceiloffsety		= currsectors[ sector.index ].ceiloffsety		= FixedToRendFixed( sector.ceiloffsety );
			prevsectors[ sector.index ].floorrotation	= currsectors[ sector.index ].floorrotation		= sector.floorrotation;
			prevsectors[ sector.index ].ceilrotation	= currsectors[ sector.index ].ceilrotation		= sector.ceilrotation;
		}
	}
}

DOOM_C_API doombool P_SpawnSectorSpecialsGeneric()
{
	if( !sim.generic_specials_handling )
	{
		return false;
	}

	activeceilingshead = nullptr;
	activeplatshead = nullptr;

	bool extendedspecialsectors = sim.boom_sector_specials || sim.mbf21_sector_specials;

	for( sector_t& sector : Sectors() )
	{
		bool doextended = extendedspecialsectors && ( sector.special & ~DSS_Mask ) != 0;
		if( doextended && ( sector.special & SectorSecret_Mask ) == SectorSecret_Yes )
		{
			totalsecret++;
			++session.start_total_secrets;
		}

		switch( sector.special & DSS_Mask )
		{
		case DSS_LightRandom:
			P_SpawnLightFlash( &sector );
			sector.special &= ~DSS_Mask;
			break;

		case DSS_LightBlinkHalfSecond:
			P_SpawnStrobeFlash( &sector, FASTDARK, 0 );
			sector.special &= ~DSS_Mask;
			break;

		case DSS_LightBlinkSecond:
			P_SpawnStrobeFlash( &sector, SLOWDARK, 0 );
			sector.special &= ~DSS_Mask;
			break;

		case DSS_20DamageAndLightBlink:
			P_SpawnStrobeFlash( &sector, FASTDARK, 0 );
			break;

		case DSS_LightOscillate:
			P_SpawnGlowingLight( &sector );
			sector.special &= ~DSS_Mask;
			break;

		case DSS_Secret:
			totalsecret++;
			++session.start_total_secrets;
			sector.special &= ~DSS_Mask;
			sector.special |= SectorSecret_Yes;
			break;

		case DSS_30SecondsClose:
			if( !doextended )
			{
				EV_DoDelayedDoorGeneric( &sector, sd_close, 30 * TICRATE, 0, IntToFixed( 2 ) );
				sector.special &= ~DSS_Mask;
			}
			break;

		case DSS_LightBlinkHalfSecondSynchronised:
			P_SpawnStrobeFlash( &sector, SLOWDARK, 1 );
			sector.special &= ~DSS_Mask;
			break;

		case DSS_LightBlinkSecondSynchronised:
			P_SpawnStrobeFlash( &sector, FASTDARK, 1 );
			sector.special &= ~DSS_Mask;
			break;

		case DSS_300SecondsOpen:
			if( !doextended )
			{
				EV_DoDelayedDoorGeneric( &sector, sd_open, 300 * TICRATE, VDOORWAIT, IntToFixed( 2 ) );
				sector.special &= ~DSS_Mask;
			}
			break;

		case DSS_LightFlicker:
			P_SpawnFireFlicker( &sector );
			sector.special &= ~DSS_Mask;
			break;

		case DSS_None:
		case DSS_10Damage:
		case DSS_5Damage:
		case DSS_20DamageAndEnd:
		case DSS_20Damage:
			break;

		case DSS_Unused1:
		case DSS_Unused2:
		default:
			I_LogAddEntryVar( Log_Error, "P_SpawnSectorSpecialsGeneric: Sector %d has unknown special type %d", sector.index, sector.special );
			sector.special &= ~DSS_Mask;
			break;
		}
	}

	int32_t leftcount = 0;
	int32_t leftbothcount = 0;
	int32_t rightcount = 0;
	int32_t rightbothcount = 0;
	int32_t offsetcount = 0;

	std::vector< sector_t* > transfertargets;
	transfertargets.reserve( numsectors );

	for( line_t& line : Lines() )
	{
		switch( line.special )
		{
		case Scroll_WallTextureLeft_Always:
			++leftcount;
			break;

		default:
			break;
		}

		if( sim.boom_line_specials )
		{
			switch( line.special )
			{
			case Scroll_CeilingTexture_Accelerative_Always:
			case Scroll_FloorTexture_Accelerative_Always:
			case Scroll_FloorObjects_Accelerative_Always:
			case Scroll_FloorTextureObjects_Accelerative_Always:
			case Scroll_WallTextureBySector_Accelerative_Always:
			case Transfer_WindByLength_Always:
			case Transfer_CurrentByLength_Always:
			case Transfer_WindOrCurrentByPoint_Always:
			case Scroll_CeilingTexture_Displace_Always:
			case Scroll_FloorTexture_Displace_Always:
			case Scroll_FloorObjects_Displace_Always:
			case Scroll_FloorTextureObjects_Displace_Always:
			case Scroll_WallTextureBySector_Displace_Always:
			case Scroll_CeilingTexture_Always:
			case Scroll_FloorTexture_Always:
			case Scroll_FloorObjects_Always:
			case Scroll_FloorTextureObjects_Always:
			case Scroll_WallTextureBySector_Always:
				P_SpawnSectorScroller( &line );
				break;

			case Scroll_WallTextureRight_Always:
				++rightcount;
				break;

			case Scroll_WallTextureByOffset_Always:
				++offsetcount;
				break;

			case Transfer_FloorLighting_Always:
			case Transfer_CeilingLighting_Always:
				if( line.frontsector )
				{
					doombool setceil = line.special == Transfer_CeilingLighting_Always;
					for( sector_t& sector : Sectors() )
					{
						if( sector.tag == line.tag )
						{
							sector_t** toset = setceil ? &sector.ceilinglightsec : &sector.floorlightsec;
							int32_t* tosetprev = setceil ? &prevsectors[ sector.index ].ceillightlevel
														: &prevsectors[ sector.index ].floorlightlevel;
							int32_t* tosetcurr = setceil ? &currsectors[ sector.index ].ceillightlevel
														: &currsectors[ sector.index ].floorlightlevel;

							*toset = line.frontsector;
							*tosetprev = *tosetcurr = line.frontsector->lightlevel;
						}
					}
				}
				break;

			case Transfer_Friction_Always: // Friction
				{
					fixed_t linelength = M_CLAMP( P_AproxDistance( line.dx, line.dy ), 0, IntToFixed( 200 ) );
					fixed_t frictionpercent = FixedDiv( linelength, IntToFixed( 100 ) );
					fixed_t frictionmul = frictionpercent - IntToFixed( 1 );
					fixed_t targetfriction = FRICTION + FixedMul( ( IntToFixed( 1 ) - FRICTION ), frictionmul );

					for( sector_t& sector : Sectors() )
					{
						if( sector.tag == line.tag )
						{
							sector.friction = targetfriction;
							sector.frictionpercent = frictionpercent;
						}
					}
				}
				break;

			case Transfer_Properties_Always:
				if( line.frontside )
				{
					side_t* side = line.frontside;
					line.topcolormap = ( !side->toptexture && side->toptextureindex >= 0 )
										? R_GetColormapForNum( side->toptextureindex )
										: colormaps;
					line.bottomcolormap = ( !side->bottomtexture && side->bottomtextureindex >= 0 )
										? R_GetColormapForNum( side->bottomtextureindex )
										: colormaps;
					line.midcolormap = ( !side->midtexture && side->midtextureindex >= 0 )
										? R_GetColormapForNum( side->midtextureindex )
										: colormaps;

					for( sector_t& sector : Sectors() )
					{
						if( sector.tag == line.tag )
						{
							sector.transferline = &line;
							transfertargets.push_back( &sector );
						}
					}
				}
				break;

			case Texture_Translucent_Always: // Transparency
				if( line.tag == 0 )
				{
					line.transparencymap = tranmap;
				}
				else
				{
					byte* customtranmap = tranmap;
					if( line.sidenum[ 0 ] >= 0 && sides[ line.sidenum[ 0 ] ].midtextureindex >= 0 )
					{
						lumpindex_t customtranmapindex = sides[ line.sidenum[ 0 ] ].midtextureindex;
						size_t customtranmaplen = W_LumpLength( customtranmapindex );
						if( customtranmaplen == 65536 )
						{
							customtranmap = (byte*)W_CacheLumpNum( customtranmapindex, PU_LEVEL );
						}
						else if( sides[ line.sidenum[ 0 ] ].midtexture == 0 )
						{
							I_Error( "Tagged transparent line requires either a valid transmap entry or a valid texture" );
						}
					}

					for( int32_t target = 0 ; target < numlines; ++target )
					{
						if( lines[ target ].tag == line.tag )
						{
							lines[ target ].transparencymap = customtranmap;
						}
					}
				}
				break;
			}
		}

		if( sim.mbf_line_specials )
		{
			switch( line.special )
			{
			case Transfer_Sky_Always:
			case Transfer_SkyReversed_Always:
				if( line.frontside != nullptr )
				{
					prevsides[ line.frontside->index ].sky
						= currsides[ line.frontside->index ].sky
						= line.frontside->toptexturesky
							= R_GetSky( R_TextureNameForNum( line.frontside->toptexture ), true );

					fixed_t scale = IntToFixed( line.special == Transfer_Sky_Always ? -1 : 1 );
					for( sector_t& sector : Sectors() )
					{
						if( sector.tag == line.tag )
						{
							prevsectors[ sector.index ].skyline
								= currsectors[ sector.index ].skyline
								= sector.skyline
									= &sides[ line.sidenum[ 0 ] ];

							sector.skyxscale = scale;
						}
					}
				}
				break;
			}
		}

		if( sim.mbf21_line_specials )
		{
			switch( line.special )
			{
			case Scroll_WallTextureBySectorDiv8_Always:
			case Scroll_WallTextureBySectorDiv8_Displacement_Always:
			case Scroll_WallTextureBySectorDiv8_Accelerative_Always:
				P_SpawnSectorScroller( &line );
				break;

			default:
				break;
			}
		}

		if( sim.rnr24_line_specials )
		{
			switch( line.special )
			{
			case Offset_FloorTexture_Always:
			case Offset_CeilingTexture_Always:
			case Offset_FloorCeilingTexture_Always:
			case Rotate_FloorTexture_Always:
			case Rotate_CeilingTexture_Always:
			case Rotate_FloorCeilingTexture_Always:
			case OffsetRotate_FloorTexture_Always:
			case OffsetRotate_CeilingTexture_Always:
			case OffsetRotate_FloorCeilingTexture_Always:
				SetupSectorOffset( line );
				break;

			case Tint_SetTo_Always:
				if( line.frontside )
				{
					byte* colormap = ( !line.frontside->toptexture && line.frontside->toptextureindex >= 0 )
									? R_GetColormapForNum( line.frontside->toptextureindex )
									: nullptr;

					for( sector_t& sector : Sectors() )
					{
						if( sector.tag == line.tag )
						{
							sector.colormap = colormap;
						}
					}
				}
				break;

			case Scroll_WallTextureBothSides_Left_Always:
				++leftbothcount;
				break;

			case Scroll_WallTextureBothSides_Right_Always:
				++rightbothcount;
				break;

			case Scroll_WallTextureBothSides_SectorDiv8_Always:
			case Scroll_WallTextureBothSides_SectorDiv8Displacement_Always:
			case Scroll_WallTextureBothSides_SectorDiv8Accelerative_Always:
				P_SpawnSectorScroller( &line );
				break;
			}
		}
	}

	if( !transfertargets.empty() )
	{
		size_t size = sizeof( sector_t*) * transfertargets.size();
		transfertargetsectors = (sector_t**)Z_Malloc( size, PU_LEVSPEC, nullptr );
		memcpy( transfertargetsectors, transfertargets.data(), size );
		numtransfertargetsectors = (int32_t)transfertargets.size();
	}

	if( leftcount > 0 || leftbothcount > 0 || rightcount > 0 || rightbothcount > 0 || offsetcount > 0 )
	{
		P_SpawnLineScrollers( leftcount, leftbothcount, rightcount, rightbothcount, offsetcount );
	}

	return true;
}

DOOM_C_API void P_MobjInSectorGeneric( mobj_t* mobj )
{
	sector_t*&	sector = mobj->subsector->sector;
	int16_t&	special = sector->special;
	player_t*&	player = mobj->player;

	if( mobj->z > sector->FloorEffectHeight() )
	{
		return;
	}

	bool candamage = !( leveltime & 0x1f );

	if( player )
	{
		if( ( special & ~DSS_Mask ) == 0
			&& !player->powers[pw_ironfeet] )
		{
			switch( special & DSS_Mask )
			{
			case DSS_5Damage:
				if( candamage )
				{
					P_DamageMobj( mobj, NULL, NULL, 5, damage_none );
				}
				break;

			case DSS_10Damage:
				if( candamage )
				{
					P_DamageMobj( mobj, NULL, NULL, 10, damage_none );
				}
				break;

			case DSS_20Damage:
			case DSS_20DamageAndLightBlink:
				if( candamage )
				{
					P_DamageMobj( mobj, NULL, NULL, 20, damage_none );
				}
				break;

			case DSS_20DamageAndEnd:
				if( !comp.god_mode_absolute )
				{
					player->cheats &= ~CF_GODMODE;
				}

				if( candamage )
				{
					P_DamageMobj( mobj, NULL, NULL, 20, damage_none );
				}

				if( player->health <= 10 )
				{
					G_ExitLevel( false );
				}
			default:
				break;
			}
		}

		if( sector->secretstate != Secret_Discovered && ( special & SectorSecret_Mask ) == SectorSecret_Yes )
		{
			player->secretcount++;
			sector->special &= ~SectorSecret_Mask;
			sector->secretstate = Secret_Discovered;
			++session.total_found_secrets_global;
			++session.total_found_secrets[ player - players ];
		}
	}

	if( player && sim.boom_sector_specials )
	{
		bool isboomdamage = !sim.mbf21_sector_specials
						|| ( special & SectorAltDamage_Mask ) == SectorAltDamage_No;
		if( candamage
			&& isboomdamage 
			&& player != nullptr
			&& !player->powers[pw_ironfeet] )
		{
			switch( special & SectorDamage_Mask )
			{
			case SectorDamage_5:
				P_DamageMobj( mobj, NULL, NULL, 5, damage_none );
				break;
			case SectorDamage_10:
				P_DamageMobj( mobj, NULL, NULL, 10, damage_none );
				break;
			case SectorDamage_20:
				P_DamageMobj( mobj, NULL, NULL, 20, damage_none );
				break;
			default:
				break;
			}
		}
	}

	if( sim.mbf21_sector_specials )
	{
		bool isaltdamage = ( special & SectorAltDamage_Mask ) == SectorAltDamage_Yes;
		if( isaltdamage
			&& player != nullptr )
		{
			switch( special & SectorDamage_Mask )
			{
			case SectorAltDamage_KillPlayerWithoutRadsuit:
				if( ( !player->powers[pw_ironfeet] || !player->powers[pw_invulnerability] ) )
				{
					P_DamageMobj( mobj, nullptr, nullptr, 10000, damage_theworks );
				}
				break;

			case SectorAltDamage_KillPlayer:
				P_DamageMobj( mobj, nullptr, nullptr, 10000, damage_theworks );
				break;

			case SectorAltDamage_KillPlayersAndExit:
				for( player_t& currplayer : std::span( players ) )
				{
					P_DamageMobj( currplayer.mo, nullptr, nullptr, 10000, damage_theworks );
				}
				G_ExitLevel( false );
				break;

			case SectorAltDamage_KillPlayersAndSecretExit:
				for( player_t& currplayer : std::span( players ) )
				{
					P_DamageMobj( currplayer.mo, nullptr, nullptr, 10000, damage_theworks );
				}
				G_SecretExitLevel( false );
				break;

			default:
				break;
			}
		}

		if( candamage
			&& player == nullptr
			&& ( special & SectorKillGroundEnemies_Mask ) == SectorKillGroundEnemies_Yes
			&& !( mobj->flags & MF_NOGRAVITY ) )
		{
			P_DamageMobj( mobj, nullptr, nullptr, 10000, damage_theworks );
		}
	}
}

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
// DESCRIPTION:
//	All the new generic action handlers
//

#include "doomdef.h"
#include "doomstat.h"

#include "g_game.h"

#include "i_log.h"

#include "m_bbox.h"
#include "m_random.h"

#include "p_local.h"
#include "p_lineaction.h"
#include "p_spec.h"

#include "s_sound.h"
#include "sounds.h"

extern "C"
{
	ceiling_t*	activeceilingshead = nullptr;
	plat_t*		activeplatshead = nullptr;
}

#pragma optimize( "", off )

fixed_t P_FindShortestLowerTexture( sector_t* sector )
{
	fixed_t lowestheight = INT_MAX;
	int32_t mintextureindex = remove_limits ? 1 : 0; // fix_shortest_lower_texture_line

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

int32_t PerformOperationOnSectors( line_t* line, mobj_t* activator
								, std::function< bool( sector_t& ) >&& precon
								, std::function< int32_t( line_t*, mobj_t*, sector_t& ) >&& createfunc )
{
	int32_t createdcount = 0;
	if( line->action->AnimatedActivationType() == LT_Use )
	{
		if( line->backsector == nullptr )
		{
			I_Error("EV_DoDoorGeneric: Dx special type on 1-sided linedef");
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
			currceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeilingGeneric;
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
			currceil->thinker.function.acv = nullptr;
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
		ceiling->thinker.function.acp1 = (actionf_p1)&T_MoveCeilingGeneric;
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
		ceiling->thinker.function.acp1 = (actionf_p1)&T_MoveCeilingGeneric;
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
			int16_t& targetspecial = target->direction == sd_down ? target->newspecial : sourcesector.special;
			int16_t& targettexture = target->direction == sd_down ? target->newtexture : sourcesector.ceilingpic;

			targetspecial = type == sct_zerospecial
								? 0
								: type == sct_copyboth
									? setfrom.special
									: -1;
			targettexture = ( type == sct_copytexture || type == sct_copyboth )
								? setfrom.ceilingpic
								: -1;
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
							SetSpecial( sector, ceiling, (sectorchangetype_t)secline->action->param4, setfrom );
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
	result_e	res;
	
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
				for( int32_t secnum = 0; secnum < numsectors; ++secnum )
				{
					if( sectors[ secnum ].tag == door->lighttag )
					{
						sectors[ secnum ].lightlevel =  door->lightmin;
					}
				}
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
		break;
	
	case sd_open:
		// UP
		res = T_MovePlane( door->sector, door->speed, door->topheight, false, 1, door->direction );
	
		if (res == pastdest)
		{
			door->direction = sd_none;
			if( door->lighttag > 0 )
			{
				for( int32_t secnum = 0; secnum < numsectors; ++secnum )
				{
					if( sectors[ secnum ].tag == door->lighttag )
					{
						sectors[ secnum ].lightlevel =  door->lightmax;
					}
				}
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
		door->thinker.function.acp1 = (actionf_p1)T_VerticalDoorGeneric;
		door->topwait = line->action->delay;
		door->topcountdown = door->topwait;
		door->speed = line->action->speed;
		door->direction = (sectordir_t)line->action->param1;
		door->nextdirection = sd_none;
		door->blazing = door->speed >= (VDOORSPEED * 4);
		door->keepclosingoncrush = ( door->direction == sd_close && door->topwait == 0 );
		door->dontrecloseoncrush = ( door->direction == sd_close && door->topwait != 0 );
		door->lighttag = 0;

		if( remove_limits ) // allow_boom_specials
		{
			if( line->action->AnimatedActivationType() == LT_Use
				&& line->tag != 0
				&& line->frontsector != nullptr
				&& line->backsector != nullptr )
			{
				door->lighttag = line->tag;
				door->lightmax = M_MAX( line->frontsector->lightlevel, line->backsector->lightlevel );
				door->lightmin = M_MIN( line->frontsector->lightlevel, line->backsector->lightlevel );
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
		elevator->thinker.function.acp1 = (actionf_p1)&T_MoveElevatorGeneric;
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
		floormove_t* floor = (floormove_t*)Z_MallocZero( sizeof(floormove_t), PU_LEVSPEC, 0 );
		P_AddThinker( &floor->thinker );
		floor->sector = &sector;
		floor->sector->FloorSpecial() = floor;
		floor->thinker.function.acp1 = (actionf_p1)&T_MoveFloorGeneric;
		floor->type = genericFloor;
		floor->speed = line->action->speed;
		floor->crush = line->action->param3;
		floor->newspecial = -1;
		floor->texture = -1;

		fixed_t& stairsize = line->action->param1;
		fixed_t height = sector.floorheight + stairsize;
		floor->floordestheight = height;
		floor->direction = stairsize >= 0 ? sd_up : sd_down;

		int32_t createdcount = 1;

		sector_t* currsector = &sector;
		while( currsector != nullptr )
		{
			sector_t* searchsector = currsector;
			currsector = nullptr;

			for( line_t* searchline : Lines( *searchsector ) )
			{
				if( ( searchline->flags & ML_TWOSIDED ) != ML_TWOSIDED
					|| searchline->frontsector->index != searchsector->index
					|| ( line->action->param2 == false && searchline->backsector->floorpic != searchsector->floorpic )
					|| searchline->backsector->FloorSpecial() != nullptr )
				{
					continue;
				}

				++createdcount;
				currsector = searchline->backsector;
				height += stairsize;

				floormove_t* nextfloor = (floormove_t*)Z_MallocZero( sizeof(floormove_t), PU_LEVSPEC, 0 );
				P_AddThinker( &nextfloor->thinker );
				nextfloor->sector = searchline->backsector;
				nextfloor->sector->FloorSpecial() = nextfloor;
				nextfloor->thinker.function.acp1 = (actionf_p1)&T_MoveFloorGeneric;
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
			|| ( remove_limits && donutsector->FloorSpecial() != nullptr ) ) // fix_donut_multiple_sector_thinkers
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
			donut->thinker.function.acp1 = (actionf_p1)&T_MoveFloorGeneric;
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
			hole->thinker.function.acp1 = (actionf_p1)&T_MoveFloorGeneric;
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
		floor->thinker.function.acp1 = (actionf_p1)&T_MoveFloorGeneric;
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
			if( remove_limits ) // allow_boom_sector_targets
			{
				floor->floordestheight = M_MAX( floor->floordestheight, sector.ceilingheight );
			}
			break;

		case stt_lowestneighborceiling:
			floor->floordestheight = P_FindLowestCeilingSurrounding( &sector );
			if( remove_limits ) // allow_boom_sector_targets
			{
				floor->floordestheight = M_MIN( floor->floordestheight, sector.ceilingheight );
			}
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
			int16_t& targetspecial = target->direction == sd_down ? target->newspecial : sourcesector.special;
			int16_t& targettexture = target->direction == sd_down ? target->texture : sourcesector.floorpic;

			targetspecial = type == sct_zerospecial
								? 0
								: type == sct_copyboth
									? setfrom.special
									: -1;
			targettexture = ( type == sct_copytexture || type == sct_copyboth )
								? setfrom.floorpic
								: -1;
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
							SetSpecial( sector, floor, (sectorchangetype_t)secline->action->param4, setfrom );
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
			currplat->thinker.function.acp1 = (actionf_p1)T_RaisePlatGeneric;
		}
	}
}

DOOM_C_API void T_RaisePlatGeneric( plat_t* plat )
{
	// BIG BIG TODO HERE
	T_PlatRaise( plat );
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
		plat->thinker.function.acp1 = (actionf_p1)T_RaisePlatGeneric;
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
		plat->thinker.function.acp1 = (actionf_p1)T_RaisePlatGeneric;
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
		plat->thinker.function.acp1 = (actionf_p1)T_RaisePlatGeneric;
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
			if( remove_limits ) // allow_boom_sector_targets
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
			currplat->thinker.function.acv = nullptr;
		}
	}

	return 1;
}

// =================
//     TELEPORTS
// =================

DOOM_C_API bool EV_DoTeleportGenericThing( line_t* line, mobj_t* activator, teleporttype_e anglebehavior, fixed_t& outtargetx, fixed_t& outtargety, angle_t& outtargetangle )
{
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
					outtargetx		= targetmobj->x;
					outtargety		= targetmobj->y;
					outtargetangle	= targetmobj->angle;
					switch( anglebehavior )
					{
					case tt_setangle:
						outtargetangle = targetmobj->angle;
						break;
					case tt_preserveangle:
						outtargetangle = activator->angle;
						break;
					case tt_reverseangle:
						outtargetangle = activator->angle + ANG180;
						break;
					default:
						break;
					}

					return true;
				}

				break;
			}
		}
	}

	return false;
}

DOOM_C_API bool EV_DoTeleportGenericLine( line_t* line, mobj_t* activator, teleporttype_e anglebehavior, fixed_t& outtargetx, fixed_t& outtargety, angle_t& outtargetangle )
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
			uint32_t finediff = FINEANGLE( diff );
			fixed_t diffsine = finesine[ finediff ];
			fixed_t diffcosine = finecosine[ finediff ];

			fixed_t targetmidx = targetline.v1->x + ( targetline.dx >> 1 );
			fixed_t targetmidy = targetline.v1->y + ( targetline.dy >> 1 );

			fixed_t destx = targetmidx + FixedMul( mobjtosourcex, diffcosine ) - FixedMul( mobjtosourcey, diffsine );
			fixed_t desty = targetmidy + FixedMul( mobjtosourcex, diffsine ) + FixedMul( mobjtosourcey, diffcosine );

			if( P_TeleportMove( activator, destx, desty ) )
			{
				outtargetx		= destx;
				outtargety		= desty;
				outtargetangle	= activator->angle + diff;

				fixed_t momx = activator->momx;
				fixed_t momy = activator->momy;

				activator->momx = FixedMul( momx, diffcosine ) - FixedMul( momy, diffsine );
				activator->momy = FixedMul( momx, diffsine ) + FixedMul( momy, diffcosine );

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
	angle_t targetangle = 0;

	switch( line->action->param1 & tt_targetmask )
	{
	case tt_tothing:
		teleported = EV_DoTeleportGenericThing( line, activator, (teleporttype_t)( line->action->param1 & tt_anglemask ), targetx, targety, targetangle );
		break;

	case tt_toline:
		teleported = EV_DoTeleportGenericLine( line, activator, (teleporttype_t)( line->action->param1 & tt_anglemask ), targetx, targety, targetangle );
		break;
	}

	if( teleported )
	{
		if( gameversion != exe_final )
		{
			activator->z = activator->floorz;
		}

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
	line->action->param1 ? G_SecretExitLevel() : G_ExitLevel();

	return 1;
}

// =================
//  SECTOR ACTIONS
// =================

constexpr fixed_t FlatScrollScale = FixedDiv( 1, 32 );
constexpr fixed_t AccelScale = DoubleToFixed( 0.09375 );

INLINE void T_ScrollCeilingTexture( scroller_t* scroller )
{
	sector_t*& sector = scroller->sector;

	sector->ceiloffsetx += scroller->scrollx;
	sector->ceiloffsety += scroller->scrolly;
}

INLINE void T_ScrollFloorTexture( scroller_t* scroller )
{
	sector_t*& sector = scroller->sector;

	sector->flooroffsetx += scroller->scrollx;
	sector->flooroffsety += scroller->scrolly;
}

INLINE void T_CarryObjects( scroller_t* scroller )
{
	sector_t*& sector = scroller->sector;

	P_BlockThingsIterator( iota( sector->blockbox[ BOXLEFT ], sector->blockbox[ BOXRIGHT ] + 1 ),
							iota( sector->blockbox[ BOXBOTTOM ], sector->blockbox[ BOXTOP ] + 1 ),
							[ scroller ]( mobj_t* mobj ) -> bool
	{
		sector_t*& sector = scroller->sector;

		fixed_t scrollheight = sector->transferline ? sector->transferline->frontsector->floorheight
													: sector->floorheight;

		bool cancarry = false;
		int32_t carryshift = 0;
		switch( scroller->CarryType() )
		{
		case st_conveyor:
			cancarry = mobj->z == scrollheight;
			break;

		case st_wind:
			cancarry = mobj->z >= scrollheight;
			if( mobj->z > scrollheight ) carryshift = 1;
			break;

		case st_current:
			cancarry = mobj->z <= scrollheight;
			break;

		default:
			break;
		}

		if( !cancarry
			|| ( mobj->flags & MF_NOGRAVITY ) )
		{
			return true;
		}

		bool insector = mobj->subsector->sector->index == sector->index
			|| BSP_PointInSubsector( mobj->x - mobj->radius, mobj->y + mobj->radius )->sector->index == sector->index
			|| BSP_PointInSubsector( mobj->x + mobj->radius, mobj->y + mobj->radius )->sector->index == sector->index
			|| BSP_PointInSubsector( mobj->x - mobj->radius, mobj->y - mobj->radius )->sector->index == sector->index
			|| BSP_PointInSubsector( mobj->x + mobj->radius, mobj->y - mobj->radius )->sector->index == sector->index;

		if( insector )
		{
			mobj->momx += FixedMul( ( scroller->scrollx >> carryshift ), AccelScale );
			mobj->momy += FixedMul( ( scroller->scrolly >> carryshift ), AccelScale );
		}

		return true;
	} );
}

INLINE void T_UpdateDisplacement( scroller_t* scroller )
{
	sector_t*& control = scroller->controlsector;

	fixed_t height = control->ceilingheight - control->floorheight;
	fixed_t diff = scroller->controlheight - height;

	scroller->scrollx = FixedMul( scroller->magx, diff );
	scroller->scrolly = FixedMul( scroller->magy, diff );

	scroller->controlheight = height;
}

INLINE void T_UpdateAccelerative( scroller_t* scroller )
{
	sector_t*& control = scroller->controlsector;

	fixed_t height = control->ceilingheight - control->floorheight;
	fixed_t diff = scroller->controlheight - height;

	scroller->scrollx += FixedMul( scroller->magx, diff );
	scroller->scrolly += FixedMul( scroller->magy, diff );

	scroller->controlheight = height;
}

DOOM_C_API void T_ScrollSector( scroller_t* scroller )
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
		break;

	case st_floor:
		T_ScrollFloorTexture( scroller );
		break;

	default:
		break;
	}

	if( scroller->CarryType() != st_none )
	{
		T_CarryObjects( scroller );
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
		return st_conveyor;

	case Transfer_WindByLength_Always:
		return st_wind;

	case Transfer_CurrentByLength_Always:
		return st_current;

	case Transfer_WindOrCurrentByPoint_Always:
		return st_wind | st_current;
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
		return st_displacement;

	case Scroll_CeilingTexture_Accelerative_Always:
	case Scroll_FloorTexture_Accelerative_Always:
	case Scroll_FloorObjects_Accelerative_Always:
	case Scroll_FloorTextureObjects_Accelerative_Always:
		return st_accelerative;

	case Transfer_WindOrCurrentByPoint_Always:
		return st_point;
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

DOOM_C_API int32_t P_SpawnSectorScroller( line_t* line )
{
	scrolltype_t type = SelectScroller( line->special );
	if( type == st_none )
	{
		return 0;
	}

	int32_t createdcount = 0;

	scroller_t scroll = {};
	scroll.thinker.function.acp1 = (actionf_p1)&T_ScrollSector;
	scroll.type				= type;
	if( ( scroll.SpeedType() & ( st_accelerative | st_displacement ) ) != st_none )
	{
		if( line->frontsector == nullptr )
		{
			I_Error( "P_SpawnSectorScroller: non-static scroller line %d has no front sector", line->index );
		}
		scroll.controlsector	= line->frontsector;
		scroll.controlheight	= line->frontsector->ceilingheight - line->frontsector->floorheight;
		scroll.magx				= FixedMul( line->dx, FlatScrollScale );
		scroll.magy				= FixedMul( line->dy, FlatScrollScale );
		scroll.scrollx			= 0;
		scroll.scrolly			= 0;
	}
	else
	{
		scroll.scrollx			= FixedMul( line->dx, FlatScrollScale );
		scroll.scrolly			= FixedMul( line->dy, FlatScrollScale );
	}

	for( sector_t& sector : Sectors() )
	{
		if( sector.tag == line->tag )
		{
			++createdcount;
			scroller_t* scroller = (scroller_t*)Z_MallocZero( sizeof(scroller_t), PU_LEVSPEC, 0 );
			*scroller = scroll;
			P_AddThinker( &scroller->thinker );
			scroller->sector = &sector;
		}
	}

	return createdcount;
}


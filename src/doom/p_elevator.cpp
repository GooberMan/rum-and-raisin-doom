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
//	New actor actions for the MBF standard
//

#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "p_lineaction.h"

#include "m_random.h"

#include "i_system.h"
#include "i_log.h"

#include "r_local.h"

#include "s_sound.h"

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
		elevator->sector->floorspecialdata = nullptr;
		elevator->sector->ceilingspecialdata = nullptr;
		return;
	}

	if( !( leveltime & 7 ) )
	{
		S_StartSound( &elevator->sector->soundorg, sfx_stnmov );
	}
}

DOOM_C_API int32_t EV_DoElevatorGeneric( line_t* line, mobj_t* activator )
{
	int32_t numelevatorscreated = 0;

	for( sector_t& sector : Sectors() )
	{
		if( sector.tag == line->tag && sector.floorspecialdata == nullptr && sector.ceilingspecialdata == nullptr )
		{
			++numelevatorscreated;

			elevator_t* elevator = (elevator_t*)Z_MallocZero( sizeof(elevator_t), PU_LEVSPEC, 0 );
			P_AddThinker( &elevator->thinker );
			elevator->sector = &sector;
			elevator->sector->floorspecialdata = elevator;
			elevator->sector->ceilingspecialdata = elevator;
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
		}
	}

	return numelevatorscreated;
}

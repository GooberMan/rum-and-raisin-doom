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
// DESCRIPTION:  Ceiling aninmation (lowering, crushing, raising)
//



#include "doomdef.h"
#include "doomtype.h"
#include "doomstat.h"
#include "i_log.h"

#include "p_local.h"
#include "p_lineaction.h"

// State.
#include "r_state.h"

#include "s_sound.h"
#include "sounds.h"

#include "z_zone.h"

//
// CEILINGS
//

extern "C"
{
	ceiling_t*	activeceilings[MAXCEILINGS];
	ceiling_t*	activeceilingshead = nullptr;
}


//
// T_MoveCeiling
//

DOOM_C_API void T_MoveCeiling (ceiling_t* ceiling)
{
    result_e	res;
	
    switch(ceiling->direction)
    {
      case 0:
	// IN STASIS
	break;
      case 1:
	// UP
	res = T_MovePlane(ceiling->sector,
			  ceiling->speed,
			  ceiling->topheight,
			  false,1,ceiling->direction);
	
	if (!(leveltime&7))
	{
	    switch(ceiling->type)
	    {
	      case silentCrushAndRaise:
		break;
	      default:
		S_StartSound(&ceiling->sector->soundorg, sfx_stnmov);
		// ?
		break;
	    }
	}
	
	if (res == pastdest)
	{
	    switch(ceiling->type)
	    {
	      case raiseToHighest:
		P_RemoveActiveCeiling(ceiling);
		break;
		
	      case silentCrushAndRaise:
		S_StartSound(&ceiling->sector->soundorg, sfx_pstop);
	      case fastCrushAndRaise:
	      case crushAndRaise:
		ceiling->direction = -1;
		break;
		
	      default:
		break;
	    }
	    
	}
	break;
	
      case -1:
	// DOWN
	res = T_MovePlane(ceiling->sector,
			  ceiling->speed,
			  ceiling->bottomheight,
			  ceiling->crush,1,ceiling->direction);
	
	if (!(leveltime&7))
	{
	    switch(ceiling->type)
	    {
	      case silentCrushAndRaise: break;
	      default:
		S_StartSound(&ceiling->sector->soundorg, sfx_stnmov);
	    }
	}
	
	if (res == pastdest)
	{
	    switch(ceiling->type)
	    {
	      case silentCrushAndRaise:
		S_StartSound(&ceiling->sector->soundorg, sfx_pstop);
	      case crushAndRaise:
		ceiling->speed = CEILSPEED;
	      case fastCrushAndRaise:
		ceiling->direction = 1;
		break;

	      case lowerAndCrush:
	      case lowerToFloor:
		P_RemoveActiveCeiling(ceiling);
		break;

	      default:
		break;
	    }
	}
	else // ( res != pastdest )
	{
	    if (res == crushed)
	    {
		switch(ceiling->type)
		{
		  case silentCrushAndRaise:
		  case crushAndRaise:
		  case lowerAndCrush:
		    ceiling->speed = CEILSPEED / 8;
		    break;

		  default:
		    break;
		}
	    }
	}
	break;
    }
}


//
// EV_DoCeiling
// Move a ceiling up/down and all around!
//
DOOM_C_API int EV_DoCeiling( line_t* line, ceiling_e type )
{
    int		secnum;
    int		rtn;
    sector_t*	sec;
    ceiling_t*	ceiling;
	
    secnum = -1;
    rtn = 0;
    
    //	Reactivate in-stasis ceilings...for certain types.
    switch(type)
    {
      case fastCrushAndRaise:
      case silentCrushAndRaise:
      case crushAndRaise:
	P_ActivateInStasisCeiling(line);
      default:
	break;
    }
	
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	sec = &sectors[secnum];
	if (sec->specialdata)
	    continue;
	
	// new door thinker
	rtn = 1;
	ceiling = (ceiling_t*)Z_Malloc (sizeof(ceiling_t), PU_LEVSPEC, 0);
	P_AddThinker (&ceiling->thinker);
	sec->specialdata = ceiling;
	ceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
	ceiling->sector = sec;
	ceiling->crush = false;
	
	switch(type)
	{
	  case fastCrushAndRaise:
	    ceiling->crush = true;
	    ceiling->topheight = sec->ceilingheight;
	    ceiling->bottomheight = sec->floorheight + (8*FRACUNIT);
	    ceiling->direction = -1;
	    ceiling->speed = CEILSPEED * 2;
	    break;

	  case silentCrushAndRaise:
	  case crushAndRaise:
	    ceiling->crush = true;
	    ceiling->topheight = sec->ceilingheight;
	  case lowerAndCrush:
	  case lowerToFloor:
	    ceiling->bottomheight = sec->floorheight;
	    if (type != lowerToFloor)
		ceiling->bottomheight += 8*FRACUNIT;
	    ceiling->direction = -1;
	    ceiling->speed = CEILSPEED;
	    break;

	  case raiseToHighest:
	    ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
	    ceiling->direction = 1;
	    ceiling->speed = CEILSPEED;
	    break;
	default:
		break;
	}
		
	ceiling->tag = sec->tag;
	ceiling->type = type;
	P_AddActiveCeiling(ceiling);
    }
    return rtn;
}


//
// Add an active ceiling
//
DOOM_C_API void P_AddActiveCeiling(ceiling_t* c)
{
    int		i;
    
    for (i = 0; i < MAXCEILINGS;i++)
    {
	if (activeceilings[i] == NULL)
	{
	    activeceilings[i] = c;
	    return;
	}
    }
}



//
// Remove a ceiling's thinker
//
DOOM_C_API void P_RemoveActiveCeiling(ceiling_t* c)
{
    int		i;
	
    for (i = 0;i < MAXCEILINGS;i++)
    {
	if (activeceilings[i] == c)
	{
	    activeceilings[i]->sector->specialdata = NULL;
	    P_RemoveThinker (&activeceilings[i]->thinker);
	    activeceilings[i] = NULL;
	    break;
	}
    }
}



//
// Restart a ceiling that's in-stasis
//
DOOM_C_API void P_ActivateInStasisCeiling(line_t* line)
{
    int		i;
	
    for (i = 0;i < MAXCEILINGS;i++)
    {
	if (activeceilings[i]
	    && (activeceilings[i]->tag == line->tag)
	    && (activeceilings[i]->direction == 0))
	{
	    activeceilings[i]->direction = activeceilings[i]->olddirection;
	    activeceilings[i]->thinker.function.acp1
	      = (actionf_p1)T_MoveCeiling;
	}
    }
}



//
// EV_CeilingCrushStop
// Stop a ceiling from crushing!
//
DOOM_C_API int	EV_CeilingCrushStop(line_t	*line)
{
    int		i;
    int		rtn;
	
    rtn = 0;
    for (i = 0;i < MAXCEILINGS;i++)
    {
	if (activeceilings[i]
	    && (activeceilings[i]->tag == line->tag)
	    && (activeceilings[i]->direction != 0))
	{
	    activeceilings[i]->olddirection = activeceilings[i]->direction;
	    activeceilings[i]->thinker.function.acv = (actionf_v)NULL;
	    activeceilings[i]->direction = 0;		// in-stasis
	    rtn = 1;
	}
    }
    

    return rtn;
}

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
			ceiling->sector->ceilingspecialdata = nullptr;
			return;
		}
	}
	I_Error ( "P_RemoveActiveceiling: can't find ceiling!" );
}

void P_ActivateInStasisCeilingsGeneric( int tag )
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

	if( res == pastdest )
	{
		S_StartSound( &ceiling->sector->soundorg, sfx_pstop );
		P_RemoveActiveCeilingGeneric( ceiling );
	}
	else if( sectorsound )
	{
		S_StartSound( &ceiling->sector->soundorg, sfx_stnmov );
	}
}

DOOM_C_API int32_t EV_DoCeilingGeneric( line_t* line, mobj_t* activator )
{
	int32_t createdcount = 0;

	for( sector_t& sector : Sectors() )
	{
		if( sector.tag == line->tag && sector.ceilingspecialdata == nullptr )
		{
			++createdcount;

			ceiling_t* ceiling = (ceiling_t*)Z_MallocZero( sizeof(ceiling_t), PU_LEVSPEC, 0 );
			P_AddThinker( &ceiling->thinker );
			P_AddActiveCeilingGeneric( ceiling );
			ceiling->sector = &sector;
			ceiling->sector->ceilingspecialdata = ceiling;
			ceiling->thinker.function.acp1 = (actionf_p1)&T_MoveCeilingGeneric;
			ceiling->type = genericCeiling;
			ceiling->crush = line->action->param6 == sc_crush;
			ceiling->direction = line->action->param2;
			ceiling->speed = line->action->speed;

			fixed_t& heighttarget = ceiling->direction == sd_down ? ceiling->bottomheight : ceiling->topheight;

			switch( (sectortargettype_t)line->action->param1 )
			{
			case stt_highestneighborfloor:
			case stt_highestneighborfloor_noaddifmatch:
				heighttarget = P_FindHighestFloorSurrounding( &sector );
				break;

			case stt_lowestneighborfloor:
				heighttarget = P_FindLowestFloorSurrounding( &sector );
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
				heighttarget = sector.ceilingheight;
				break;

			case stt_ceiling:
				I_LogAddEntryVar( Log_Error, "EV_DoFloorGeneric: Line %d is trying to move a sector's ceiling to the ceiling", line->index );
				break;

			case stt_shortestlowertexture:
				{
					int32_t lowestheight = INT_MAX;
					for( line_t* secline : Lines( sector ) )
					{
						if( secline->frontside && secline->frontside->bottomtexture >= 0 )
						{
							lowestheight = M_MIN( lowestheight, texturelookup[ secline->frontside->bottomtexture ]->height );
						}
						if( secline->backside && secline->backside->bottomtexture >= 0 )
						{
							lowestheight = M_MIN( lowestheight, texturelookup[ secline->backside->bottomtexture ]->height );
						}
					}
					heighttarget = sector.ceilingheight + IntToFixed( lowestheight * ceiling->direction );
				}
				break;

			case stt_perpetual:
				I_LogAddEntryVar( Log_Error, "EV_DoFloorGeneric: Line %d is trying start a perpetual ceilingform, try using EV_DoPerpetualLiftGeneric instead", line->index );
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
				if( line->action->param6 == scm_numeric )
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
		}
	}

	return createdcount;
}
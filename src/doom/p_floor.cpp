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
//	Floor animation: raising stairs.
//



#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lineaction.h"

#include "i_log.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"
// Data.
#include "sounds.h"

//e6y
#define STAIRS_UNINITIALIZED_CRUSH_FIELD_VALUE 10

//
// FLOORS
//

//
// Move a plane (floor or ceiling) and check for crushing
//
DOOM_C_API result_e T_MovePlane( sector_t* sector, fixed_t speed, fixed_t dest, doombool crush, int floorOrCeiling, int direction )
{
    doombool	flag;
    fixed_t	lastpos;

	sector->snapfloor = sector->snapceiling = false;
	
	switch(floorOrCeiling)
	{
	case 0:
		// FLOOR
		switch(direction)
		{
		case -1:
			// DOWN
			if( sector->floorheight < dest )
			{
				sector->snapfloor = true;
			}

			if (sector->floorheight - speed < dest)
			{
				lastpos = sector->floorheight;
				sector->floorheight = dest;
				flag = P_ChangeSector(sector,crush);
				if (flag == true)
				{
					sector->floorheight =lastpos;
					P_ChangeSector(sector,crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				lastpos = sector->floorheight;
				sector->floorheight -= speed;
				flag = P_ChangeSector(sector,crush);
				if (flag == true)
				{
					sector->floorheight = lastpos;
					P_ChangeSector(sector,crush);
					return crushed;
				}
			}
			break;

		case 1:
			// UP
			if( sector->floorheight > dest )
			{
				sector->snapfloor = true;
			}

			if (sector->floorheight + speed > dest)
			{
			lastpos = sector->floorheight;
			sector->floorheight = dest;
			flag = P_ChangeSector(sector,crush);
			if (flag == true)
			{
				sector->floorheight = lastpos;
				P_ChangeSector(sector,crush);
				//return crushed;
			}
			return pastdest;
			}
			else
			{
			// COULD GET CRUSHED
			lastpos = sector->floorheight;
			sector->floorheight += speed;
			flag = P_ChangeSector(sector,crush);
			if (flag == true)
			{
				if (crush == true)
				return crushed;
				sector->floorheight = lastpos;
				P_ChangeSector(sector,crush);
				return crushed;
			}
			}
			break;
		}
		break;

	case 1:
		// CEILING
		switch(direction)
		{
		case -1:
			// DOWN
			if( sector->ceilingheight < dest )
			{
				sector->snapceiling = true;
			}
			if (sector->ceilingheight - speed < dest)
			{
				lastpos = sector->ceilingheight;
				sector->ceilingheight = dest;
				flag = P_ChangeSector(sector,crush);

				if (flag == true)
				{
					sector->ceilingheight = lastpos;
					P_ChangeSector(sector,crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				// COULD GET CRUSHED
				lastpos = sector->ceilingheight;
				sector->ceilingheight -= speed;
				flag = P_ChangeSector(sector,crush);

				if (flag == true)
				{
					if (crush == true)
					return crushed;
					sector->ceilingheight = lastpos;
					P_ChangeSector(sector,crush);
					return crushed;
				}
			}
			break;

		case 1:
			// UP
			if( sector->ceilingheight > dest )
			{
				sector->snapceiling = true;
			}

			if (sector->ceilingheight + speed > dest)
			{
				lastpos = sector->ceilingheight;
				sector->ceilingheight = dest;
				flag = P_ChangeSector(sector,crush);
				if (flag == true)
				{
					sector->ceilingheight = lastpos;
					P_ChangeSector(sector,crush);
					//return crushed;
				}
				return pastdest;
			}
			else
			{
				lastpos = sector->ceilingheight;
				sector->ceilingheight += speed;
				flag = P_ChangeSector(sector,crush);
// UNUSED
#if 0
				if (flag == true)
				{
					sector->ceilingheight = lastpos;
					P_ChangeSector(sector,crush);
					return crushed;
				}
#endif
			}
			break;
		}
		break;
		
    }
    return ok;
}


//
// MOVE A FLOOR TO IT'S DESTINATION (UP OR DOWN)
//
DOOM_C_API void T_MoveFloor(floormove_t* floor)
{
    result_e	res;
	
    res = T_MovePlane(floor->sector,
						floor->speed,
						floor->floordestheight,
						floor->crush,0,floor->direction);

	if (!(leveltime&7))
	{
		S_StartSound(&floor->sector->soundorg, sfx_stnmov);
	}
    
    if (res == pastdest)
    {
		floor->sector->specialdata = NULL;
		if( floor->newspecial >= 0 )
		{
			floor->sector->special = floor->newspecial;
		}
		if( floor->texture >= 0 )
		{
			floor->sector->floorpic = floor->texture;
		}

		P_RemoveThinker(&floor->thinker);

		S_StartSound(&floor->sector->soundorg, sfx_pstop);
	}

}

//
// HANDLE FLOOR TYPES
//
DOOM_C_API int EV_DoFloor( line_t* line, floor_e floortype )
{
    int			secnum;
    int			rtn;
    int			i;
    sector_t*		sec;
    floormove_t*	floor;

    secnum = -1;
    rtn = 0;
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	sec = &sectors[secnum];
		
	// ALREADY MOVING?  IF SO, KEEP GOING...
	if (sec->specialdata)
	    continue;
	
	// new floor thinker
	rtn = 1;
	floor = (floormove_t*)Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
	P_AddThinker (&floor->thinker);
	sec->specialdata = floor;
	floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
	floor->type = floortype;
	floor->crush = false;
	floor->newspecial = -1;
	floor->texture = -1;

	switch(floortype)
	{
	  case lowerFloor:
	    floor->direction = -1;
	    floor->sector = sec;
	    floor->speed = FLOORSPEED;
	    floor->floordestheight = P_FindHighestFloorSurrounding(sec);
	    break;

	  case lowerFloorToLowest:
	    floor->direction = -1;
	    floor->sector = sec;
	    floor->speed = FLOORSPEED;
	    floor->floordestheight = P_FindLowestFloorSurrounding(sec);
	    break;

	  case turboLower:
	    floor->direction = -1;
	    floor->sector = sec;
	    floor->speed = FLOORSPEED * 4;
	    floor->floordestheight = P_FindHighestFloorSurrounding(sec);
	    if (gameversion <= exe_doom_1_2 || floor->floordestheight != sec->floorheight)
		{
			floor->floordestheight += 8*FRACUNIT;
		}
	    break;

	  case raiseFloorCrush:
	    floor->crush = true;
	  case raiseFloor:
	    floor->direction = 1;
	    floor->sector = sec;
	    floor->speed = FLOORSPEED;
	    floor->floordestheight = P_FindLowestCeilingSurrounding(sec);
	    if (floor->floordestheight > sec->ceilingheight)
		{
			floor->floordestheight = sec->ceilingheight;
		}
	    floor->floordestheight -= (8*FRACUNIT) * (floortype == raiseFloorCrush);
	    break;

	  case raiseFloorTurbo:
	    floor->direction = 1;
	    floor->sector = sec;
	    floor->speed = FLOORSPEED*4;
	    floor->floordestheight = P_FindNextHighestFloor(sec);
	    break;

	  case raiseFloorToNearest:
	    floor->direction = 1;
	    floor->sector = sec;
	    floor->speed = FLOORSPEED;
	    floor->floordestheight = P_FindNextHighestFloor(sec);
	    break;

	  case raiseFloor24:
	    floor->direction = 1;
	    floor->sector = sec;
	    floor->speed = FLOORSPEED;
	    floor->floordestheight = floor->sector->floorheight + 24 * FRACUNIT;
	    break;
	  case raiseFloor512:
	    floor->direction = 1;
	    floor->sector = sec;
	    floor->speed = FLOORSPEED;
	    floor->floordestheight = floor->sector->floorheight + 512 * FRACUNIT;
	    break;

	  case raiseFloor24AndChange:
	    floor->direction = 1;
	    floor->sector = sec;
	    floor->speed = FLOORSPEED;
	    floor->floordestheight = floor->sector->floorheight + 24 * FRACUNIT;
	    sec->floorpic = line->frontsector->floorpic;
	    sec->special = line->frontsector->special;
	    break;

	  case raiseToTexture:
	  {
	      int	minsize = INT_MAX;
	      side_t*	side;
				
	      floor->direction = 1;
	      floor->sector = sec;
	      floor->speed = FLOORSPEED;
	      for (i = 0; i < sec->linecount; i++)
	      {
		  if (twoSided (secnum, i) )
		  {
		      side = getSide(secnum,i,0);
		      if (side->bottomtexture >= 0)
			  if (texturelookup[side->bottomtexture]->height < 
			      minsize)
			      minsize = 
				  texturelookup[side->bottomtexture]->height;
		      side = getSide(secnum,i,1);
		      if (side->bottomtexture >= 0)
			  if (texturelookup[side->bottomtexture]->height < 
			      minsize)
			      minsize = 
				  texturelookup[side->bottomtexture]->height;
		  }
	      }
	      floor->floordestheight = floor->sector->floorheight + IntToFixed( minsize );
	  }
	  break;
	  
	  case lowerAndChange:
	    floor->direction = -1;
	    floor->sector = sec;
	    floor->speed = FLOORSPEED;
	    floor->floordestheight = 
		P_FindLowestFloorSurrounding(sec);
	    floor->texture = sec->floorpic;

	    for (i = 0; i < sec->linecount; i++)
	    {
		if ( twoSided(secnum, i) )
		{
		    if (getSide(secnum,i,0)->sector-sectors == secnum)
		    {
			sec = getSector(secnum,i,1);

			if (sec->floorheight == floor->floordestheight)
			{
			    floor->texture = sec->floorpic;
			    floor->newspecial = sec->special;
			    break;
			}
		    }
		    else
		    {
			sec = getSector(secnum,i,0);

			if (sec->floorheight == floor->floordestheight)
			{
			    floor->texture = sec->floorpic;
			    floor->newspecial = sec->special;
			    break;
			}
		    }
		}
	    }
	  default:
	    break;
	}
    }
    return rtn;
}




//
// BUILD A STAIRCASE!
//
DOOM_C_API int EV_BuildStairs( line_t* line, stair_e type )
{
    int			secnum;
    int			height;
    int			i;
    int			newsecnum;
    int			texture;
    int			ok;
    int			rtn;
    
    sector_t*		sec;
    sector_t*		tsec;

    floormove_t*	floor;
    
    fixed_t		stairsize = 0;
    fixed_t		speed = 0;

    secnum = -1;
    rtn = 0;
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	sec = &sectors[secnum];
		
	// ALREADY MOVING?  IF SO, KEEP GOING...
	if (sec->specialdata)
	    continue;
	
	// new floor thinker
	rtn = 1;
	floor = (floormove_t*)Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
	P_AddThinker (&floor->thinker);
	sec->specialdata = floor;
	floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
	floor->direction = 1;
	floor->sector = sec;
	switch(type)
	{
	  case build8:
	    speed = FLOORSPEED/4;
	    stairsize = 8*FRACUNIT;
	    break;
	  case turbo16:
	    speed = FLOORSPEED*4;
	    stairsize = 16*FRACUNIT;
	    break;
	}
	floor->speed = speed;
	height = sec->floorheight + stairsize;
	floor->floordestheight = height;
	// Initialize
	floor->type = lowerFloor;
	// e6y
	// Uninitialized crush field will not be equal to 0 or 1 (true)
	// with high probability. So, initialize it with any other value
	floor->crush = STAIRS_UNINITIALIZED_CRUSH_FIELD_VALUE;
	floor->newspecial = -1;
	floor->texture = -1;

	texture = sec->floorpic;
	
	// Find next sector to raise
	// 1.	Find 2-sided line with same sector side[0]
	// 2.	Other side is the next sector to raise
	do
	{
	    ok = 0;
	    for (i = 0;i < sec->linecount;i++)
	    {
		if ( !((sec->lines[i])->flags & ML_TWOSIDED) )
		    continue;
					
		tsec = (sec->lines[i])->frontsector;
		newsecnum = (int)( tsec - sectors );
		
		if (secnum != newsecnum)
		    continue;

		tsec = (sec->lines[i])->backsector;
		newsecnum = (int)( tsec - sectors );

		if (tsec->floorpic != texture)
		    continue;
					
		height += stairsize;

		if (tsec->specialdata)
		    continue;
					
		sec = tsec;
		secnum = newsecnum;
		floor = (floormove_t*)Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);

		P_AddThinker (&floor->thinker);

		sec->specialdata = floor;
		floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
		floor->direction = 1;
		floor->sector = sec;
		floor->speed = speed;
		floor->floordestheight = height;
		// Initialize
		floor->type = lowerFloor;
		// e6y
		// Uninitialized crush field will not be equal to 0 or 1 (true)
		// with high probability. So, initialize it with any other value
		floor->crush = STAIRS_UNINITIALIZED_CRUSH_FIELD_VALUE;
		floor->newspecial = -1;
		floor->texture = -1;
		ok = 1;
		break;
	    }
	} while(ok);
    }
    return rtn;
}

DOOM_C_API int32_t EV_DoFloorGeneric( line_t* line, mobj_t* activator )
{
	int32_t createdcount = 0;

	for( sector_t& sector : Sectors() )
	{
		if( sector.tag == line->tag || sector.specialdata != nullptr )
		{
			++createdcount;

			floormove_t* floor = (floormove_t*)Z_Malloc( sizeof(floormove_t), PU_LEVSPEC, 0 );
			P_AddThinker( &floor->thinker );
			sector.specialdata = floor;
			floor->thinker.function.acp1 = (actionf_p1)&T_MoveFloor;
			floor->type = genericFloor;
			floor->crush = line->action->param6 == sc_crush;
			floor->sector = &sector;
			floor->direction = line->action->param2;
			floor->speed = line->action->speed;

			switch( (sectortargettype_t)line->action->param1 )
			{
			case stt_highestneighborfloor:
				floor->floordestheight = P_FindHighestFloorSurrounding( &sector );
				break;

			case stt_lowestneighborfloor:
				floor->floordestheight = P_FindLowestFloorSurrounding( &sector );
				break;

			case stt_nexthighestneighborfloor:
				floor->floordestheight = P_FindNextHighestFloor( &sector ); // This will preserve a vanilla bug
				break;

			case stt_nextlowestneighborfloor:
				floor->floordestheight = P_FindNextLowestFloorSurrounding( &sector );
				break;

			case stt_highestneighborceiling:
				floor->floordestheight = P_FindHighestCeilingSurrounding( &sector );
				break;

			case stt_lowestneighborceiling:
				floor->floordestheight = P_FindLowestCeilingSurrounding( &sector );
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
					floor->floordestheight = sector.floorheight + IntToFixed( lowestheight * floor->direction );
				}
				break;

			case stt_perpetual:
				I_LogAddEntryVar( Log_Error, "EV_DoFloorGeneric: Line %d is trying start a perpetual platform, try using EV_DoPerpetualLiftGeneric instead", line->index );
				break;

			case stt_nosearch:
			default:
				floor->floordestheight = sector.floorheight;
				break;
			}

			floor->floordestheight += (fixed_t)line->action->param3;

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
		}
	}

	return createdcount;
}

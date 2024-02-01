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
//	Plats (i.e. elevator platforms) code, raising/lowering.
//

#include <stdio.h>

#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"
#include "p_lineaction.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "sounds.h"


DOOM_C_API plat_t*		activeplats[MAXPLATS] =  {};
DOOM_C_API plat_t*		activeplatshead = nullptr;


//
// Move a plat up and down
//
void T_PlatRaise(plat_t* plat)
{
    result_e	res;
	
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
	    if (!(leveltime&7))
		S_StartSound(&plat->sector->soundorg, sfx_stnmov);
	}
	
				
	if (res == crushed && (!plat->crush))
	{
	    plat->count = plat->wait;
	    plat->status = down;
	    S_StartSound(&plat->sector->soundorg, sfx_pstart);
	}
	else
	{
	    if (res == pastdest)
	    {
		plat->count = plat->wait;
		plat->status = waiting;
		S_StartSound(&plat->sector->soundorg, sfx_pstop);

		switch(plat->type)
		{
		  case blazeDWUS:
		  case downWaitUpStay:
		    P_RemoveActivePlat(plat);
		    break;
		    
		  case raiseAndChange:
		  case raiseToNearestAndChange:
		    P_RemoveActivePlat(plat);
		    break;
		    
		  default:
		    break;
		}
	    }
	}
	break;
	
      case	down:
	res = T_MovePlane(plat->sector,plat->speed,plat->low,false,0,-1);

	if (res == pastdest)
	{
	    plat->count = plat->wait;
	    plat->status = waiting;
	    S_StartSound(&plat->sector->soundorg,sfx_pstop);
	}
	break;
	
      case	waiting:
	if (!--plat->count)
	{
	    if (plat->sector->floorheight == plat->low)
		plat->status = up;
	    else
		plat->status = down;
	    S_StartSound(&plat->sector->soundorg,sfx_pstart);
	}
      case	in_stasis:
	break;
    }
}


//
// Do Platforms
//  "amount" is only used for SOME platforms.
//
int
EV_DoPlat
( line_t*	line,
  plattype_e	type,
  int		amount )
{
    plat_t*	plat;
    int		secnum;
    int		rtn;
    sector_t*	sec;
	
    secnum = -1;
    rtn = 0;

    
    //	Activate all <type> plats that are in_stasis
    switch(type)
    {
      case perpetualRaise:
	P_ActivateInStasis(line->tag);
	break;
	
      default:
	break;
    }
	
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	sec = &sectors[secnum];

	if (sec->specialdata)
	    continue;
	
	// Find lowest & highest floors around sector
	rtn = 1;
	plat = (plat_t*)Z_Malloc( sizeof(*plat), PU_LEVSPEC, 0);
	P_AddThinker(&plat->thinker);
		
	plat->type = type;
	plat->sector = sec;
	plat->sector->specialdata = plat;
	plat->thinker.function.acp1 = (actionf_p1) T_PlatRaise;
	plat->crush = false;
	plat->tag = line->tag;
	
	switch(type)
	{
	  case raiseToNearestAndChange:
	    plat->speed = PLATSPEED/2;
	    sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
	    plat->high = P_FindNextHighestFloor(sec);
	    plat->wait = 0;
	    plat->status = up;
	    // NO MORE DAMAGE, IF APPLICABLE
	    sec->special = 0;		

	    S_StartSound(&sec->soundorg,sfx_stnmov);
	    break;
	    
	  case raiseAndChange:
	    plat->speed = PLATSPEED/2;
	    sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
	    plat->high = sec->floorheight + amount*FRACUNIT;
	    plat->wait = 0;
	    plat->status = up;

	    S_StartSound(&sec->soundorg,sfx_stnmov);
	    break;
	    
	  case downWaitUpStay:
	    plat->speed = PLATSPEED * 4;
	    plat->low = P_FindLowestFloorSurrounding(sec);

	    if (plat->low > sec->floorheight)
		plat->low = sec->floorheight;

	    plat->high = sec->floorheight;
	    plat->wait = TICRATE*PLATWAIT;
	    plat->status = down;
	    S_StartSound(&sec->soundorg,sfx_pstart);
	    break;
	    
	  case blazeDWUS:
	    plat->speed = PLATSPEED * 8;
	    plat->low = P_FindLowestFloorSurrounding(sec);

	    if (plat->low > sec->floorheight)
		plat->low = sec->floorheight;

	    plat->high = sec->floorheight;
	    plat->wait = TICRATE*PLATWAIT;
	    plat->status = down;
	    S_StartSound(&sec->soundorg,sfx_pstart);
	    break;
	    
	  case perpetualRaise:
	    plat->speed = PLATSPEED;
	    plat->low = P_FindLowestFloorSurrounding(sec);

	    if (plat->low > sec->floorheight)
		plat->low = sec->floorheight;

	    plat->high = P_FindHighestFloorSurrounding(sec);

	    if (plat->high < sec->floorheight)
		plat->high = sec->floorheight;

	    plat->wait = TICRATE*PLATWAIT;
	    plat->status = (plat_e)( P_Random() & 1 );

	    S_StartSound(&sec->soundorg,sfx_pstart);
	    break;
	}
	P_AddActivePlat(plat);
    }
    return rtn;
}

void P_ActivateInStasis(int tag)
{
	int		i;
	
    for (i = 0;i < MAXPLATS;i++)
	if (activeplats[i]
	    && (activeplats[i])->tag == tag
	    && (activeplats[i])->status == in_stasis)
	{
	    (activeplats[i])->status = (activeplats[i])->oldstatus;
	    (activeplats[i])->thinker.function.acp1
	      = (actionf_p1) T_PlatRaise;
	}
}

void EV_StopPlat(line_t* line)
{
	if( remove_limits )
	{
		EV_StopAnyLiftGeneric( line, nullptr );
		return;
	}

    int		j;
	
    for (j = 0;j < MAXPLATS;j++)
	if (activeplats[j]
	    && ((activeplats[j])->status != in_stasis)
	    && ((activeplats[j])->tag == line->tag))
	{
	    (activeplats[j])->oldstatus = (activeplats[j])->status;
	    (activeplats[j])->status = in_stasis;
	    (activeplats[j])->thinker.function.acv = (actionf_v)NULL;
	}
}

void P_AddActivePlat( plat_t* plat )
{
    int		i;
    
    for (i = 0;i < MAXPLATS;i++)
	if (activeplats[i] == NULL)
	{
	    activeplats[i] = plat;
	    return;
	}
    I_Error ("P_AddActivePlat: no more plats!");
}

void P_RemoveActivePlatGeneric( plat_t* plat );
void P_RemoveActivePlat( plat_t* plat )
{
	// Hack
	if( activeplatshead )
	{
		P_RemoveActivePlatGeneric( plat );
		return;
	}

    int		i;
    for (i = 0;i < MAXPLATS;i++)
	if (plat == activeplats[i])
	{
	    (activeplats[i])->sector->specialdata = NULL;
	    P_RemoveThinker(&(activeplats[i])->thinker);
	    activeplats[i] = NULL;
	    
	    return;
	}
    I_Error ("P_RemoveActivePlat: can't find plat!");
}



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
			plat->sector->specialdata = nullptr;
			return;
		}
	}
	I_Error ( "P_RemoveActivePlat: can't find plat!" );
}

void P_ActivateInStasisPlatsGeneric( int tag )
{
	for( plat_t* currplat = activeplatshead; currplat != nullptr; currplat = currplat->nextactive )
	{
		if( currplat->tag == tag && currplat->status == in_stasis )
		{
			currplat->status = currplat->oldstatus;
			currplat->thinker.function.acp1 = (actionf_p1)T_PlatRaise;
		}
	}
}

DOOM_C_API int32_t EV_DoVanillaPlatformRaiseGeneric( line_t* line, mobj_t* activator )
{
	int32_t		platformscreated = 0;

	for( sector_t& sector : Sectors() )
	{
		if( sector.tag == line->tag || sector.specialdata != nullptr )
		{
			// Find lowest & highest floors around sector
			++platformscreated;
			plat_t* plat = (plat_t*)Z_Malloc( sizeof(plat_t), PU_LEVSPEC, 0 );
			P_AddThinker( &plat->thinker );
		
			plat->sector = &sector;
			plat->sector->specialdata = plat;
			plat->thinker.function.acp1 = (actionf_p1)T_PlatRaise;
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
		}
	}

	return platformscreated;
}

DOOM_C_API int32_t EV_DoPerpetualLiftGeneric( line_t* line, mobj_t* activator )
{
	P_ActivateInStasisPlatsGeneric( line->tag );

	int32_t		platformscreated = 0;

	for( sector_t& sector : Sectors() )
	{
		if( sector.tag == line->tag || sector.specialdata != nullptr )
		{
			// Find lowest & highest floors around sector
			++platformscreated;
			plat_t* plat = (plat_t*)Z_Malloc( sizeof(plat_t), PU_LEVSPEC, 0 );
			P_AddThinker( &plat->thinker );
	
			plat->type = perpetualRaise;
			plat->sector = &sector;
			plat->sector->specialdata = plat;
			plat->thinker.function.acp1 = (actionf_p1)T_PlatRaise;
			plat->crush = false;
			plat->tag = line->tag;
			plat->speed = line->action->speed;
			plat->wait = line->action->delay;
			plat->status = (plat_e)( P_Random() & 1 );

			plat->low = M_MIN( P_FindLowestFloorSurrounding( &sector ), sector.floorheight );
			plat->high = M_MAX( P_FindHighestFloorSurrounding( &sector ), sector.floorheight );

			S_StartSound( &sector.soundorg, sfx_pstart );

			P_AddActivePlatGeneric( plat );
		}
	}

	return platformscreated;
}

DOOM_C_API int32_t EV_DoLiftGeneric( line_t* line, mobj_t* activator )
{
	int32_t		platformscreated = 0;

	for( sector_t& sector : Sectors() )
	{
		if( sector.tag == line->tag || sector.specialdata != nullptr )
		{
			// Find lowest & highest floors around sector
			++platformscreated;
			plat_t* plat = (plat_t*)Z_Malloc( sizeof(plat_t), PU_LEVSPEC, 0 );
			P_AddThinker( &plat->thinker );
		
			plat->type = downWaitUpStay;
			plat->sector = &sector;
			plat->sector->specialdata = plat;
			plat->thinker.function.acp1 = (actionf_p1)T_PlatRaise;
			plat->crush = false;
			plat->tag = line->tag;
			plat->speed = line->action->speed;
			plat->wait = line->action->delay;
			plat->status = down;

			switch( line->action->param1 )
			{
			case stt_lowestneighborfloor:
				plat->low = P_FindLowestFloorSurrounding( &sector );
				break;

			case stt_nextlowestneighborfloor:
				plat->low = P_FindNextLowestFloorSurrounding( &sector );
				break;

			case stt_lowestneighborceiling:
				plat->low = P_FindLowestCeilingSurrounding( &sector );
				break;

			default:
				break;
			}

			plat->low = M_MIN( plat->low, sector.floorheight );
			plat->high = sector.floorheight;

			S_StartSound( &sector.soundorg, sfx_pstart );

			P_AddActivePlatGeneric( plat );
		}
	}

	return platformscreated;
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

	return 0;
}

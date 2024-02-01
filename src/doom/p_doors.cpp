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
// DESCRIPTION: Door animation code (opening/closing)
//



#include "z_zone.h"
#include "doomdef.h"
#include "deh_main.h"
#include "p_local.h"
#include "p_lineaction.h"
#include "i_system.h"
#include "i_log.h"

#include "s_sound.h"


// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

#if 0
//
// Sliding door frame information
//
slidename_t	slideFrameNames[MAXSLIDEDOORS] =
{
    {"GDOORF1","GDOORF2","GDOORF3","GDOORF4",	// front
     "GDOORB1","GDOORB2","GDOORB3","GDOORB4"},	// back
	 
    {"\0","\0","\0","\0"}
};
#endif


//
// VERTICAL DOORS
//

//
// T_VerticalDoor
//
void T_VerticalDoor (vldoor_t* door)
{
	result_e	res;
	
	switch(door->direction)
	{
	case sd_none:
		// WAITING
		if (!--door->topcountdown)
		{
			switch(door->type)
			{
			case vld_blazeRaise:
				door->direction = sd_close; // time to go back down
				S_StartSound(&door->sector->soundorg, sfx_bdcls);
				break;
		
			case vld_normal:
				door->direction = sd_close; // time to go back down
				S_StartSound(&door->sector->soundorg, sfx_dorcls);
				break;
		
			case vld_close30ThenOpen:
				door->direction = sd_open;
				S_StartSound(&door->sector->soundorg, sfx_doropn);
				break;
		
				default:
			break;
			}
		}
		break;
	
	case sd_raisein5nonsense:
		//  INITIAL WAIT
		if (!--door->topcountdown)
		{
			switch(door->type)
			{
			case vld_raiseIn5Mins:
				door->direction = sd_open;
				door->type = vld_normal;
				S_StartSound(&door->sector->soundorg, sfx_doropn);
				break;
		
			default:
				break;
			}
		}
		break;
	
	case sd_close:
		// DOWN
		res = T_MovePlane( door->sector, door->speed, door->sector->floorheight, false, 1, door->direction );
		if (res == pastdest)
		{
			switch(door->type)
			{
			case vld_blazeRaise:
			case vld_blazeClose:
				door->sector->specialdata = NULL;
				P_RemoveThinker (&door->thinker);  // unlink and free
				S_StartSound(&door->sector->soundorg, sfx_bdcls);
				break;

			case vld_normal:
			case vld_close:
				door->sector->specialdata = NULL;
				P_RemoveThinker (&door->thinker);  // unlink and free
				break;

			case vld_close30ThenOpen:
				door->direction = sd_none;
				door->topcountdown = TICRATE*30;
				break;

			default:
				break;
			}
		}
		else if (res == crushed)
		{
			switch(door->type)
			{
			case vld_blazeClose:
			case vld_close:		// DO NOT GO BACK UP!
				break;
		
			default:
				door->direction = sd_open;
				S_StartSound(&door->sector->soundorg, sfx_doropn);
				break;
			}
		}
		break;
	
	case sd_open:
		// UP
		res = T_MovePlane( door->sector, door->speed, door->topheight, false, 1, door->direction );
	
		if (res == pastdest)
		{
			switch(door->type)
			{
			case vld_blazeRaise:
			case vld_normal:
				door->direction = sd_none; // wait at top
				door->topcountdown = door->topwait;
				break;

			case vld_close30ThenOpen:
			case vld_blazeOpen:
			case vld_open:
				door->sector->specialdata = NULL;
				P_RemoveThinker (&door->thinker);  // unlink and free
				break;

			default:
				break;
			}
		}
		break;
	}
}

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

void T_VerticalDoorGeneric( vldoor_t* door )
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
				door->sector->specialdata = NULL;
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
				door->sector->specialdata = NULL;
				P_RemoveThinker( &door->thinker ); // unlink and free
			}
		}
		break;

	default:
		break;
	}
}

//
// EV_DoLockedDoor
// Move a locked door up/down
//

int
EV_DoLockedDoor
( line_t*	line,
  vldoor_e	type,
  mobj_t*	thing )
{
    player_t*	p;
	
    p = thing->player;
	
    if (!p)
	return 0;
		
    switch(line->special)
    {
      case 99:	// Blue Lock
      case 133:
	if (!p->cards[it_bluecard] && !p->cards[it_blueskull])
	{
	    p->message = DEH_String(PD_BLUEO);
	    S_StartSound(NULL,sfx_oof);
	    return 0;
	}
	break;
	
      case 134: // Red Lock
      case 135:
	if (!p->cards[it_redcard] && !p->cards[it_redskull])
	{
	    p->message = DEH_String(PD_REDO);
	    S_StartSound(NULL,sfx_oof);
	    return 0;
	}
	break;
	
      case 136:	// Yellow Lock
      case 137:
	if (!p->cards[it_yellowcard] &&
	    !p->cards[it_yellowskull])
	{
	    p->message = DEH_String(PD_YELLOWO);
	    S_StartSound(NULL,sfx_oof);
	    return 0;
	}
	break;	
    }

    return EV_DoDoor(line,type);
}


int EV_DoDoor ( line_t* line, vldoor_e type )
{
	int		secnum,rtn;
	sector_t*	sec;
	vldoor_t*	door;

	secnum = -1;
	rtn = 0;

	while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
	{
		sec = &sectors[secnum];
		if (sec->specialdata)
			continue;
		
		// new door thinker
		rtn = 1;
		door = (vldoor_t*)Z_Malloc (sizeof(*door), PU_LEVSPEC, 0);
		P_AddThinker (&door->thinker);
		sec->specialdata = door;

		door->thinker.function.acp1 = (actionf_p1) T_VerticalDoor;
		door->sector = sec;
		door->type = type;
		door->topwait = VDOORWAIT;
		door->speed = VDOORSPEED;

		switch(type)
		{
		  case vld_blazeClose:
			door->topheight = P_FindLowestCeilingSurrounding(sec);
			door->topheight -= 4*FRACUNIT;
			door->direction = sd_close;
			door->speed = VDOORSPEED * 4;
			S_StartSound(&door->sector->soundorg, sfx_bdcls);
			break;

		  case vld_close:
			door->topheight = P_FindLowestCeilingSurrounding(sec);
			door->topheight -= 4*FRACUNIT;
			door->direction = sd_close;
			S_StartSound(&door->sector->soundorg, sfx_dorcls);
			break;

		  case vld_close30ThenOpen:
			door->topheight = sec->ceilingheight;
			door->direction = sd_close;
			S_StartSound(&door->sector->soundorg, sfx_dorcls);
			break;

		  case vld_blazeRaise:
		  case vld_blazeOpen:
			door->direction = sd_open;
			door->topheight = P_FindLowestCeilingSurrounding(sec);
			door->topheight -= 4*FRACUNIT;
			door->speed = VDOORSPEED * 4;
			if (door->topheight != sec->ceilingheight)
			S_StartSound(&door->sector->soundorg, sfx_bdopn);
			break;

		  case vld_normal:
		  case vld_open:
			door->direction = sd_open;
			door->topheight = P_FindLowestCeilingSurrounding(sec);
			door->topheight -= 4*FRACUNIT;
			if (door->topheight != sec->ceilingheight)
			S_StartSound(&door->sector->soundorg, sfx_doropn);
			break;

		  default:
			break;
		}
	}
	return rtn;
}

void EV_DoDoorGeneric( line_t* line, sector_t* sec )
{
	vldoor_t* door = (vldoor_t*)Z_Malloc( sizeof(vldoor_t), PU_LEVSPEC, 0 );

	P_AddThinker (&door->thinker);
	sec->specialdata = door;

	door->thinker.function.acp1 = (actionf_p1)T_VerticalDoorGeneric;
	door->sector = sec;
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
		door->topheight = sec->ceilingheight;
	}
	else
	{
		door->topheight = P_FindLowestCeilingSurrounding(sec);
		door->topheight -= IntToFixed( 4 );
	}

	if( door->direction == sd_close || door->topheight != sec->ceilingheight )
	{
		S_StartSound( &door->sector->soundorg, DoorSoundFor( door->direction, door->blazing ) );
	}
}

int32_t EV_DoDoorGeneric( line_t* line, mobj_t* activator )
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
			&& line->backsector->specialdata )
		{
			vldoor_t* door = thinker_cast< vldoor_t >( line->backsector->specialdata );
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

		if( !line->backsector->specialdata )
		{
			EV_DoDoorGeneric( line, line->backsector );
			return 1;
		}

		return 0;
	}

	int32_t secnum = -1;
	int32_t sectorsactivated = 0;
	while( (secnum = P_FindSectorFromLineTag(line,secnum)) >= 0 )
	{
		sector_t* sec = &sectors[secnum];
		if (sec->specialdata)
			continue;

		// new door thinker
		++sectorsactivated;
		EV_DoDoorGeneric( line, sec );
	}

	return sectorsactivated;
}

//
// EV_VerticalDoor : open a door manually, no tag value
//
doombool
EV_VerticalDoor
( line_t*	line,
  mobj_t*	thing )
{
    player_t*	player;
    sector_t*	sec;
    vldoor_t*	door;
    int		side;
	
    side = 0;	// only front sides can be used

    //	Check for locks
    player = thing->player;
		
    switch(line->special)
    {
      case 26: // Blue Lock
      case 32:
	if ( !player )
	    return false;
	
	if (!player->cards[it_bluecard] && !player->cards[it_blueskull])
	{
	    player->message = DEH_String(PD_BLUEK);
	    S_StartSound(NULL,sfx_oof);
	    return false;
	}
	break;
	
      case 27: // Yellow Lock
      case 34:
	if ( !player )
	    return false;
	
	if (!player->cards[it_yellowcard] &&
	    !player->cards[it_yellowskull])
	{
	    player->message = DEH_String(PD_YELLOWK);
	    S_StartSound(NULL,sfx_oof);
	    return false;
	}
	break;
	
      case 28: // Red Lock
      case 33:
	if ( !player )
	    return false;
	
	if (!player->cards[it_redcard] && !player->cards[it_redskull])
	{
	    player->message = DEH_String(PD_REDK);
	    S_StartSound(NULL,sfx_oof);
	    return false;
	}
	break;
    }
	
    // if the sector has an active thinker, use it

    if (line->sidenum[side^1] == -1)
    {
        I_Error("EV_VerticalDoor: DR special type on 1-sided linedef");
    }

    sec = sides[ line->sidenum[side^1]] .sector;

    if (sec->specialdata)
    {
		door = (vldoor_t*)sec->specialdata;
		switch(line->special)
		{
		  case	1: // ONLY FOR "RAISE" DOORS, NOT "OPEN"s
		  case	26:
		  case	27:
		  case	28:
		  case	117:
			if (door->direction == -1)
			door->direction = sd_open;	// go back up
			else
			{
			if (!thing->player)
				return false;		// JDC: bad guys never close doors

					// When is a door not a door?
					// In Vanilla, door->direction is set, even though
					// "specialdata" might not actually point at a door.

					if (door->thinker.function.acp1 == (actionf_p1) T_VerticalDoor)
					{
						door->direction = sd_close;	// start going down immediately
					}
					else if (door->thinker.function.acp1 == (actionf_p1) T_PlatRaise)
					{
						// Erm, this is a plat, not a door.
						// This notably causes a problem in ep1-0500.lmp where
						// a plat and a door are cross-referenced; the door
						// doesn't open on 64-bit.
						// The direction field in vldoor_t corresponds to the wait
						// field in plat_t.  Let's set that to -1 instead.

						plat_t *plat;

						plat = (plat_t *) door;
						plat->wait = -1;
					}
					else
					{
						// This isn't a door OR a plat.  Now we're in trouble.

						fprintf(stderr, "EV_VerticalDoor: Tried to close "
										"something that wasn't a door.\n");

						// Try closing it anyway. At least it will work on 32-bit
						// machines.

						door->direction = sd_close;
					}
			}
			return true;
		}
    }
	
    // for proper sound
    switch(line->special)
    {
	case 117:	// BLAZING DOOR RAISE
	case 118:	// BLAZING DOOR OPEN
		S_StartSound(&sec->soundorg,sfx_bdopn);
		break;
	
	case 1:	// NORMAL DOOR SOUND
	case 31:
		S_StartSound(&sec->soundorg,sfx_doropn);
		break;
	
	default:	// LOCKED DOOR SOUND
		S_StartSound(&sec->soundorg,sfx_doropn);
		break;
    }

    // new door thinker
    door = (vldoor_t*)Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
    P_AddThinker (&door->thinker);
    sec->specialdata = door;
    door->thinker.function.acp1 = (actionf_p1) T_VerticalDoor;
    door->sector = sec;
    door->direction = sd_open;
    door->speed = VDOORSPEED;
    door->topwait = VDOORWAIT;

    switch(line->special)
    {
	case 1:
	case 26:
	case 27:
	case 28:
		door->type = vld_normal;
		break;

	case 31:
	case 32:
	case 33:
	case 34:
		door->type = vld_open;
		line->special = 0;
		break;

	case 117:	// blazing door raise
		door->type = vld_blazeRaise;
		door->speed = VDOORSPEED*4;
		break;

	case 118:	// blazing door open
		door->type = vld_blazeOpen;
		line->special = 0;
		door->speed = VDOORSPEED*4;
		break;
	}

	// find the top and bottom of the movement range
	door->topheight = P_FindLowestCeilingSurrounding(sec);
	door->topheight -= 4*FRACUNIT;

	return true;
}


//
// Spawn a door that closes after 30 seconds
//
void P_SpawnDoorCloseIn30 (sector_t* sec)
{
    vldoor_t*	door;
	
    door = (vldoor_t*)Z_Malloc( sizeof(*door), PU_LEVSPEC, 0);

    P_AddThinker (&door->thinker);

    sec->specialdata = door;
    sec->special = 0;

    door->thinker.function.acp1 = (actionf_p1)T_VerticalDoor;
    door->sector = sec;
    door->direction = sd_none;
    door->type = vld_normal;
    door->speed = VDOORSPEED;
    door->topcountdown = 30 * TICRATE;
}

//
// Spawn a door that opens after 5 minutes
//
void
P_SpawnDoorRaiseIn5Mins
( sector_t*	sec,
  int		secnum )
{
    vldoor_t*	door;
	
    door = (vldoor_t*)Z_Malloc( sizeof(*door), PU_LEVSPEC, 0 );
    
    P_AddThinker (&door->thinker);

    sec->specialdata = door;
    sec->special = 0;

    door->thinker.function.acp1 = (actionf_p1)T_VerticalDoor;
    door->sector = sec;
    door->direction = sd_raisein5nonsense;
    door->type = vld_raiseIn5Mins;
    door->speed = VDOORSPEED;
    door->topheight = P_FindLowestCeilingSurrounding(sec);
    door->topheight -= 4*FRACUNIT;
    door->topwait = VDOORWAIT;
    door->topcountdown = 5 * 60 * TICRATE;
}



// UNUSED
// Separate into p_slidoor.c?

#if 0		// ABANDONED TO THE MISTS OF TIME!!!
//
// EV_SlidingDoor : slide a door horizontally
// (animate midtexture, then set noblocking line)
//


slideframe_t slideFrames[MAXSLIDEDOORS];

void P_InitSlidingDoorFrames(void)
{
    int		i;
    int		f1;
    int		f2;
    int		f3;
    int		f4;
	
    // DOOM II ONLY...
    if ( gamemode != commercial)
	return;
	
    for (i = 0;i < MAXSLIDEDOORS; i++)
    {
	if (!slideFrameNames[i].frontFrame1[0])
	    break;
			
	f1 = R_TextureNumForName(slideFrameNames[i].frontFrame1);
	f2 = R_TextureNumForName(slideFrameNames[i].frontFrame2);
	f3 = R_TextureNumForName(slideFrameNames[i].frontFrame3);
	f4 = R_TextureNumForName(slideFrameNames[i].frontFrame4);

	slideFrames[i].frontFrames[0] = f1;
	slideFrames[i].frontFrames[1] = f2;
	slideFrames[i].frontFrames[2] = f3;
	slideFrames[i].frontFrames[3] = f4;
		
	f1 = R_TextureNumForName(slideFrameNames[i].backFrame1);
	f2 = R_TextureNumForName(slideFrameNames[i].backFrame2);
	f3 = R_TextureNumForName(slideFrameNames[i].backFrame3);
	f4 = R_TextureNumForName(slideFrameNames[i].backFrame4);

	slideFrames[i].backFrames[0] = f1;
	slideFrames[i].backFrames[1] = f2;
	slideFrames[i].backFrames[2] = f3;
	slideFrames[i].backFrames[3] = f4;
    }
}


//
// Return index into "slideFrames" array
// for which door type to use
//
int P_FindSlidingDoorType(line_t*	line)
{
    int		i;
    int		val;
	
    for (i = 0;i < MAXSLIDEDOORS;i++)
    {
	val = sides[line->sidenum[0]].midtexture;
	if (val == slideFrames[i].frontFrames[0])
	    return i;
    }
	
    return -1;
}

void T_SlidingDoor (slidedoor_t*	door)
{
    switch(door->status)
    {
      case sd_opening:
	if (!door->timer--)
	{
	    if (++door->frame == SNUMFRAMES)
	    {
		// IF DOOR IS DONE OPENING...
		sides[door->line->sidenum[0]].midtexture = 0;
		sides[door->line->sidenum[1]].midtexture = 0;
		door->line->flags &= ML_BLOCKING^0xff;
					
		if (door->type == sdt_openOnly)
		{
		    door->frontsector->specialdata = NULL;
		    P_RemoveThinker (&door->thinker);
		    break;
		}
					
		door->timer = SDOORWAIT;
		door->status = sd_waiting;
	    }
	    else
	    {
		// IF DOOR NEEDS TO ANIMATE TO NEXT FRAME...
		door->timer = SWAITTICS;
					
		sides[door->line->sidenum[0]].midtexture =
		    slideFrames[door->whichDoorIndex].
		    frontFrames[door->frame];
		sides[door->line->sidenum[1]].midtexture =
		    slideFrames[door->whichDoorIndex].
		    backFrames[door->frame];
	    }
	}
	break;
			
      case sd_waiting:
	// IF DOOR IS DONE WAITING...
	if (!door->timer--)
	{
	    // CAN DOOR CLOSE?
	    if (door->frontsector->thinglist != NULL ||
		door->backsector->thinglist != NULL)
	    {
		door->timer = SDOORWAIT;
		break;
	    }

	    //door->frame = SNUMFRAMES-1;
	    door->status = sd_closing;
	    door->timer = SWAITTICS;
	}
	break;
			
      case sd_closing:
	if (!door->timer--)
	{
	    if (--door->frame < 0)
	    {
		// IF DOOR IS DONE CLOSING...
		door->line->flags |= ML_BLOCKING;
		door->frontsector->specialdata = NULL;
		P_RemoveThinker (&door->thinker);
		break;
	    }
	    else
	    {
		// IF DOOR NEEDS TO ANIMATE TO NEXT FRAME...
		door->timer = SWAITTICS;
					
		sides[door->line->sidenum[0]].midtexture =
		    slideFrames[door->whichDoorIndex].
		    frontFrames[door->frame];
		sides[door->line->sidenum[1]].midtexture =
		    slideFrames[door->whichDoorIndex].
		    backFrames[door->frame];
	    }
	}
	break;
    }
}



void
EV_SlidingDoor
( line_t*	line,
  mobj_t*	thing )
{
    sector_t*		sec;
    slidedoor_t*	door;
	
    // DOOM II ONLY...
    if (gamemode != commercial)
	return;
    
    // Make sure door isn't already being animated
    sec = line->frontsector;
    door = NULL;
    if (sec->specialdata)
    {
	if (!thing->player)
	    return;
			
	door = sec->specialdata;
	if (door->type == sdt_openAndClose)
	{
	    if (door->status == sd_waiting)
		door->status = sd_closing;
	}
	else
	    return;
    }
    
    // Init sliding door vars
    if (!door)
    {
	door = Z_Malloc (sizeof(*door), PU_LEVSPEC, 0);
	P_AddThinker (&door->thinker);
	sec->specialdata = door;
		
	door->type = sdt_openAndClose;
	door->status = sd_opening;
	door->whichDoorIndex = P_FindSlidingDoorType(line);

	if (door->whichDoorIndex < 0)
	    I_Error("EV_SlidingDoor: Can't use texture for sliding door!");
			
	door->frontsector = sec;
	door->backsector = line->backsector;
	door->thinker.function = T_SlidingDoor;
	door->timer = SWAITTICS;
	door->frame = 0;
	door->line = line;
    }
}
#endif
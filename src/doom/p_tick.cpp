//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020 Ethan Watson
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
//	Archiving: SaveGame I/O.
//	Thinker, Ticker.
//


#include "z_zone.h"
#include "p_local.h"

#include "doomstat.h"


int	leveltime;

//
// THINKERS
// All thinkers should be allocated by Z_Malloc
// so they can be operated on uniformly.
// The actual structures will vary in size,
// but the first element must be thinker_t.
//



// Both the head and tail of the thinker list.
extern "C"
{
	thinker_t	thinkercap;
}


//
// P_InitThinkers
//
DOOM_C_API void P_InitThinkers (void)
{
    thinkercap.prev = thinkercap.next  = &thinkercap;
}




//
// P_AddThinker
// Adds a new thinker at the end of the list.
//
DOOM_C_API void P_AddThinker (thinker_t* thinker)
{
    thinkercap.prev->next = thinker;
    thinker->next = &thinkercap;
    thinker->prev = thinkercap.prev;
    thinkercap.prev = thinker;
}



//
// P_RemoveThinker
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
DOOM_C_API void P_RemoveThinker (thinker_t* thinker)
{
	thinker->function = nullptr;
}

//
// P_RunThinkers
//
void P_RunThinkers (void)
{
    thinker_t *currentthinker, *nextthinker;

    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
		nextthinker = currentthinker->next;
		if ( !currentthinker->function.Valid() )
		{
			// time to remove it
			currentthinker->next->prev = currentthinker->prev;
			currentthinker->prev->next = currentthinker->next;
			Z_Free(currentthinker);
		}
		else if( currentthinker->function.Enabled() )
		{
			currentthinker->function(currentthinker);
		}
		currentthinker = nextthinker;
    }
}

void P_FlipInstanceData( void )
{
	sectorinstance_t* tempsec = currsectors;
	currsectors = prevsectors;
	prevsectors = tempsec;

	sideinstance_t* tempside = currsides;
	currsides = prevsides;
	prevsides = tempside;

	for( thinker_t* th = thinkercap.next; th != &thinkercap; th = th->next )
	{
		if( mobj_t* mobj = thinker_cast< mobj_t >( th ) )
		{
			mobj->prev = mobj->curr;
		}
	}
}

DOOM_C_API void P_UpdateInstanceData( void )
{
	int32_t index = 0;

	sector_t* thissec = sectors;
	sectorinstance_t* thissecinst = currsectors;
	
	for( index = 0; index < numsectors; ++index )
	{
		thissecinst->floortex			= flatlookup[ flattranslation[ thissec->floorpic ] ];
		thissecinst->ceiltex			= flatlookup[ flattranslation[ thissec->ceilingpic ] ];
		thissecinst->floorheight		= thissecinst->midtexfloor = FixedToRendFixed( thissec->floorheight );
		thissecinst->ceilheight			= thissecinst->midtexceil = FixedToRendFixed( thissec->ceilingheight );
		thissecinst->lightlevel			= thissec->lightlevel;
		thissecinst->floorlightlevel	= thissec->floorlightsec->lightlevel;
		thissecinst->ceillightlevel		= thissec->ceilinglightsec->lightlevel;
		thissecinst->flooroffsetx		= FixedToRendFixed( thissec->flooroffsetx );
		thissecinst->flooroffsety		= FixedToRendFixed( thissec->flooroffsety );
		thissecinst->ceiloffsetx		= FixedToRendFixed( thissec->ceiloffsetx );
		thissecinst->ceiloffsety		= FixedToRendFixed( thissec->ceiloffsety );
		thissecinst->snapfloor			= thissec->snapfloor;
		thissecinst->snapceiling		= thissec->snapceiling;
		thissecinst->activethisframe	= thissec->lastactivetic == gametic
										|| thissec->floorlightsec->lastactivetic == gametic
										|| thissec->ceilinglightsec->lastactivetic == gametic;

		++thissec;
		++thissecinst;
	}

	side_t* thisside = sides;
	sideinstance_t* thissideinst = currsides;

	for( index = 0; index < numsides; ++index )
	{
		thissideinst->toptex		= thisside->toptexture ? texturelookup[ texturetranslation[ thisside->toptexture ] ] : NULL;
		thissideinst->midtex		= thisside->midtexture ? texturelookup[ texturetranslation[ thisside->midtexture ] ] : NULL;
		thissideinst->bottomtex		= thisside->bottomtexture ? texturelookup[ texturetranslation[ thisside->bottomtexture ] ] : NULL;
		thissideinst->coloffset		= FixedToRendFixed( thisside->textureoffset );
		thissideinst->rowoffset		= FixedToRendFixed( thisside->rowoffset );

		++thisside;
		++thissideinst;
	}

	for( thinker_t* th = thinkercap.next; th != &thinkercap; th = th->next )
	{
		if( mobj_t* mobj = thinker_cast< mobj_t >( th ) )
		{
			mobj->curr.x = FixedToRendFixed( mobj->x );
			mobj->curr.y = FixedToRendFixed( mobj->y );
			mobj->curr.z = FixedToRendFixed( mobj->z );
			mobj->curr.angle = mobj->angle;
			mobj->curr.sprite = mobj->sprite;
			mobj->curr.frame = mobj->frame;
			mobj->curr.teleported = mobj->teleporttic == gametic;
		}
	}
}

//
// P_Ticker
//

DOOM_C_API void P_Ticker (void)
{
    int		i;

	P_FlipInstanceData();
    
	doombool ispaused = paused
		|| 	( !demoplayback && dashboardactive && dashboardpausesplaysim && ( solonetgame || !netgame ) );

    // run the tic
    if (ispaused)
	{
		P_UpdateInstanceData();
		return;
	}
		
    // pause if in menu and at least one tic has been run
    if ( !netgame
	 && menuactive
	 && !demoplayback
	 && players[consoleplayer].viewz != 1)
	{
		P_UpdateInstanceData();
		return;
	}
    
		
    for (i=0 ; i<MAXPLAYERS ; i++)
	if (playeringame[i])
	{
		players[ i ].prevviewz = players[ i ].currviewz;
		players[ i ].mo->prev = players[ i ].mo->curr;

		P_PlayerThink (&players[i]);

		players[ i ].currviewz = FixedToRendFixed( players[ i ].viewz );
		players[ i ].mo->curr.x = FixedToRendFixed( players[ i ].mo->x );
		players[ i ].mo->curr.y = FixedToRendFixed( players[ i ].mo->y );
		players[ i ].mo->curr.z = FixedToRendFixed( players[ i ].mo->z );
		players[ i ].mo->curr.angle = players[ i ].mo->angle;
		players[ i ].mo->curr.sprite = players[ i ].mo->sprite;
		players[ i ].mo->curr.frame = players[ i ].mo->frame;
	}

    P_RunThinkers ();
    P_UpdateSpecials ();
    P_RespawnSpecials ();

	P_UpdateInstanceData();

    // for par times
    leveltime++;
	session.level_time = leveltime;
	++session.session_time;
}

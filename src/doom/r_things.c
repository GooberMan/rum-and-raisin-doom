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
//	Refresh of things, i.e. objects represented by sprites.
//




#include <stdio.h>
#include <stdlib.h>


#include "deh_main.h"
#include "doomdef.h"

#include "i_swap.h"
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"
#include "m_misc.h"

#include "r_local.h"

#include "doomstat.h"

#define MINZ				( IntToFixed( 4 ) )
#define BASEYCENTER			(V_VIRTUALHEIGHT/2)

// Constant arrays, don't need to live in a context

//  used for psprite clipping and initializing clipping
vertclip_t		negonearray[ MAXSCREENWIDTH ];
vertclip_t		screenheightarray[ MAXSCREENWIDTH ];

fixed_t			pspritescale;
fixed_t			pspriteiscale;

//
// INITIALIZATION FUNCTIONS
//

// variables used to look up
//  and range check thing_t sprites patches
spritedef_t*	sprites;
int32_t			numsprites;

// Only used in initialisation? Might be safe to keep on stack
spriteframe_t	sprtemp[29];
int32_t			maxframe;
const char		*spritename;


//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
void
R_InstallSpriteLump
( int		lump,
  unsigned	frame,
  unsigned	rotation,
  boolean	flipped )
{
    int		r;
	
    if (frame >= 29 || rotation > 8)
	I_Error("R_InstallSpriteLump: "
		"Bad frame characters in lump %i", lump);
	
    if ((int)frame > maxframe)
	maxframe = frame;
		
    if (rotation == 0)
    {
	// the lump should be used for all rotations
	if (sprtemp[frame].rotate == false)
	    I_Error ("R_InitSprites: Sprite %s frame %c has "
		     "multip rot=0 lump", spritename, 'A'+frame);

	if (sprtemp[frame].rotate == true)
	    I_Error ("R_InitSprites: Sprite %s frame %c has rotations "
		     "and a rot=0 lump", spritename, 'A'+frame);
			
	sprtemp[frame].rotate = false;
	for (r=0 ; r<8 ; r++)
	{
	    sprtemp[frame].lump[r] = lump - firstspritelump;
	    sprtemp[frame].flip[r] = (byte)flipped;
	}
	return;
    }
	
    // the lump is only used for one rotation
    if (sprtemp[frame].rotate == false)
	I_Error ("R_InitSprites: Sprite %s frame %c has rotations "
		 "and a rot=0 lump", spritename, 'A'+frame);
		
    sprtemp[frame].rotate = true;

    // make 0 based
    rotation--;		
    if (sprtemp[frame].lump[rotation] != -1)
	I_Error ("R_InitSprites: Sprite %s : %c : %c "
		 "has two lumps mapped to it",
		 spritename, 'A'+frame, '1'+rotation);
		
    sprtemp[frame].lump[rotation] = lump - firstspritelump;
    sprtemp[frame].flip[rotation] = (byte)flipped;
}




//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//  (4 chars exactly) to be used.
// Builds the sprite rotation matrixes to account
//  for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant. 
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//  a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//  letter/number appended.
// The rotation character can be 0 to signify no rotations.
//
void R_InitSpriteDefs(const char **namelist)
{ 
    const char **check;
    int		i;
    int		l;
    int		frame;
    int		rotation;
    int		start;
    int		end;
    int		patched;
		
    // count the number of sprite names
    check = namelist;
    while (*check != NULL)
	check++;

    numsprites = check-namelist;
	
    if (!numsprites)
	return;
		
    sprites = Z_Malloc(numsprites *sizeof(*sprites), PU_STATIC, NULL);
	
    start = firstspritelump-1;
    end = lastspritelump+1;
	
    // scan all the lump names for each of the names,
    //  noting the highest frame letter.
    // Just compare 4 characters as ints
    for (i=0 ; i<numsprites ; i++)
    {
	spritename = DEH_String(namelist[i]);
	memset (sprtemp,-1, sizeof(sprtemp));
		
	maxframe = -1;
	
	// scan the lumps,
	//  filling in the frames for whatever is found
	for (l=start+1 ; l<end ; l++)
	{
	    if (!strncasecmp(lumpinfo[l]->name, spritename, 4))
	    {
		frame = lumpinfo[l]->name[4] - 'A';
		rotation = lumpinfo[l]->name[5] - '0';

		if (modifiedgame)
		    patched = W_GetNumForName (lumpinfo[l]->name);
		else
		    patched = l;

		R_InstallSpriteLump (patched, frame, rotation, false);

		if (lumpinfo[l]->name[6])
		{
		    frame = lumpinfo[l]->name[6] - 'A';
		    rotation = lumpinfo[l]->name[7] - '0';
		    R_InstallSpriteLump (l, frame, rotation, true);
		}
	    }
	}
	
	// check the frames that were found for completeness
	if (maxframe == -1)
	{
	    sprites[i].numframes = 0;
	    continue;
	}
		
	maxframe++;
	
	for (frame = 0 ; frame < maxframe ; frame++)
	{
	    switch ((int)sprtemp[frame].rotate)
	    {
	      case -1:
		// no rotations were found for that frame at all
		I_Error ("R_InitSprites: No patches found "
			 "for %s frame %c", spritename, frame+'A');
		break;
		
	      case 0:
		// only the first rotation is needed
		break;
			
	      case 1:
		// must have all 8 frames
		for (rotation=0 ; rotation<8 ; rotation++)
		    if (sprtemp[frame].lump[rotation] == -1)
			I_Error ("R_InitSprites: Sprite %s frame %c "
				 "is missing rotations",
				 spritename, frame+'A');
		break;
	    }
	}
	
	// allocate space for the frames present and copy sprtemp to it
	sprites[i].numframes = maxframe;
	sprites[i].spriteframes = 
	    Z_Malloc (maxframe * sizeof(spriteframe_t), PU_STATIC, NULL);
	memcpy (sprites[i].spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));
    }

}




//
// R_InitSprites
// Called at program start.
//
void R_InitSprites(const char **namelist)
{
    vertclip_t		i;
	
    for (i=0 ; i<MAXSCREENWIDTH ; i++)
    {
		negonearray[i] = -1;
    }
	
    R_InitSpriteDefs (namelist);
}



//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites ( spritecontext_t* spritecontext )
{
	spritecontext->nextvissprite = spritecontext->vissprites;
}


vissprite_t* R_NewVisSprite ( spritecontext_t* spritecontext )
{
	if ( spritecontext->nextvissprite < &spritecontext->vissprites[ MAXVISSPRITES ] )
	{
		return spritecontext->nextvissprite++;
	}

	return spritecontext->nextvissprite;
}


//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//

void R_DrawMaskedColumn( spritecontext_t* spritecontext, colcontext_t* colcontext, column_t* column )
{
	rend_fixed_t		topscreen;
	rend_fixed_t		bottomscreen;
	rend_fixed_t		basetexturemid;
	boolean				isfuzz = colcontext->colormap == NULL;
	
	basetexturemid = colcontext->texturemid;
	
	for ( ; column->topdelta != 0xff ; ) 
	{
		// calculate unclipped screen coordinates
		//  for post
		topscreen = spritecontext->sprtopscreen + spritecontext->spryscale*column->topdelta;
		bottomscreen = topscreen + spritecontext->spryscale*column->length;

		colcontext->yl = RendFixedToInt( topscreen + FRACUNIT - 1 );
		colcontext->yh = RendFixedToInt( bottomscreen - 1 );

		if( isfuzz )
		{
			colcontext->yl = M_MAX( colcontext->yl, 1 );
			colcontext->yh = M_MIN( colcontext->yh, viewheight - 1 );
		}

		if (colcontext->yh >= spritecontext->mfloorclip[colcontext->x])
			colcontext->yh = spritecontext->mfloorclip[colcontext->x]-1;
		if (colcontext->yl <= spritecontext->mceilingclip[colcontext->x])
			colcontext->yl = spritecontext->mceilingclip[colcontext->x]+1;

		if (colcontext->yl < colcontext->yh)
		{
			colcontext->source = (byte *)column + 3;
			colcontext->texturemid = basetexturemid - IntToRendFixed( column->topdelta );
			// colcontext->source = (byte *)column + 3 - column->topdelta;

			// Drawn by either R_DrawColumn
			//  or (SHADOW) R_DrawFuzzColumn.
			colcontext->colfunc( colcontext );	
		}
		column = (column_t *)(  (byte *)column + column->length + 4);
	}
	
	colcontext->texturemid = basetexturemid;
}


#define FUZZ_X_RATIO ( ( render_width * 100 ) / 320 )
//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
void R_DrawVisSprite( vbuffer_t* dest, spritecontext_t* spritecontext, vissprite_t* vis, int32_t x1, int32_t x2 )
{
	column_t*			column;
	int32_t				texturecolumn;
	int32_t				prevfuzzcolumn;
	fixed_t				frac;
	patch_t*			patch;
	boolean				isfuzz = false;
	int32_t				fuzzcolumn;

	colcontext_t		spritecolcontext;

	patch = spritepatches[ vis->patch ];

	spritecolcontext.output = *dest;
	spritecolcontext.colormap = vis->colormap;
	spritecolcontext.colfunc = &R_DrawColumn_Untranslated; 

	if (!spritecolcontext.colormap)
	{
		// NULL colormap = shadow draw
		spritecolcontext.colfunc = colfuncs[ COLFUNC_FUZZBASEINDEX + fuzz_style ];
		isfuzz = true;
	}
	else if (vis->mobjflags & MF_TRANSLATION)
	{
		spritecolcontext.colfunc = transcolfunc;
		spritecolcontext.translation = translationtables - 256 +
			( (vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT-8) );
	}
	
	spritecolcontext.scale = FixedToRendFixed( abs(vis->xscale)>>detailshift );
	spritecolcontext.iscale = FixedToRendFixed( abs(vis->xiscale)>>detailshift );
	spritecolcontext.texturemid = FixedToRendFixed( vis->texturemid );
	frac = vis->startfrac;
	spritecontext->spryscale = FixedToRendFixed( vis->scale );
	spritecontext->sprtopscreen = FixedToRendFixed( centeryfrac ) - RendFixedMul( spritecolcontext.texturemid, spritecontext->spryscale );
	
	prevfuzzcolumn = -1;
	for (spritecolcontext.x=vis->x1 ; spritecolcontext.x<=vis->x2 ; spritecolcontext.x++, frac += vis->xiscale)
	{
		texturecolumn = FixedToInt( frac );
#ifdef RANGECHECK
		if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
			I_Error ("R_DrawSpriteRange: bad texturecolumn");
#endif

		if( fuzz_style != Fuzz_Original )
		{
			fuzzcolumn = ( spritecolcontext.x * 100 ) / FUZZ_X_RATIO;
			if(prevfuzzcolumn != fuzzcolumn)
			{
				R_CacheFuzzColumn();
				prevfuzzcolumn = fuzzcolumn;
			}
		}

		column = (column_t *)( (byte *)patch + LONG(patch->columnofs[texturecolumn]) );
		R_DrawMaskedColumn( spritecontext, &spritecolcontext, column );
	}
}



//
// R_ProjectSprite
// Generates a vissprite for a thing
//  if it might be visible.
//
void R_ProjectSprite ( spritecontext_t* spritecontext, mobj_t* thing)
{
	fixed_t			tr_x;
	fixed_t			tr_y;

	fixed_t			gxt;
	fixed_t			gyt;

	fixed_t			tx;
	fixed_t			tz;

	fixed_t			xscale;

	int32_t			x1;
	int32_t			x2;

	spritedef_t*	sprdef;
	spriteframe_t*	sprframe;
	int32_t			lump;

	uint32_t		rot;
	boolean			flip;

	int32_t			index;

	vissprite_t*	vis;

	angle_t			ang;
	fixed_t			iscale;

	// transform the origin point
	tr_x = thing->x - viewx;
	tr_y = thing->y - viewy;
	
	gxt = FixedMul(tr_x,viewcos); 
	gyt = -FixedMul(tr_y,viewsin);

	tz = gxt-gyt; 

	// thing is behind view plane?
	if (tz < MINZ)
	{
		return;
	}

	xscale = FixedDiv( RendFixedToFixed( projection ), tz);
	
	gxt = -FixedMul(tr_x,viewsin); 
	gyt = FixedMul(tr_y,viewcos); 
	tx = -(gyt+gxt); 

	// too far off the side?
	if (abs(tx)>(tz<<2))
	{
		return;
	}

	// decide which patch to use for sprite relative to player
#ifdef RANGECHECK
	if ((uint32_t) thing->sprite >= (uint32_t) numsprites)
	{
		I_Error ("R_ProjectSprite: invalid sprite number %i ",
				thing->sprite);
	}
#endif

	sprdef = &sprites[thing->sprite];

#ifdef RANGECHECK
	if ( (thing->frame&FF_FRAMEMASK) >= sprdef->numframes )
	{
		I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ",
				thing->sprite, thing->frame);
	}
#endif

	sprframe = &sprdef->spriteframes[ thing->frame & FF_FRAMEMASK];

	if (sprframe->rotate)
	{
		// choose a different rotation based on player view
		ang = R_PointToAngle (thing->x, thing->y);
		rot = (ang-thing->angle+(unsigned)(ANG45/2)*9)>>29;
		lump = sprframe->lump[rot];
		flip = (boolean)sprframe->flip[rot];
	}
	else
	{
		// use single rotation for all views
		lump = sprframe->lump[0];
		flip = (boolean)sprframe->flip[0];
	}

	// calculate edges of the shape
	tx -= spriteoffset[lump];
	x1 = FixedToInt( centerxfrac + FixedMul( tx, xscale ) );

	// off the right side?
	if (x1 > spritecontext->rightclip)
	{
		return;
	}

	tx += spritewidth[lump];
	x2 = FixedToInt( centerxfrac + FixedMul( tx, xscale) ) - 1;

	// off the left side
	if (x2 < spritecontext->leftclip)
	{
		return;
	}

	// store information in a vissprite
	vis = R_NewVisSprite( spritecontext );
	vis->mobjflags = thing->flags;
	vis->scale = xscale<<detailshift;
	vis->gx = thing->x;
	vis->gy = thing->y;
	vis->gz = thing->z;
	vis->gzt = thing->z + spritetopoffset[lump];
	vis->texturemid = vis->gzt - viewz;
	vis->x1 = x1 < spritecontext->leftclip ? spritecontext->leftclip : x1;
	vis->x2 = x2 >= spritecontext->rightclip ? spritecontext->rightclip-1 : x2;	
	iscale = FixedDiv (FRACUNIT, xscale);

	if (flip)
	{
		vis->startfrac = spritewidth[lump]-1;
		vis->xscale = -xscale;
		vis->xiscale = -iscale;
	}
	else
	{
		vis->startfrac = 0;
		vis->xscale = xscale;
		vis->xiscale = iscale;
	}

	if (vis->x1 > x1)
	{
		vis->startfrac += vis->xiscale*(vis->x1-x1);
	}
	vis->patch = lump;

	// get light level
	if (thing->flags & MF_SHADOW)
	{
		// shadow draw
		vis->colormap = NULL;
	}
	else if (fixedcolormap)
	{
		// fixed map
		vis->colormap = fixedcolormap;
	}
	else if (thing->frame & FF_FULLBRIGHT)
	{
		// full bright
		vis->colormap = colormaps;
	}
	else
	{
		// diminished light
		index = xscale>>(LIGHTSCALESHIFT-detailshift);
		if( LIGHTSCALEMUL != FRACUNIT )
		{
			index = RendFixedToInt( RendFixedMul( IntToRendFixed( index ), LIGHTSCALEMUL ) );
		}

		if (index >= MAXLIGHTSCALE) 
			index = MAXLIGHTSCALE-1;

		vis->colormap = spritecontext->spritelights[index];
	}	
}


//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
void R_AddSprites ( spritecontext_t* spritecontext, sector_t* sec)
{
	mobj_t*		thing;
	int			lightnum;

	// BSP is traversed by subsector.
	// A sector might have been split into several
	//  subsectors during BSP building.
	// Thus we check whether its already added.
	if ( spritecontext->sectorvisited[ sec->index ] )
	{
		return;
	}

	// Well, now it will be done.
	spritecontext->sectorvisited[ sec->index ] = true;
	
	lightnum = (sec->lightlevel >> LIGHTSEGSHIFT)+extralight;

	if (lightnum < 0)
	{
		spritecontext->spritelights = scalelight[0];
	}
	else if (lightnum >= LIGHTLEVELS)
	{
		spritecontext->spritelights = scalelight[LIGHTLEVELS-1];
	}
	else
	{
		spritecontext->spritelights = scalelight[lightnum];
	}

	// Handle all things in sector.
	for (thing = sec->thinglist ; thing ; thing = thing->snext)
	{
		R_ProjectSprite( spritecontext, thing );
	}
}


//
// R_DrawPSprite
//
void R_DrawPSprite ( vbuffer_t* dest, spritecontext_t* spritecontext, pspdef_t* psp )
{
	fixed_t			tx;
	int32_t			x1;
	int32_t			x2;
	spritedef_t*	sprdef;
	spriteframe_t*	sprframe;
	int32_t			lump;
	boolean			flip;
	vissprite_t*	vis;
	vissprite_t		avis;

    // decide which patch to use
#ifdef RANGECHECK
    if ( (uint32_t)psp->state->sprite >= (uint32_t) numsprites)
	{
		I_Error ("R_ProjectSprite: invalid sprite number %i ",
			 psp->state->sprite);
	}
#endif
	sprdef = &sprites[psp->state->sprite];
#ifdef RANGECHECK
	if ( (psp->state->frame & FF_FRAMEMASK)  >= sprdef->numframes)
	{
		I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ",
			 psp->state->sprite, psp->state->frame);
	}
#endif
	sprframe = &sprdef->spriteframes[ psp->state->frame & FF_FRAMEMASK ];

	lump = sprframe->lump[0];
	flip = (boolean)sprframe->flip[0];

	// calculate edges of the shape
	tx = psp->sx - IntToFixed( V_VIRTUALWIDTH / 2 );
	
	tx -= spriteoffset[lump];	
	x1 = FixedToInt( centerxfrac + FixedMul( tx, pspritescale ) );

	// off the right side
	if (x1 > spritecontext->rightclip)
	{
		return;
	}

	tx +=  spritewidth[lump];
	x2 = FixedToInt( centerxfrac + FixedMul( tx, pspritescale ) ) - 1;

	// off the left side
	if (x2 < spritecontext->leftclip)
	{
		return;
	}

	// store information in a vissprite
	vis = &avis;
	vis->mobjflags = 0;
	vis->texturemid = IntToFixed( BASEYCENTER ) + FRACUNIT / 2 -( psp->sy - spritetopoffset[ lump ] );
	vis->x1 = x1 < spritecontext->leftclip ? spritecontext->leftclip : x1;
	vis->x2 = x2 >= spritecontext->rightclip ? spritecontext->rightclip-1 : x2;	
	vis->scale = pspritescale<<detailshift;

	if (flip)
	{
		vis->xscale = -pspritescale;
		vis->xiscale = -pspriteiscale;
		vis->startfrac = spritewidth[lump]-1;
	}
	else
	{
		vis->xscale = pspritescale;
		vis->xiscale = pspriteiscale;
		vis->startfrac = 0;
	}

	if (vis->x1 > x1)
	{
		vis->startfrac += vis->xiscale*(vis->x1-x1);
	}

	vis->patch = lump;

	if (viewplayer->powers[pw_invisibility] > 4*32
		|| viewplayer->powers[pw_invisibility] & 8)
	{
		// shadow draw
		vis->colormap = NULL;
	}
	else if (fixedcolormap)
	{
		// fixed color
		vis->colormap = fixedcolormap;
	}
	else if (psp->state->frame & FF_FULLBRIGHT)
	{
		// full bright
		vis->colormap = colormaps;
	}
	else
	{
		// local light
		vis->colormap = spritecontext->spritelights[MAXLIGHTSCALE-1];
	}
	
	R_DrawVisSprite( dest, spritecontext, vis, vis->x1, vis->x2 );
}



//
// R_DrawPlayerSprites
//
void R_DrawPlayerSprites ( vbuffer_t* dest, spritecontext_t* spritecontext )
{
	int32_t		i;
	int32_t		lightnum;
	pspdef_t*	psp;

	// get light level
	lightnum = (viewplayer->mo->subsector->sector->lightlevel >> LIGHTSEGSHIFT) + extralight;

	if (lightnum < 0)
	{
		spritecontext->spritelights = scalelight[0];
	}
	else if (lightnum >= LIGHTLEVELS)
	{
		spritecontext->spritelights = scalelight[LIGHTLEVELS-1];
	}
	else
	{
		spritecontext->spritelights = scalelight[lightnum];
	}

	// clip to screen bounds
	spritecontext->mfloorclip = screenheightarray;
	spritecontext->mceilingclip = negonearray;

	// add all active psprites
	for (i=0, psp=viewplayer->psprites; i<NUMPSPRITES; i++,psp++)
	{
		if (psp->state)
		{
			R_DrawPSprite ( dest, spritecontext, psp );
		}
	}
}


//
// R_SortVisSprites
//

void R_SortVisSprites ( spritecontext_t* spritecontext )
{
	int32_t			i;
	int32_t			count;
	vissprite_t*	ds;
	vissprite_t*	best;
	vissprite_t		unsorted;
	fixed_t			bestscale;

	count = spritecontext->nextvissprite - spritecontext->vissprites;
	
	unsorted.next = unsorted.prev = &unsorted;

	if (!count)
	{
		return;
	}
		
	for ( ds = spritecontext->vissprites ; ds < spritecontext->nextvissprite; ds++)
	{
		ds->next = ds+1;
		ds->prev = ds-1;
	}

	spritecontext->vissprites[0].prev = &unsorted;
	unsorted.next = &spritecontext->vissprites[0];
	(spritecontext->nextvissprite -1 )->next = &unsorted;
	unsorted.prev = spritecontext->nextvissprite - 1;

	// pull the vissprites out by scale

	spritecontext->vsprsortedhead.next = spritecontext->vsprsortedhead.prev = &spritecontext->vsprsortedhead;
	for (i=0 ; i<count ; i++)
	{
		bestscale = INT_MAX;
		best = unsorted.next;
		for (ds=unsorted.next ; ds!= &unsorted ; ds=ds->next)
		{
			if (ds->scale < bestscale)
			{
				bestscale = ds->scale;
				best = ds;
			}
		}
		best->next->prev = best->prev;
		best->prev->next = best->next;
		best->next = &spritecontext->vsprsortedhead;
		best->prev = spritecontext->vsprsortedhead.prev;
		spritecontext->vsprsortedhead.prev->next = best;
		spritecontext->vsprsortedhead.prev = best;
	}
}


//
// R_DrawSprite
//
void R_DrawSprite( vbuffer_t* dest, spritecontext_t* spritecontext, bspcontext_t* bspcontext, vissprite_t* spr )
{
	drawseg_t*		ds;
	vertclip_t		clipbot[MAXSCREENWIDTH];
	vertclip_t		cliptop[MAXSCREENWIDTH];
	int32_t			x;
	int32_t			r1;
	int32_t			r2;
	fixed_t			scale;
	fixed_t			lowscale;
	int32_t			silhouette;

#if RENDER_PERF_GRAPHING
	uint64_t			starttime;
	uint64_t			endtime;
#endif // RENDER_PERF_GRAPHING

		
	for (x = spr->x1 ; x<=spr->x2 ; x++)
	{
		clipbot[x] = cliptop[x] = -2;
	}

	// Scan drawsegs from end to start for obscuring segs.
	// The first drawseg that has a greater scale
	//  is the clip seg.
	for (ds = bspcontext->thisdrawseg - 1 ; ds >= bspcontext->drawsegs ; ds--)
	{
		// determine if the drawseg obscures the sprite
		if (ds->x1 > spr->x2
			|| ds->x2 < spr->x1
			|| (!ds->silhouette
			&& !ds->maskedtexturecol) )
		{
			// does not cover sprite
			continue;
		}
			
		r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
		r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

		if (ds->scale1 > ds->scale2)
		{
			lowscale = ds->scale2;
			scale = ds->scale1;
		}
		else
		{
			lowscale = ds->scale1;
			scale = ds->scale2;
		}
		
		if (scale < spr->scale
			|| ( lowscale < spr->scale
				&& !R_PointOnSegSide (spr->gx, spr->gy, ds->curline) ) )
		{
			// masked mid texture?
			if (ds->maskedtexturecol)
			{
#if RENDER_PERF_GRAPHING
				starttime = I_GetTimeUS();
#endif // RENDER_PERF_GRAPHING
				R_RenderMaskedSegRange( dest, bspcontext, spritecontext, ds, r1, r2 );
#if RENDER_PERF_GRAPHING
				endtime = I_GetTimeUS();
				bspcontext->maskedtimetaken += ( endtime - starttime );
#endif // RENDER_PERF_GRAPHING
			}
			// seg is behind sprite
			continue;			
		}

	
		// clip this piece of the sprite
		silhouette = ds->silhouette;
	
		if (spr->gz >= ds->bsilheight)
			silhouette &= ~SIL_BOTTOM;

		if (spr->gzt <= ds->tsilheight)
			silhouette &= ~SIL_TOP;
			
		if (silhouette == 1)
		{
			// bottom sil
			for (x=r1 ; x<=r2 ; x++)
			if (clipbot[x] == -2)
				clipbot[x] = ds->sprbottomclip[x];
		}
		else if (silhouette == 2)
		{
			// top sil
			for (x=r1 ; x<=r2 ; x++)
			if (cliptop[x] == -2)
				cliptop[x] = ds->sprtopclip[x];
		}
		else if (silhouette == 3)
		{
			// both
			for (x=r1 ; x<=r2 ; x++)
			{
			if (clipbot[x] == -2)
				clipbot[x] = ds->sprbottomclip[x];
			if (cliptop[x] == -2)
				cliptop[x] = ds->sprtopclip[x];
			}
		}
		
	}

	// all clipping has been performed, so draw the sprite

	// check for unclipped columns
	for (x = spr->x1 ; x<=spr->x2 ; x++)
	{
		if (clipbot[x] == -2)
			clipbot[x] = viewheight;

		if (cliptop[x] == -2)
			cliptop[x] = -1;
	}
		
	spritecontext->mfloorclip = clipbot;
	spritecontext->mceilingclip = cliptop;
#if RENDER_PERF_GRAPHING
	starttime = I_GetTimeUS();
#endif // RENDER_PERF_GRAPHING
	R_DrawVisSprite( dest, spritecontext, spr, spr->x1, spr->x2 );
#if RENDER_PERF_GRAPHING
	endtime = I_GetTimeUS();
	spritecontext->maskedtimetaken += ( endtime - starttime );
#endif // RENDER_PERF_GRAPHING
}


//
// R_DrawMasked
//
void R_DrawMasked( vbuffer_t* dest, spritecontext_t* spritecontext, bspcontext_t* bspcontext )
{
	vissprite_t*	spr;
	drawseg_t*		ds;
	
	R_SortVisSprites( spritecontext );

	if ( spritecontext->nextvissprite > spritecontext->vissprites)
	{
		// draw all vissprites back to front
		for (spr = spritecontext->vsprsortedhead.next ;
				spr != &spritecontext->vsprsortedhead ;
				spr = spr->next)
		{
			R_DrawSprite( dest, spritecontext, bspcontext, spr );
		}
	}

	// render any remaining masked mid textures
	for ( ds = bspcontext->thisdrawseg - 1 ; ds >= bspcontext->drawsegs ; ds-- )
	{
		if (ds->maskedtexturecol)
		{
			R_RenderMaskedSegRange( dest, bspcontext, spritecontext, ds, ds->x1, ds->x2 );
		}
	}

	// draw the psprites on top of everything
	//  but does not draw on side views
	if (!viewangleoffset)
	{
		R_DrawPlayerSprites ( dest, spritecontext );
	}
}


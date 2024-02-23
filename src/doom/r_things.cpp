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

#include "doomdef.h"
#include "m_fixed.h"

#include "deh_main.h"

#include "i_system.h"

#include "m_profile.h"
#include "m_misc.h"

#include "p_local.h"

#include "r_main.h"
#include "r_local.h"

#include "w_wad.h"

#include "z_zone.h"

extern "C"
{
	#include "doomstat.h"

	#include "i_swap.h"


	#define MINZ				( IntToRendFixed( 4 ) )
	#define BASEYCENTER			(V_VIRTUALHEIGHT/2)

	extern int32_t interpolate_this_frame;
	extern int32_t view_bobbing_percent;

	// Constant arrays, don't need to live in a context

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
}


//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
void
R_InstallSpriteLump
( int32_t	lump,
  unsigned	frame,
  unsigned	rotation,
  doombool	flipped )
{
    int		r;
	
    //if (frame >= 29 || rotation > 8)
	//I_Error("R_InstallSpriteLump: "
	//	"Bad frame characters in lump %i", lump);
	
    if ((int)frame > maxframe)
	{
		maxframe = frame;
	}
		
    if (rotation == 0)
    {
		// the lump should be used for all rotations
		if (sprtemp[frame].rotate == 0)
		{
			I_Error ("R_InitSprites: Sprite %s frame %c has "
				 "multip rot=0 lump", spritename, 'A'+frame);
		}

		if (sprtemp[frame].rotate == 1)
		{
			if( !comp.additive_data_blocks )
			{
				I_Error ("R_InitSprites: Sprite %s frame %c has rotations "
					 "and a rot=0 lump", spritename, 'A'+frame);
			}
		}
			
		sprtemp[frame].rotate = 0;
		for (r=0 ; r<8 ; r++)
		{
			sprtemp[frame].lump[r] = lump;
			sprtemp[frame].flip[r] = (byte)flipped;
		}
		return;
    }

	// the lump is only used for one rotation
	if (sprtemp[frame].rotate == 0)
	{
		if( !comp.additive_data_blocks )
		{
			I_Error ("R_InitSprites: Sprite %s frame %c has rotations "
				 "and a rot=0 lump", spritename, 'A'+frame);
		}
	}

    sprtemp[frame].rotate = 1;

    // make 0 based
    rotation--;
    if( sprtemp[frame].lump[rotation] != -1 )
	{
		if( !comp.additive_data_blocks )
		{
			I_Error ("R_InitSprites: Sprite %s : %c : %c "
				 "has two lumps mapped to it",
				 spritename, 'A'+frame, '1'+rotation);
		}
	}

	sprtemp[frame].lump[rotation] = lump;
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
    int		frame;
    int		rotation;

    // count the number of sprite names
    check = namelist;
    while (*check != NULL)
	check++;

    numsprites = (int32_t)( check - namelist );
	
    if (!numsprites)
	return;
		
    sprites = (spritedef_t*)Z_Malloc(numsprites *sizeof(*sprites), PU_STATIC, NULL);
	
    // scan all the lump names for each of the names,
    //  noting the highest frame letter.
    // Just compare 4 characters as ints
    for (i=0 ; i<numsprites ; i++)
    {
		spritename = DEH_String( namelist[ i ] );
		memset( sprtemp,-1, sizeof(sprtemp));
		
		maxframe = -1;
	
		// scan the lumps,
		//  filling in the frames for whatever is found
		for( const lookup_t& lookup : R_GetSpriteLumps() )
		{
			lumpinfo_t* lump = lumpinfo[ lookup.lumpindex ];
			if( !strncasecmp( lump->name, spritename, 4 ) )
			{
				frame = lump->name[4] - 'A';
				rotation = lump->name[5] - '0';

				int32_t patch = lookup.compositeindex;

				R_InstallSpriteLump( patch, frame, rotation, false );

				if( lump->name[ 6 ] )
				{
					frame = lump->name[6] - 'A';
					rotation = lump->name[7] - '0';
					R_InstallSpriteLump( patch, frame, rotation, true );
				}
			}
		}
	
		// check the frames that were found for completeness
		if (maxframe == -1)
		{
			sprites[ i ].numframes = 0;
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
				{
					if ( sprtemp[frame].lump[rotation] == -1 )
					{
						I_Error ("R_InitSprites: Sprite %s frame %c "
								"is missing rotations",
								spritename, frame+'A');
					}
				}
				break;
			}
		}
	
		// allocate space for the frames present and copy sprtemp to it
		sprites[i].numframes = maxframe;
		sprites[i].spriteframes = (spriteframe_t*)Z_Malloc( maxframe * sizeof(spriteframe_t), PU_STATIC, NULL );
		memcpy (sprites[i].spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));
	}

}




//
// R_InitSprites
// Called at program start.
//
void R_InitSprites(const char **namelist)
{
    R_InitSpriteDefs (namelist);
}



//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites ( spritecontext_t& spritecontext )
{
	spritecontext.nextvissprite = spritecontext.vissprites;

	spritecontext.clipbot = R_AllocateScratch< vertclip_t >( drs_current->viewwidth );
	spritecontext.cliptop = R_AllocateScratch< vertclip_t >( drs_current->viewwidth );
}


vissprite_t* R_NewVisSprite ( spritecontext_t& spritecontext )
{
	if ( spritecontext.nextvissprite < &spritecontext.vissprites[ MAXVISSPRITES ] )
	{
		return spritecontext.nextvissprite++;
	}

	return spritecontext.nextvissprite;
}


//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//

void R_DrawMaskedColumn( spritecontext_t& spritecontext, colcontext_t& colcontext, column_t* column )
{
	M_PROFILE_FUNC();

	doombool isfuzz = colcontext.colormap == nullptr;

	rend_fixed_t basetexturemid = colcontext.texturemid;
	
	for ( ; column->topdelta != 0xff ; ) 
	{
		// calculate unclipped screen coordinates
		//  for post
		rend_fixed_t topscreen = spritecontext.sprtopscreen + spritecontext.spryscale * ( column->topdelta + colcontext.sourceyoffset );
		rend_fixed_t bottomscreen = topscreen + spritecontext.spryscale*column->length;

		colcontext.yl = RendFixedToInt( topscreen + RENDFRACUNIT - 1 );
		colcontext.yh = RendFixedToInt( bottomscreen - 1 );

		if( isfuzz )
		{
			colcontext.yl = M_MAX( colcontext.yl, 1 );
			colcontext.yh = M_MIN( colcontext.yh, drs_current->viewheight - 1 );
		}

		if (colcontext.yh >= spritecontext.mfloorclip[colcontext.x])
			colcontext.yh = spritecontext.mfloorclip[colcontext.x]-1;
		if (colcontext.yl <= spritecontext.mceilingclip[colcontext.x])
			colcontext.yl = spritecontext.mceilingclip[colcontext.x]+1;

		if (colcontext.yl < colcontext.yh)
		{
			colcontext.source = (byte *)column + 3;
			colcontext.texturemid = basetexturemid - IntToRendFixed( column->topdelta + colcontext.sourceyoffset );
			// colcontext.source = (byte *)column + 3 - column->topdelta;

			// Drawn by either R_DrawColumn
			//  or (SHADOW) R_DrawFuzzColumn.
			colcontext.colfunc( &colcontext );	
		}
		column = (column_t *)(  (byte *)column + column->length + 4);
	}
	
	colcontext.texturemid = basetexturemid;
}


#define FUZZ_X_RATIO ( ( frame_adjusted_width * 100 ) / 320 )
//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
void R_DrawVisSprite( rendercontext_t& rendercontext, vissprite_t* vis, int32_t x1, int32_t x2 )
{
	M_PROFILE_FUNC();

	vbuffer_t&			dest			= rendercontext.viewbuffer;
	spritecontext_t&	spritecontext	= rendercontext.spritecontext;

	column_t*			column;
	int32_t				texturecolumn;
	int32_t				prevfuzzcolumn;
	patch_t*			patch;
	int32_t				fuzzcolumn;

	colcontext_t		spritecolcontext = {};

	patch = spritepatches[ vis->patch ];

	spritecolcontext.output = dest;
	spritecolcontext.colormap = vis->colormap;
	spritecolcontext.transparency = vis->tranmap;
	spritecolcontext.colfunc = vis->tranmap ? &R_SpriteDrawColumn_Transparent : &R_SpriteDrawColumn_Colormap;

	if (!spritecolcontext.colormap)
	{
		// NULL colormap = shadow draw
		spritecolcontext.colfunc = fuzzfuncs[ fuzz_style ];
		spritecolcontext.fuzzworkingbuffer = rendercontext.fuzzworkingbuffer;
		spritecolcontext.fuzzlightmap = rendercontext.viewpoint.colormaps + 6 * 256;
		spritecolcontext.fuzzdarkmap = rendercontext.viewpoint.colormaps + 14 * 256;
	}
	else if (vis->mobjflags & MF_TRANSLATION)
	{
		spritecolcontext.colfunc = &R_DrawTranslatedColumn;
		spritecolcontext.translation = translationtables - 256 +
			( (vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT-8) );
	}
	
	spritecolcontext.iscale = abs( vis->iscale );
	spritecolcontext.texturemid = vis->texturemid;
	spritecontext.spryscale = vis->scale;
	spritecontext.spryiscale = vis->iscale;
	spritecontext.sprtopscreen = drs_current->centeryfrac - RendFixedMul( spritecolcontext.texturemid, spritecontext.spryscale );
	
#ifdef NEWFUZZ
	constexpr rend_fixed_t screenscale = RendFixedDiv( IntToRendFixed( 1 ), IntToRendFixed( 320 ) );

	rend_fixed_t fuzzincrement = RendFixedMul( IntToRendFixed( frame_adjusted_width ), screenscale );
	rend_fixed_t fuzzcolumnpos = RendFixedMul( IntToRendFixed( spritecolcontext.x ), screenscale );
	int32_t prevfuzzcolumn = RendFixedToInt( fuzzcolumnpos );

	for( spritecolcontext.x = vis->x1; spritecolcontext.x <= vis->x2; spritecolcontext.x++, frac += vis->xiscale )
	{
		texturecolumn = RendFixedToInt( frac );
#if RANGECHECK
		if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
			I_Error ("R_DrawSpriteRange: bad texturecolumn");
#endif

		if( !spritecolcontext.colormap && fuzz_style != Fuzz_Original )
		{
			fuzzcolumnpos += fuzzincrement;
			int32_t fuzzcolumn = RendFixedToInt( fuzzcolumnpos );
			if( prevfuzzcolumn != fuzzcolumn )
			{
				R_CacheFuzzColumn();
				prevfuzzcolumn = fuzzcolumn;
			}
		}

		column = (column_t *)( (byte *)patch + LONG(patch->columnofs[texturecolumn]) );

		R_DrawMaskedColumn( spritecontext, spritecolcontext, column );
	}
#else
	prevfuzzcolumn = -1;
	rend_fixed_t frac = vis->startfrac;
	for( spritecolcontext.x = vis->x1; spritecolcontext.x <= vis->x2; spritecolcontext.x++, frac += vis->xiscale )
	{
		texturecolumn = RendFixedToInt( frac );
#if RANGECHECK
		if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
			I_Error ("R_DrawSpriteRange: bad texturecolumn");
#endif

		if( !spritecolcontext.colormap && fuzz_style != Fuzz_Original )
		{
			fuzzcolumn = ( spritecolcontext.x * 100 ) / FUZZ_X_RATIO;
			if(prevfuzzcolumn != fuzzcolumn)
			{
				R_CacheFuzzColumn();
				prevfuzzcolumn = fuzzcolumn;
			}
		}

		column = (column_t *)( (byte *)patch + LONG(patch->columnofs[texturecolumn]) );

		R_DrawMaskedColumn( spritecontext, spritecolcontext, column );
	}
#endif
}



//
// R_ProjectSprite
// Generates a vissprite for a thing
//  if it might be visible.
//

void R_ProjectSprite( rendercontext_t& rendercontext, mobj_t* thing)
{
	viewpoint_t&		viewpoint		= rendercontext.viewpoint;
	spritecontext_t&	spritecontext	= rendercontext.spritecontext;

	int32_t			x1;
	int32_t			x2;

	spritedef_t*	sprdef;
	spriteframe_t*	sprframe;
	int32_t			lump;

	uint32_t		rot;
	doombool		flip;

	int32_t			index;

	vissprite_t*	vis;

	rend_fixed_t	thingx			= thing->curr.x;
	rend_fixed_t	thingy			= thing->curr.y;
	rend_fixed_t	thingz			= thing->curr.z;

	int32_t			thingframe		= thing->curr.frame;
	spritenum_t		thingsprite		= thing->curr.sprite;
	angle_t			thingangle		= thing->curr.angle;

	if( interpolate_this_frame )
	{
		doombool selectcurr = ( viewpoint.lerp >= ( RENDFRACUNIT >> 1 ) );
		if( thing->curr.teleported )
		{
			thingx = selectcurr ? thing->curr.x : thing->prev.x;
			thingy = selectcurr ? thing->curr.y : thing->prev.y;
			thingz = selectcurr ? thing->curr.z : thing->prev.z;
		}
		else
		{
			thingx = RendFixedLerp( thing->prev.x, thing->curr.x, viewpoint.lerp );
			thingy = RendFixedLerp( thing->prev.y, thing->curr.y, viewpoint.lerp );
			thingz = RendFixedLerp( thing->prev.z, thing->curr.z, viewpoint.lerp );
		}

		thingframe = selectcurr ? thing->curr.frame : thing->prev.frame;
		thingsprite = selectcurr ? thing->curr.sprite : thing->prev.sprite;
		thingangle = selectcurr ? thing->curr.angle : thing->prev.angle;

		if( thingsprite == -1 ) // Spawned in current frame
		{
			return;
		}
	}

	// transform the origin point
	rend_fixed_t tr_x = thingx - viewpoint.x;
	rend_fixed_t tr_y = thingy - viewpoint.y;
	
	rend_fixed_t gxt = RendFixedMul( tr_x, viewpoint.cos ); 
	rend_fixed_t gyt = -RendFixedMul( tr_y, viewpoint.sin );

	rend_fixed_t tz = gxt - gyt;

	// thing is behind view plane?
	if( tz < MINZ )
	{
		return;
	}

	rend_fixed_t xscale = RendFixedDiv( drs_current->xprojection, tz );
	rend_fixed_t xiscale = RendFixedDiv( RENDFRACUNIT, xscale );
	rend_fixed_t yscale = RendFixedDiv( drs_current->yprojection, tz );
	
	gxt = -RendFixedMul( tr_x, viewpoint.sin );
	gyt = RendFixedMul( tr_y, viewpoint.cos );
	rend_fixed_t tx = -( gyt + gxt );

	// too far off the side?
	if( abs( tx ) > ( tz << 2 ) )
	{
		return;
	}

	// decide which patch to use for sprite relative to player
#if RANGECHECK
	if ((uint32_t) thingsprite >= (uint32_t) numsprites)
	{
		I_Error ("R_ProjectSprite: invalid sprite number %i ",
				thingsprite);
	}
#endif

	sprdef = &sprites[thingsprite];

#if RANGECHECK
	if ( (thingframe&FF_FRAMEMASK) >= sprdef->numframes )
	{
		if( remove_limits ) return;

		I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ",
				thingsprite, thingframe);
	}
#endif

	sprframe = &sprdef->spriteframes[ thingframe & FF_FRAMEMASK];

	if (sprframe->rotate)
	{
		// choose a different rotation based on player view
		angle_t ang = R_PointToAngle( &viewpoint, thingx, thingy );
		rot = ( ang - thingangle + (uint32_t)( ANG45 / 2 ) * 9 ) >> 29;
		lump = sprframe->lump[rot];
		flip = (doombool)sprframe->flip[rot];
	}
	else
	{
		// use single rotation for all views
		lump = sprframe->lump[0];
		flip = (doombool)sprframe->flip[0];
	}

	// calculate edges of the shape
	tx -= spriteoffset[lump];
	x1 = RendFixedToInt( drs_current->centerxfrac + RendFixedMul( tx, xscale ) );

	// off the right side?
	if (x1 > spritecontext.rightclip)
	{
		return;
	}

	tx += spritewidth[lump];
	x2 = RendFixedToInt( drs_current->centerxfrac + RendFixedMul( tx, xscale ) ) - 1;

	// off the left side
	if (x2 < spritecontext.leftclip)
	{
		return;
	}

	// store information in a vissprite
	vis = R_NewVisSprite( spritecontext );
	vis->mobjflags = thing->flags;
	vis->mobjflags2 = thing->flags2;
	vis->scale = yscale;
	vis->iscale = RendFixedDiv( RENDFRACUNIT, yscale );
	vis->gx = thingx;
	vis->gy = thingy;
	vis->gz = thingz;
	vis->gzt = thingz + spritetopoffset[ lump ];
	vis->sector = &rendsectors[ thing->subsector->sector->index ];
	vis->texturemid = vis->gzt - viewpoint.z;
	vis->x1 = x1 < spritecontext.leftclip ? spritecontext.leftclip : x1;
	vis->x2 = x2 >= spritecontext.rightclip ? spritecontext.rightclip-1 : x2;	

	if (flip)
	{
		vis->startfrac = spritewidth[lump] - 1;
		vis->xscale = -xscale;
		vis->xiscale = -xiscale;
	}
	else
	{
		vis->startfrac = 0;
		vis->xscale = xscale;
		vis->xiscale = xiscale;
	}

	if (vis->x1 > x1)
	{
		vis->startfrac += vis->xiscale * ( vis->x1 - x1 );
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
	else if (thingframe & FF_FULLBRIGHT)
	{
		// full bright
		vis->colormap = rendercontext.viewpoint.colormaps;
	}
	else
	{
		// diminished light
		index = (int32_t)RendFixedMul( xscale, drs_current->frame_adjusted_light_mul ) >> RENDLIGHTSCALESHIFT;

		if (index >= MAXLIGHTSCALE)
			index = MAXLIGHTSCALE-1;

		vis->colormap = rendercontext.viewpoint.colormaps + spritecontext.spritelightoffsets[ index ];
	}

	if( comp.use_translucency && thing->flags & MF_BOOM_TRANSLUCENT )
	{
		vis->tranmap = tranmap;
	}
}

//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
void R_AddSprites( rendercontext_t& rendercontext, sector_t* sec )
{
	spritecontext_t&	spritecontext	= rendercontext.spritecontext;

	mobj_t*		thing;
	int			lightnum;

	// BSP is traversed by subsector.
	// A sector might have been split into several
	//  subsectors during BSP building.
	// Thus we check whether its already added.
	if ( spritecontext.sectorvisited[ sec->index ] )
	{
		return;
	}

	// Well, now it will be done.
	spritecontext.sectorvisited[ sec->index ] = true;
	
	lightnum = (sec->lightlevel >> LIGHTSEGSHIFT)+extralight;
	lightnum = M_CLAMP( lightnum, 0, LIGHTLEVELS - 1 );
	spritecontext.spritelightoffsets = &drs_current->scalelightoffset[ MAXLIGHTSCALE * lightnum ];

	// Handle all things in sector.
	for (thing = sec->thinglist ; thing ; thing = thing->snext)
	{
		R_ProjectSprite( rendercontext, thing );
	}
}


//
// R_DrawPSprite
//
void R_DrawPSprite( rendercontext_t& rendercontext, pspdef_t* psp )
{
	viewpoint_t&		viewpoint		= rendercontext.viewpoint;
	spritecontext_t&	spritecontext	= rendercontext.spritecontext;

	int32_t			x1;
	int32_t			x2;
	spritedef_t*	sprdef;
	spriteframe_t*	sprframe;
	int32_t			lump;
	doombool		flip;
	vissprite_t*	vis;
	vissprite_t		avis;

    // decide which patch to use
#if RANGECHECK
    if ( (uint32_t)psp->state->sprite >= (uint32_t) numsprites)
	{
		I_Error ("R_ProjectSprite: invalid sprite number %i ",
			 psp->state->sprite);
	}
#endif
	sprdef = &sprites[psp->state->sprite];
#if RANGECHECK
	if ( (psp->state->frame & FF_FRAMEMASK)  >= sprdef->numframes)
	{
		I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ",
			 psp->state->sprite, psp->state->frame);
	}
#endif
	sprframe = &sprdef->spriteframes[ psp->state->frame & FF_FRAMEMASK ];

	lump = sprframe->lump[0];
	flip = (doombool)sprframe->flip[0];

	rend_fixed_t sx = FixedToRendFixed( psp->sx );
	rend_fixed_t sy = FixedToRendFixed( psp->sy );

	if( interpolate_this_frame && psp->duration > 0 )
	{
		if( psp->viewbob )
		{
			rend_fixed_t percent = RendFixedDiv( IntToRendFixed( view_bobbing_percent ), IntToRendFixed( 100 ) );
			rend_fixed_t fracleveltime = IntToRendFixed( leveltime ) + viewpoint.lerp;
			rend_fixed_t adjusted = fracleveltime * 128;
			int32_t actual = RendFixedToInt( adjusted );
			angle_t angle = actual & FINEMASK;
			sx = RENDFRACUNIT + RendFixedMul( RendFixedMul( viewpoint.weaponbob, renderfinecosine[ angle << RENDERQUALITYSHIFT ] ), percent );
			angle &= FINEANGLES/2-1;
			sy = FixedToRendFixed( WEAPONTOP ) + RendFixedMul( RendFixedMul( viewpoint.weaponbob, renderfinesine[ angle << RENDERQUALITYSHIFT ] ), percent );
		}
		else
		{
			int64_t thisframe = leveltime - psp->leveltime;
			rend_fixed_t framecount = IntToRendFixed( thisframe + 1 );
			rend_fixed_t framelen = RendFixedDiv( IntToRendFixed( 1 ), framecount );
			rend_fixed_t amount = thisframe * framelen + RendFixedDiv( viewpoint.lerp, framecount );
			rend_fixed_t lastx = FixedToRendFixed( psp->lastx );
			rend_fixed_t lasty = FixedToRendFixed( psp->lasty );
			sx = RendFixedLerp( lastx, sx, amount );
			sy = RendFixedLerp( lasty, sy, amount );
		}
	}
	else if( psp->viewbob )
	{
		rend_fixed_t percent = RendFixedDiv( IntToRendFixed( view_bobbing_percent ), IntToRendFixed( 100 ) );
		rend_fixed_t fracleveltime = IntToRendFixed( psp->leveltime );
		rend_fixed_t adjusted = fracleveltime * 128;
		int32_t actual = RendFixedToInt( adjusted );
		angle_t angle = actual & FINEMASK;
		sx = RENDFRACUNIT + RendFixedMul( RendFixedMul( viewpoint.weaponbob, renderfinecosine[ angle << RENDERQUALITYSHIFT ] ), percent );
		angle &= FINEANGLES/2-1;
		sy = FixedToRendFixed( WEAPONTOP ) + RendFixedMul( RendFixedMul( viewpoint.weaponbob, renderfinesine[ angle << RENDERQUALITYSHIFT ] ), percent );
	}

	// calculate edges of the shape
	rend_fixed_t tx = sx - IntToRendFixed( V_VIRTUALWIDTH / 2 );
	
	tx -= spriteoffset[lump]; 	
	x1 = RendFixedToInt( drs_current->centerxfrac + RendFixedMul( tx, drs_current->pspritescalex ) );

	// off the right side
	if (x1 > spritecontext.rightclip)
	{
		return;
	}

	tx += spritewidth[lump];
	x2 = RendFixedToInt( drs_current->centerxfrac + RendFixedMul( tx, drs_current->pspritescalex ) ) - 1;

	// off the left side
	if (x2 < spritecontext.leftclip)
	{
		return;
	}

	// store information in a vissprite
	vis = &avis;
	vis->mobjflags = 0;
	vis->texturemid = IntToRendFixed( BASEYCENTER ) + RENDFRACUNIT / 2 -( sy - spritetopoffset[ lump ] );
	vis->x1 = x1 < spritecontext.leftclip ? spritecontext.leftclip : x1;
	vis->x2 = x2 >= spritecontext.rightclip ? spritecontext.rightclip-1 : x2;	
	vis->scale = drs_current->pspritescaley;
	vis->iscale = drs_current->pspriteiscaley;

	if (flip)
	{
		vis->xscale = -drs_current->pspritescalex;
		vis->xiscale = -drs_current->pspriteiscalex;
		vis->startfrac = spritewidth[lump] - 1;
	}
	else
	{
		vis->xscale = drs_current->pspritescalex;
		vis->xiscale = drs_current->pspriteiscalex;
		vis->startfrac = 0;
	}

	if (vis->x1 > x1)
	{
		vis->startfrac += vis->xiscale*(vis->x1-x1);
	}

	vis->patch = lump;
	vis->tranmap = nullptr;

	if (viewpoint.player->powers[pw_invisibility] > 4*32
		|| viewpoint.player->powers[pw_invisibility] & 8)
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
		vis->colormap = rendercontext.viewpoint.colormaps;
	}
	else
	{
		// local light
		vis->colormap = rendercontext.viewpoint.colormaps + spritecontext.spritelightoffsets[MAXLIGHTSCALE-1];
	}
	
	R_DrawVisSprite( rendercontext, vis, vis->x1, vis->x2 );
}



//
// R_DrawPlayerSprites
//
void R_DrawPlayerSprites( rendercontext_t& rendercontext )
{
	M_PROFILE_FUNC();

	viewpoint_t&		viewpoint		= rendercontext.viewpoint;
	spritecontext_t&	spritecontext	= rendercontext.spritecontext;

	int32_t		i;
	int32_t		lightnum;
	pspdef_t*	psp;

	// get light level
	lightnum = (viewpoint.player->mo->subsector->sector->lightlevel >> LIGHTSEGSHIFT) + extralight;
	lightnum = M_CLAMP( lightnum, 0, LIGHTLEVELS - 1 );
	spritecontext.spritelightoffsets = &drs_current->scalelightoffset[ MAXLIGHTSCALE * lightnum ];

	// clip to screen bounds
	spritecontext.mfloorclip = drs_current->screenheightarray;
	spritecontext.mceilingclip = drs_current->negonearray;

	// add all active psprites
	for (i=0, psp = viewpoint.player->psprites; i<NUMPSPRITES; i++,psp++)
	{
		if (psp->state)
		{
			R_DrawPSprite( rendercontext, psp );
		}
	}
}


//
// R_SortVisSprites
//

void R_SortVisSprites ( spritecontext_t& spritecontext )
{
	M_PROFILE_FUNC();

	int32_t			i;
	int32_t			count;
	vissprite_t*	ds;
	vissprite_t*	best;
	vissprite_t		unsorted;

	count = (int32_t)( spritecontext.nextvissprite - spritecontext.vissprites );
	
	unsorted.next = unsorted.prev = &unsorted;

	if (!count)
	{
		return;
	}
		
	for ( ds = spritecontext.vissprites ; ds < spritecontext.nextvissprite; ds++)
	{
		ds->next = ds+1;
		ds->prev = ds-1;
	}

	spritecontext.vissprites[0].prev = &unsorted;
	unsorted.next = &spritecontext.vissprites[0];
	(spritecontext.nextvissprite -1 )->next = &unsorted;
	unsorted.prev = spritecontext.nextvissprite - 1;

	// pull the vissprites out by scale

	spritecontext.vsprsortedhead.next = spritecontext.vsprsortedhead.prev = &spritecontext.vsprsortedhead;
	for (i=0 ; i<count ; i++)
	{
		rend_fixed_t bestscale = LLONG_MAX;
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
		best->next = &spritecontext.vsprsortedhead;
		best->prev = spritecontext.vsprsortedhead.prev;
		spritecontext.vsprsortedhead.prev->next = best;
		spritecontext.vsprsortedhead.prev = best;
	}
}


//
// R_DrawSprite
//
void R_DrawSprite( rendercontext_t& rendercontext, vissprite_t* spr )
{
	M_PROFILE_FUNC();

	viewpoint_t&		viewpoint		= rendercontext.viewpoint;
	bspcontext_t&		bspcontext		= rendercontext.bspcontext;
	spritecontext_t&	spritecontext	= rendercontext.spritecontext;

	drawseg_t*		ds;
	vertclip_t*		clipbot = spritecontext.clipbot;
	vertclip_t*		cliptop = spritecontext.cliptop;
	int32_t			x;
	int32_t			r1;
	int32_t			r2;
	rend_fixed_t	scale;
	rend_fixed_t	lowscale;
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
	for (ds = bspcontext.thisdrawseg - 1 ; ds >= bspcontext.drawsegs ; ds--)
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
				// Don't even do all that heavy lifting in RenderMaskedSegRange if there's nothing to render
				for( int32_t check = spr->x1; check <= spr->x2; ++check )
				{
					if( ds->maskedtexturecol[ check ] != MASKEDTEXCOL_INVALID )
					{
#if RENDER_PERF_GRAPHING
						starttime = I_GetTimeUS();
#endif // RENDER_PERF_GRAPHING
						R_RenderMaskedSegRange( rendercontext, ds, r1, r2 );
#if RENDER_PERF_GRAPHING
						endtime = I_GetTimeUS();
						bspcontext.maskedtimetaken += ( endtime - starttime );
#endif // RENDER_PERF_GRAPHING
						// Break out of enclosing for loop now that we know we didn't waste our time
						break;
					}
				}
			}
			// seg is behind sprite
			continue;			
		}

		// clip this piece of the sprite
		silhouette = ds->silhouette;
	
		if( spr->gz >= ds->bsilheight )
			silhouette &= ~SIL_BOTTOM;

		if( spr->gzt <= ds->tsilheight )
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

	if( spr->sector->clipceiling )
	{
		int32_t seccliptop = RendFixedToInt( drs_current->centeryfrac - RendFixedMul( spr->sector->ceilheight - viewpoint.z, spr->scale ) + RENDFRACUNIT - 1 );
		seccliptop = M_MAX( -1, seccliptop );
		for (x=spr->x1 ; x<=spr->x2 ; x++)
		{
			cliptop[x] = cliptop[x] == -2 ? seccliptop : M_MAX( cliptop[x], seccliptop );
		}
	}
		
	if( spr->sector->clipfloor )
	{
		int32_t secclipbot = RendFixedToInt( drs_current->centeryfrac - RendFixedMul( spr->sector->floorheight - viewpoint.z, spr->scale ) + RENDFRACUNIT - 1 );
		secclipbot = M_MIN( drs_current->viewheight, secclipbot );
		for (x=spr->x1 ; x<=spr->x2 ; x++)
		{
			clipbot[x] = clipbot[x] == -2 ? secclipbot : M_MIN( clipbot[x], secclipbot );
		}
	}
		
	// all clipping has been performed, so draw the sprite
	// check for unclipped columns
	int32_t clippedcolumns = 0;
	for (x = spr->x1 ; x<=spr->x2 ; x++)
	{
		if (clipbot[x] == -2)
			clipbot[x] = drs_current->viewheight;

		if (cliptop[x] == -2)
			cliptop[x] = -1;

		if( cliptop[x] == clipbot[x] ) ++clippedcolumns;
	}

	if( clippedcolumns > ( spr->x2 - spr->x1 ) )
	{
		return;
	}
		
	spritecontext.mfloorclip = clipbot;
	spritecontext.mceilingclip = cliptop;
#if RENDER_PERF_GRAPHING
	starttime = I_GetTimeUS();
#endif // RENDER_PERF_GRAPHING
	R_DrawVisSprite( rendercontext, spr, spr->x1, spr->x2 );
#if RENDER_PERF_GRAPHING
	endtime = I_GetTimeUS();
	spritecontext.maskedtimetaken += ( endtime - starttime );
#endif // RENDER_PERF_GRAPHING
}


//
// R_DrawMasked
//
void R_DrawMasked( rendercontext_t& rendercontext )
{
	vissprite_t*	spr;
	drawseg_t*		ds;
	
	R_SortVisSprites( rendercontext.spritecontext );

	if ( rendercontext.spritecontext.nextvissprite > rendercontext.spritecontext.vissprites)
	{
		// draw all vissprites back to front
		for (spr = rendercontext.spritecontext.vsprsortedhead.next ;
				spr != &rendercontext.spritecontext.vsprsortedhead ;
				spr = spr->next)
		{
			R_DrawSprite( rendercontext, spr );
		}
	}

	// render any remaining masked mid textures
	for ( ds = rendercontext.bspcontext.thisdrawseg - 1 ; ds >= rendercontext.bspcontext.drawsegs ; ds-- )
	{
		if (ds->maskedtexturecol)
		{
			R_RenderMaskedSegRange( rendercontext, ds, ds->x1, ds->x2 );
		}
	}

	// draw the psprites on top of everything
	//  but does not draw on side views
	if (!viewangleoffset)
	{
		R_DrawPlayerSprites( rendercontext );
	}
}


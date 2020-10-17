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
//	All the clipping: columns, horizontal spans, sky columns.
//






#include <stdio.h>
#include <stdlib.h>

#include "i_system.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_sky.h"

#include "m_misc.h"

// OPTIMIZE: closed two sided lines as single sided

angle_t		rw_normalangle;
// angle to line origin
int32_t		rw_angle1;	

//
// regular wall
//
angle_t		rw_centerangle;
fixed_t		rw_offset;
fixed_t		rw_distance;
fixed_t		rw_scale;
fixed_t		rw_scalestep;
fixed_t		rw_midtexturemid;
fixed_t		rw_toptexturemid;
fixed_t		rw_bottomtexturemid;

typedef struct segloopcontext_s
{
	int32_t			startx;
	int32_t			stopx;

	// True if any of the segs textures might be visible.
	boolean			segtextured;

	boolean			maskedtexture;
	int32_t			toptexture;
	int32_t			bottomtexture;
	int32_t			midtexture;

	// False if the back side is the same plane.
	boolean			markfloor;	
	boolean			markceiling;

	fixed_t			pixhigh;
	fixed_t			pixlow;
	fixed_t			pixhighstep;
	fixed_t			pixlowstep;

	fixed_t			topfrac;
	fixed_t			topstep;

	fixed_t			bottomfrac;
	fixed_t			bottomstep;

	vertclip_t*		maskedtexturecol;

} segloopcontext_t;


lighttable_t**		walllights;
int32_t				walllightsindex;

#ifdef RANGECHECK
void R_RangeCheckNamed( colcontext_t* context, const char* func )
{
	if( context->yl != context->yh )
	{
		if ((unsigned)context->x >= SCREENWIDTH
			|| context->yl < 0
			|| context->yh >= SCREENHEIGHT) 
		{
			I_Error ("%s: %i to %i at %i", func, context->yl, context->yh, context->x);
		}
	}
}

#define R_RangeCheck() R_RangeCheckNamed( &wallcontext, __FUNCTION__ )

#else // !RANGECHECK

#define R_RangeCheckNamed( context, func )
#define R_RangeCheck()

#endif // RANGECHECK


//
// R_RenderMaskedSegRange
//
void R_RenderMaskedSegRange( bspcontext_t* bspcontext, drawseg_t* ds, int x1, int x2 )
{
	uint32_t			index;
	column_t*			col;
	int32_t				lightnum;
	int32_t				texnum;
	vertclip_t*			maskedtexturecol;

	colcontext_t		spritecontext;
	extern vbuffer_t*	dest_buffer;

	colfunc_t			restorefunc = colfunc;

	spritecontext.output = *dest_buffer;

	// Calculate light table.
	// Use different light tables
	//   for horizontal / vertical / diagonal. Diagonal?
	// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
	bspcontext->curline = ds->curline;
	bspcontext->frontsector = bspcontext->curline->frontsector;
	bspcontext->backsector = bspcontext->curline->backsector;
	texnum = texturetranslation[bspcontext->curline->sidedef->midtexture];
	
	lightnum = (bspcontext->frontsector->lightlevel >> LIGHTSEGSHIFT)+extralight;

	if (bspcontext->curline->v1->y == bspcontext->curline->v2->y)
		lightnum--;
	else if (bspcontext->curline->v1->x == bspcontext->curline->v2->x)
		lightnum++;

	if( fixedcolormapindex )
		walllightsindex = fixedcolormapindex;
    else if (lightnum < 0)
		walllightsindex = 0;
    else if (lightnum >= LIGHTLEVELS)
		walllightsindex = LIGHTLEVELS-1;
	else
		walllightsindex = lightnum;
		
	walllights = scalelight[ walllightsindex ];

	maskedtexturecol = ds->maskedtexturecol;

	rw_scalestep = ds->scalestep;
	spryscale = ds->scale1 + (x1 - ds->x1)*rw_scalestep;
	mfloorclip = ds->sprbottomclip;
	mceilingclip = ds->sprtopclip;

	// find positioning
	if (bspcontext->curline->linedef->flags & ML_DONTPEGBOTTOM)
	{
		spritecontext.texturemid = bspcontext->frontsector->floorheight > bspcontext->backsector->floorheight
									? bspcontext->frontsector->floorheight
									: bspcontext->backsector->floorheight;
		spritecontext.texturemid = spritecontext.texturemid + textureheight[texnum] - viewz;
	}
	else
	{
		spritecontext.texturemid = bspcontext->frontsector->ceilingheight < bspcontext->backsector->ceilingheight
									? bspcontext->frontsector->ceilingheight
									: bspcontext->backsector->ceilingheight;
		spritecontext.texturemid = spritecontext.texturemid - viewz;
	}
	spritecontext.texturemid += bspcontext->curline->sidedef->rowoffset;

	if (fixedcolormap)
	{
		spritecontext.colormap = fixedcolormap;
	}

	// draw the columns
	for (spritecontext.x = x1 ; spritecontext.x <= x2 ; spritecontext.x++)
	{
		// calculate lighting
		if (maskedtexturecol[spritecontext.x] != MASKEDTEXCOL_INVALID)
		{
			if (!fixedcolormap)
			{
				index = spryscale>>LIGHTSCALESHIFT;
#if SCREENWIDTH != 320
				index = FixedDiv( index << FRACBITS, LIGHTSCALEDIVIDE ) >> FRACBITS;
#endif // SCREENWIDTH != 320

				if (index >=  MAXLIGHTSCALE )
					index = MAXLIGHTSCALE-1;

				spritecontext.colormap = walllights[index];
			}
			
			sprtopscreen = centeryfrac - FixedMul(spritecontext.texturemid, spryscale);
			spritecontext.scale = spryscale;
			spritecontext.iscale = 0xffffffffu / (unsigned)spryscale;

			// Mental note: Can't use the optimised funcs until we pre-light sprites etc :=(
			colfunc = &R_DrawColumn_Untranslated; // colfuncs[ M_MIN( ( dc_iscale >> 12 ), 15 ) ];

			// draw the texture
			col = (column_t *)( R_GetRawColumn( texnum,maskedtexturecol[spritecontext.x] ) -3 );
			
			R_DrawMaskedColumn( &spritecontext, col );
			maskedtexturecol[spritecontext.x] = MASKEDTEXCOL_INVALID;

			colfunc = restorefunc;
		}
		spryscale += rw_scalestep;

	}
}




//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked
//  texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling
//  textures.
// CALLED: CORE LOOPING ROUTINE.
//
#define HEIGHTBITS		12
#define HEIGHTUNIT		(1<<HEIGHTBITS)

// Detail maps show me how may reads are going to happen in any 16-byte block.
extern byte detailmaps[16][256];

void R_RenderSegLoop ( segloopcontext_t* context )
{
	angle_t			angle;
	uint32_t		index;
	int32_t			colormapindex;
	int32_t 		yl;
	int32_t 		yh;
	int32_t 		mid;
	fixed_t			texturecolumn;
	int32_t			top;
	int32_t			bottom;
	int32_t			currx = context->startx;

	colcontext_t	wallcontext;
	extern vbuffer_t* dest_buffer;

	colfunc_t		restorefunc = colfunc;

	wallcontext.output = *dest_buffer;

	for ( ; currx < context->stopx ; currx++)
	{
		// mark floor / ceiling areas
		yl = (context->topfrac+HEIGHTUNIT-1)>>HEIGHTBITS;

		// no space above wall?
		if (yl < ceilingclip[currx]+1)
		{
			yl = ceilingclip[currx]+1;
		}
	
		if (context->markceiling)
		{
			top = ceilingclip[currx]+1;
			bottom = yl-1;

			if (bottom >= floorclip[currx])
			{
				bottom = floorclip[currx]-1;
			}

			if (top <= bottom)
			{
				ceilingplane->top[currx] = top;
				ceilingplane->bottom[currx] = bottom;
			}
		}
		
		yh = context->bottomfrac>>HEIGHTBITS;

		if (yh >= floorclip[currx])
		{
			yh = floorclip[currx]-1;
		}

		if (context->markfloor)
		{
			top = yh+1;
			bottom = floorclip[currx]-1;
			if (top <= ceilingclip[currx])
			{
				top = ceilingclip[currx]+1;
			}
			if (top <= bottom)
			{
				floorplane->top[currx] = top;
				floorplane->bottom[currx] = bottom;
			}
		}
	
		// texturecolumn and lighting are independent of wall tiers
		if (context->segtextured)
		{
			// calculate texture offset
			angle = (rw_centerangle + xtoviewangle[currx])>>RENDERANGLETOFINESHIFT;
			texturecolumn = rw_offset-FixedMul(renderfinetangent[angle],rw_distance);
			texturecolumn >>= FRACBITS;
			// calculate lighting
			index = rw_scale>>LIGHTSCALESHIFT;

#if SCREENWIDTH != 320
			index = FixedDiv( index << FRACBITS, LIGHTSCALEDIVIDE ) >> FRACBITS;
#endif // SCREENWIDTH != 320

			if (index >=  MAXLIGHTSCALE ) index = MAXLIGHTSCALE-1;

			if( fixedcolormapindex )
			{
				colormapindex = fixedcolormapindex;
			}
			else
			{
				colormapindex = walllightsindex < NUMLIGHTCOLORMAPS ? scalelightindex[ walllightsindex ][ index ] : walllightsindex;
			}
			wallcontext.x = currx;
			wallcontext.scale = rw_scale;
			wallcontext.iscale = 0xffffffffu / (unsigned)rw_scale;

#if R_DRAWCOLUMN_DEBUGDISTANCES
			colfunc = colfuncs[ 15 ];
#else
			colfunc = colfuncs[ M_MIN( ( wallcontext.iscale >> 12 ), 15 ) ];
#endif
	
		}
		else
		{
			// purely to shut up the compiler
			texturecolumn = 0;
		}

		// draw the wall tiers
		if (context->midtexture && yh >= yl)
		{
			// single sided line
			wallcontext.yl = yl;
			wallcontext.yh = yh;
			wallcontext.texturemid = rw_midtexturemid;
			wallcontext.source = R_DRAWCOLUMN_DEBUGDISTANCES ? detailmaps[ M_MIN( ( wallcontext.iscale >> 12 ), 15 ) ] : R_GetColumn(context->midtexture,texturecolumn,colormapindex);
			R_RangeCheck();
			colfunc( &wallcontext );
			ceilingclip[currx] = viewheight;
			floorclip[currx] = -1;
		}
		else
		{
			// two sided line
			if (context->toptexture)
			{
				// top wall
				mid = context->pixhigh>>HEIGHTBITS;
				context->pixhigh += context->pixhighstep;

				if (mid >= floorclip[currx])
				{
					mid = floorclip[currx]-1;
				}

				if (mid >= yl)
				{
					wallcontext.yl = yl;
					wallcontext.yh = mid;
					wallcontext.texturemid = rw_toptexturemid;
					wallcontext.source = R_DRAWCOLUMN_DEBUGDISTANCES ? detailmaps[ M_MIN( ( wallcontext.iscale >> 12 ), 15 ) ] : R_GetColumn(context->toptexture,texturecolumn,colormapindex);
					R_RangeCheck();
					colfunc( &wallcontext );
					ceilingclip[currx] = mid;
				}
				else
				{
					ceilingclip[currx] = yl-1;
				}
			}
			else
			{
				// no top wall
				if (context->markceiling)
				{
					ceilingclip[currx] = yl-1;
				}
			}
			
			if (context->bottomtexture)
			{
				// bottom wall
				mid = (context->pixlow+HEIGHTUNIT-1)>>HEIGHTBITS;
				context->pixlow += context->pixlowstep;

				// no space above wall?
				if (mid <= ceilingclip[currx])
					mid = ceilingclip[currx]+1;
		
				if (mid <= yh)
				{
					wallcontext.yl = mid;
					wallcontext.yh = yh;
					wallcontext.texturemid = rw_bottomtexturemid;
					wallcontext.source = R_DRAWCOLUMN_DEBUGDISTANCES ? detailmaps[ M_MIN( ( wallcontext.iscale >> 12 ), 15 ) ] : R_GetColumn(context->bottomtexture,texturecolumn,colormapindex);
					R_RangeCheck();
					colfunc( &wallcontext );
					floorclip[currx] = mid;
				}
				else
				{
					floorclip[currx] = yh+1;
				}
			}
			else
			{
				// no bottom wall
				if (context->markfloor)
				{
					floorclip[currx] = yh+1;
				}
			}
			
			if (context->maskedtexture)
			{
				// save texturecol
				//  for backdrawing of masked mid texture
				context->maskedtexturecol[currx] = texturecolumn;
			}
		}
		
		rw_scale += rw_scalestep;
		context->topfrac += context->topstep;
		context->bottomfrac += context->bottomstep;

		colfunc = restorefunc;
	#if R_DRAWCOLUMN_DEBUGDISTANCES
		wallcontext.colormap = restorelightmap;
	#endif // R_DRAWCOLUMN_DEBUGDISTANCES
	}
}

//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
void R_StoreWallRange( bspcontext_t* bspcontext, int start, int stop )
{
	fixed_t		hyp;
	fixed_t		sineval;
	angle_t		distangle, offsetangle;
	fixed_t		vtop;
	int32_t		lightnum;

	segloopcontext_t loopcontext;

	int32_t		worldtop;
	int32_t		worldbottom;
	int32_t		worldhigh;
	int32_t		worldlow;

	// don't overflow and crash
	if ( bspcontext->thisdrawseg == &bspcontext->drawsegs[MAXDRAWSEGS])
	{
		return;
	}
		
#ifdef RANGECHECK
	if (start >=viewwidth || start > stop)
	I_Error ("Bad R_RenderWallRange: %i to %i", start , stop);
#endif

	bspcontext->sidedef = bspcontext->curline->sidedef;
	bspcontext->linedef = bspcontext->curline->linedef;

	// mark the segment as visible for auto map
	bspcontext->linedef->flags |= ML_MAPPED;

	// calculate rw_distance for scale calculation
	rw_normalangle = bspcontext->curline->angle + ANG90;
	offsetangle = abs((int)rw_normalangle-(int)rw_angle1);
	offsetangle = M_MIN( offsetangle, ANG90 );

	distangle = ANG90 - offsetangle;
	hyp = R_PointToDist (bspcontext->curline->v1->x, bspcontext->curline->v1->y);
	sineval = renderfinesine[distangle>>RENDERANGLETOFINESHIFT];
	// If this value blows out, renderer go boom. Need to increase resolution of this thing
	rw_distance = FixedMul (hyp, sineval);
	
	bspcontext->thisdrawseg->x1 = loopcontext.startx = start;
	bspcontext->thisdrawseg->x2 = stop;
	bspcontext->thisdrawseg->curline = bspcontext->curline;
	loopcontext.stopx = stop+1;

	// calculate scale at both ends and step
	bspcontext->thisdrawseg->scale1 = rw_scale = 
	R_ScaleFromGlobalAngle (viewangle + xtoviewangle[start], rw_distance);

	if (stop > start )
	{
		// TODO: calculate second distance? Maybe?
		//rw_distance = FixedMul( R_PointToDist (curline->v2->x, curline->v2->y), sineval );

		bspcontext->thisdrawseg->scale2 = R_ScaleFromGlobalAngle (viewangle + xtoviewangle[stop], rw_distance );
		bspcontext->thisdrawseg->scalestep = rw_scalestep = 
			(bspcontext->thisdrawseg->scale2 - rw_scale) / (stop-start);
	}
	else
	{
	// UNUSED: try to fix the stretched line bug
#if 0
		if (rw_distance < FRACUNIT/2)
		{
			fixed_t		trx,try;
			fixed_t		gxt,gyt;

			trx = curline->v1->x - viewx;
			try = curline->v1->y - viewy;
			
			gxt = FixedMul(trx,viewcos); 
			gyt = -FixedMul(try,viewsin); 
			bspcontext->thisdrawseg->scale1 = FixedDiv(projection, gxt-gyt)<<detailshift;
		}
#endif
		bspcontext->thisdrawseg->scale2 = bspcontext->thisdrawseg->scale1;
	}

	// calculate texture boundaries
	//  and decide if floor / ceiling marks are needed
	worldtop = bspcontext->frontsector->ceilingheight - viewz;
	worldbottom = bspcontext->frontsector->floorheight - viewz;
	
	loopcontext.midtexture = loopcontext.toptexture = loopcontext.bottomtexture = loopcontext.maskedtexture = 0;
	bspcontext->thisdrawseg->maskedtexturecol = NULL;
	
	if (!bspcontext->backsector)
	{
		// single sided line
		loopcontext.midtexture = texturetranslation[bspcontext->sidedef->midtexture];
		// a single sided line is terminal, so it must mark ends
		loopcontext.markfloor = loopcontext.markceiling = true;
		if (bspcontext->linedef->flags & ML_DONTPEGBOTTOM)
		{
			vtop = bspcontext->frontsector->floorheight +
			textureheight[bspcontext->sidedef->midtexture];
			// bottom of texture at bottom
			rw_midtexturemid = vtop - viewz;	
		}
		else
		{
			// top of texture at top
			rw_midtexturemid = worldtop;
		}
		rw_midtexturemid += bspcontext->sidedef->rowoffset;

		bspcontext->thisdrawseg->silhouette = SIL_BOTH;
		bspcontext->thisdrawseg->sprtopclip = screenheightarray;
		bspcontext->thisdrawseg->sprbottomclip = negonearray;
		bspcontext->thisdrawseg->bsilheight = INT_MAX;
		bspcontext->thisdrawseg->tsilheight = INT_MIN;
	}
	else
	{
		// two sided line
		bspcontext->thisdrawseg->sprtopclip = bspcontext->thisdrawseg->sprbottomclip = NULL;
		bspcontext->thisdrawseg->silhouette = 0;
	
		if (bspcontext->frontsector->floorheight > bspcontext->backsector->floorheight)
		{
			bspcontext->thisdrawseg->silhouette = SIL_BOTTOM;
			bspcontext->thisdrawseg->bsilheight = bspcontext->frontsector->floorheight;
		}
		else if (bspcontext->backsector->floorheight > viewz)
		{
			bspcontext->thisdrawseg->silhouette = SIL_BOTTOM;
			bspcontext->thisdrawseg->bsilheight = INT_MAX;
			// bspcontext->thisdrawseg->sprbottomclip = negonearray;
		}
	
		if (bspcontext->frontsector->ceilingheight < bspcontext->backsector->ceilingheight)
		{
			bspcontext->thisdrawseg->silhouette |= SIL_TOP;
			bspcontext->thisdrawseg->tsilheight = bspcontext->frontsector->ceilingheight;
		}
		else if (bspcontext->backsector->ceilingheight < viewz)
		{
			bspcontext->thisdrawseg->silhouette |= SIL_TOP;
			bspcontext->thisdrawseg->tsilheight = INT_MIN;
			// bspcontext->thisdrawseg->sprtopclip = screenheightarray;
		}
		
		if (bspcontext->backsector->ceilingheight <= bspcontext->frontsector->floorheight)
		{
			bspcontext->thisdrawseg->sprbottomclip = negonearray;
			bspcontext->thisdrawseg->bsilheight = INT_MAX;
			bspcontext->thisdrawseg->silhouette |= SIL_BOTTOM;
		}
	
		if (bspcontext->backsector->floorheight >= bspcontext->frontsector->ceilingheight)
		{
			bspcontext->thisdrawseg->sprtopclip = screenheightarray;
			bspcontext->thisdrawseg->tsilheight = INT_MIN;
			bspcontext->thisdrawseg->silhouette |= SIL_TOP;
		}
	
		worldhigh = bspcontext->backsector->ceilingheight - viewz;
		worldlow = bspcontext->backsector->floorheight - viewz;
		
		// hack to allow height changes in outdoor areas
		if (bspcontext->frontsector->ceilingpic == skyflatnum 
			&& bspcontext->backsector->ceilingpic == skyflatnum)
		{
			worldtop = worldhigh;
		}
	
			
		if (worldlow != worldbottom 
			|| bspcontext->backsector->floorpic != bspcontext->frontsector->floorpic
			|| bspcontext->backsector->lightlevel != bspcontext->frontsector->lightlevel)
		{
			loopcontext.markfloor = true;
		}
		else
		{
			// same plane on both sides
			loopcontext.markfloor = false;
		}
	
			
		if (worldhigh != worldtop 
			|| bspcontext->backsector->ceilingpic != bspcontext->frontsector->ceilingpic
			|| bspcontext->backsector->lightlevel != bspcontext->frontsector->lightlevel)
		{
			loopcontext.markceiling = true;
		}
		else
		{
			// same plane on both sides
			loopcontext.markceiling = false;
		}
	
		if (bspcontext->backsector->ceilingheight <= bspcontext->frontsector->floorheight
			|| bspcontext->backsector->floorheight >= bspcontext->frontsector->ceilingheight)
		{
			// closed door
			loopcontext.markceiling = loopcontext.markfloor = true;
		}
	

		if (worldhigh < worldtop)
		{
			// top texture
			loopcontext.toptexture = texturetranslation[bspcontext->sidedef->toptexture];
			if (bspcontext->linedef->flags & ML_DONTPEGTOP)
			{
			// top of texture at top
				rw_toptexturemid = worldtop;
			}
			else
			{
				vtop =
					bspcontext->backsector->ceilingheight
					+ textureheight[bspcontext->sidedef->toptexture];
		
				// bottom of texture
				rw_toptexturemid = vtop - viewz;	
			}
		}
		if (worldlow > worldbottom)
		{
			// bottom texture
			loopcontext.bottomtexture = texturetranslation[bspcontext->sidedef->bottomtexture];

			if (bspcontext->linedef->flags & ML_DONTPEGBOTTOM )
			{
				// bottom of texture at bottom
				// top of texture at top
				rw_bottomtexturemid = worldtop;
			}
			else	// top of texture at top
			{
				rw_bottomtexturemid = worldlow;
			}
		}
		rw_toptexturemid += bspcontext->sidedef->rowoffset;
		rw_bottomtexturemid += bspcontext->sidedef->rowoffset;
	
		// allocate space for masked texture tables
		if (bspcontext->sidedef->midtexture)
		{
			// masked midtexture
			loopcontext.maskedtexture = true;
			bspcontext->thisdrawseg->maskedtexturecol = loopcontext.maskedtexturecol = lastopening - loopcontext.startx;
			lastopening += loopcontext.stopx - loopcontext.startx;
		}
	}

	// calculate rw_offset (only needed for textured lines)
	loopcontext.segtextured = loopcontext.midtexture | loopcontext.toptexture | loopcontext.bottomtexture | loopcontext.maskedtexture;

	if (loopcontext.segtextured)
	{
		offsetangle = rw_normalangle-rw_angle1;
	
		if (offsetangle > ANG180)
		{
			// Performs the negate op on an unsigned type without a warning
			offsetangle = M_NEGATE( offsetangle );
		}

		if (offsetangle > ANG90)
		{
			offsetangle = ANG90;
		}

		sineval = finesine[offsetangle >>ANGLETOFINESHIFT];
		rw_offset = FixedMul (hyp, sineval);

		if (rw_normalangle-rw_angle1 < ANG180)
			rw_offset = -rw_offset;

		rw_offset += bspcontext->sidedef->textureoffset + bspcontext->curline->offset;
		rw_centerangle = ANG90 + viewangle - rw_normalangle;
	
		// calculate light table
		//  use different light tables
		//  for horizontal / vertical / diagonal
		// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
		if (!fixedcolormap)
		{
			lightnum = (bspcontext->frontsector->lightlevel >> LIGHTSEGSHIFT)+extralight;

			if (bspcontext->curline->v1->y == bspcontext->curline->v2->y)
			{
				lightnum--;
			}
			else if (bspcontext->curline->v1->x == bspcontext->curline->v2->x)
			{
				lightnum++;
			}

			if( fixedcolormapindex )
				walllightsindex = fixedcolormapindex;
			else if (lightnum < 0)
				walllightsindex = 0;
			else if (lightnum >= LIGHTLEVELS)
				walllightsindex = LIGHTLEVELS-1;
			else
				walllightsindex = lightnum;
		
			walllights = scalelight[ walllightsindex ];
		}
	}

	// if a floor / ceiling plane is on the wrong side
	//  of the view plane, it is definitely invisible
	//  and doesn't need to be marked.
	if (bspcontext->frontsector->floorheight >= viewz)
	{
		// above view plane
		loopcontext.markfloor = false;
	}

	if (bspcontext->frontsector->ceilingheight <= viewz && bspcontext->frontsector->ceilingpic != skyflatnum)
	{
		// below view plane
		loopcontext.markceiling = false;
	}


	// calculate incremental stepping values for texture edges
	worldtop >>= 4;
	worldbottom >>= 4;
	
	loopcontext.topstep = -FixedMul (rw_scalestep, worldtop);
	loopcontext.topfrac = (centeryfrac>>4) - FixedMul (worldtop, rw_scale);

	loopcontext.bottomstep = -FixedMul (rw_scalestep,worldbottom);
	loopcontext.bottomfrac = (centeryfrac>>4) - FixedMul (worldbottom, rw_scale);
	
	if (bspcontext->backsector)
	{	
		worldhigh >>= 4;
		worldlow >>= 4;

		if (worldhigh < worldtop)
		{
			loopcontext.pixhigh = (centeryfrac>>4) - FixedMul (worldhigh, rw_scale);
			loopcontext.pixhighstep = -FixedMul (rw_scalestep,worldhigh);
		}
	
		if (worldlow > worldbottom)
		{
			loopcontext.pixlow = (centeryfrac>>4) - FixedMul (worldlow, rw_scale);
			loopcontext.pixlowstep = -FixedMul (rw_scalestep,worldlow);
		}
	}

	// render it
	if (loopcontext.markceiling)
	{
		ceilingplane = R_CheckPlane (ceilingplane, loopcontext.startx, loopcontext.stopx-1);
	}

	if (loopcontext.markfloor)
	{
		floorplane = R_CheckPlane (floorplane, loopcontext.startx, loopcontext.stopx-1);
	}

	R_RenderSegLoop( &loopcontext );

	// save sprite clipping info
	if ( ((bspcontext->thisdrawseg->silhouette & SIL_TOP) || loopcontext.maskedtexture)
		&& !bspcontext->thisdrawseg->sprtopclip)
	{
		memcpy (lastopening, ceilingclip+loopcontext.startx, sizeof(*lastopening)*(loopcontext.stopx-loopcontext.startx));
		bspcontext->thisdrawseg->sprtopclip = lastopening - loopcontext.startx;
		lastopening += loopcontext.stopx - loopcontext.startx;
	}

	if ( ((bspcontext->thisdrawseg->silhouette & SIL_BOTTOM) || loopcontext.maskedtexture)
		&& !bspcontext->thisdrawseg->sprbottomclip)
	{
		memcpy (lastopening, floorclip+loopcontext.startx, sizeof(*lastopening)*(loopcontext.stopx-loopcontext.startx));
		bspcontext->thisdrawseg->sprbottomclip = lastopening - loopcontext.startx;
		lastopening += loopcontext.stopx - loopcontext.startx;	
	}

	if (loopcontext.maskedtexture && !(bspcontext->thisdrawseg->silhouette&SIL_TOP))
	{
		bspcontext->thisdrawseg->silhouette |= SIL_TOP;
		bspcontext->thisdrawseg->tsilheight = INT_MIN;
	}
	if (loopcontext.maskedtexture && !(bspcontext->thisdrawseg->silhouette&SIL_BOTTOM))
	{
		bspcontext->thisdrawseg->silhouette |= SIL_BOTTOM;
		bspcontext->thisdrawseg->bsilheight = INT_MAX;
	}
	bspcontext->thisdrawseg++;
}


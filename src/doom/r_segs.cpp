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
//	All the clipping: columns, horizontal spans, sky columns.
//

#include <stdio.h>
#include <stdlib.h>

#include "doomdef.h"
#include "m_fixed.h"
#include "doomstat.h"

#include "r_main.h"
#include "r_sky.h"
#include "r_raster.h"
#include "r_local.h"

#include "m_container.h"
#include "m_misc.h"
#include "m_profile.h"

#include "i_system.h"

// OPTIMIZE: closed two sided lines as single sided

typedef struct segloopcontext_s
{
	int32_t			startx;
	int32_t			stopx;

	// True if any of the segs textures might be visible.
	doombool		segtextured;

	doombool		maskedtexture;
	int32_t			toptexture;
	int32_t			bottomtexture;
	int32_t			midtexture;

	// False if the back side is the same plane.
	doombool		markfloor;	
	doombool		markceiling;

	rend_fixed_t	pixhigh;
	rend_fixed_t	pixlow;
	rend_fixed_t	pixhighstep;
	rend_fixed_t	pixlowstep;

	rend_fixed_t	topfrac;
	rend_fixed_t	topstep;

	rend_fixed_t	bottomfrac;
	rend_fixed_t	bottomstep;

	vertclip_t*		maskedtexturecol;

} segloopcontext_t;


#if RANGECHECK
void R_RangeCheckNamed( colcontext_t* context, const char* func )
{
	if( context->yl != context->yh )
	{
		if ((unsigned)context->x >= frame_width
			|| context->yl < 0
			|| context->yh >= frame_height) 
		{
			I_Error ("%s: %i to %i at %i", func, context->yl, context->yh, context->x);
		}
	}
}

#define R_RangeCheck() R_RangeCheckNamed( &wallcolcontext, __FUNCTION__ )

#else // !RANGECHECK

#define R_RangeCheckNamed( context, func )
#define R_RangeCheck()

#endif // RANGECHECK

//
// R_RenderMaskedSegRange
//
void R_RenderMaskedSegRange( rendercontext_t& rendercontext, drawseg_t* ds, int x1, int x2 )
{
	M_PROFILE_FUNC();

	viewpoint_t&		viewpoint		= rendercontext.viewpoint;
	vbuffer_t&			dest			= rendercontext.viewbuffer;
	bspcontext_t&		bspcontext		= rendercontext.bspcontext;
	spritecontext_t&	spritecontext	= rendercontext.spritecontext;

	uint32_t			index;
	column_t*			col;
	int32_t				lightnum;
	int32_t				texnum;
	rend_fixed_t		scalestep;
	vertclip_t*			maskedtexturecol;

	colcontext_t		spritecolcontext = {};

#if RENDER_PERF_GRAPHING
	uint64_t		starttime = I_GetTimeUS();
	uint64_t		endtime;
#endif // RENDER_PERF_GRAPHING

	// Calculate light table.
	// Use different light tables
	//   for horizontal / vertical / diagonal. Diagonal?
	// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
	bspcontext.curline = ds->curline;
	bspcontext.frontsector = bspcontext.curline->frontsector;
	bspcontext.frontsectorinst = &rendsectors[ bspcontext.curline->frontsector->index ];
	bspcontext.backsector = bspcontext.curline->backsector;
	bspcontext.backsectorinst = &rendsectors[ bspcontext.curline->backsector->index ];

	texnum = texturetranslation[ bspcontext.curline->sidedef->midtexture ];

	spritecolcontext.output = dest;
	spritecolcontext.transparency = bspcontext.curline->linedef->transparencymap;
	spritecolcontext.colfunc = bspcontext.curline->linedef->transparencymap == nullptr ? &R_SpriteDrawColumn_Colormap : texturelookup[ texnum ]->transparentwallrender;

	lightnum = (bspcontext.frontsectorinst->lightlevel >> LIGHTSEGSHIFT)+extralight;

	if (bspcontext.curline->v1->y == bspcontext.curline->v2->y)
		lightnum--;
	else if (bspcontext.curline->v1->x == bspcontext.curline->v2->x)
		lightnum++;

	int32_t walllightsindex = fixedcolormapindex ? fixedcolormapindex : M_CLAMP( lightnum, 0, LIGHTLEVELS - 1 );
	int32_t* walllightoffsets = &drs_current->scalelightoffset[ walllightsindex * MAXLIGHTSCALE ];

	maskedtexturecol = ds->maskedtexturecol;

	scalestep = ds->scalestep;
	spritecontext.spryscale = ds->scale1 + ( x1 - ds->x1 ) * scalestep;
	spritecontext.mfloorclip = ds->sprbottomclip;
	spritecontext.mceilingclip = ds->sprtopclip;

	// find positioning
	if (bspcontext.curline->linedef->flags & ML_DONTPEGBOTTOM)
	{
		spritecolcontext.texturemid = M_MAX( bspcontext.frontsectorinst->floorheight, bspcontext.backsectorinst->floorheight );
		spritecolcontext.texturemid += texturelookup[ texnum ]->renderheight - viewpoint.z;
	}
	else
	{
		spritecolcontext.texturemid = M_MIN( bspcontext.frontsectorinst->ceilheight, bspcontext.backsectorinst->ceilheight );
		spritecolcontext.texturemid -= viewpoint.z;
	}
	spritecolcontext.texturemid += rendsides[ bspcontext.curline->sidedef->index ].rowoffset;

	if (fixedcolormap)
	{
		spritecolcontext.colormap = fixedcolormap;
	}

	// draw the columns
	for (spritecolcontext.x = x1 ; spritecolcontext.x <= x2 ; spritecolcontext.x++)
	{
		// calculate lighting
		if (maskedtexturecol[spritecolcontext.x] != MASKEDTEXCOL_INVALID)
		{
			spritecontext.sprtopscreen = drs_current->centeryfrac - RendFixedMul( spritecolcontext.texturemid, spritecontext.spryscale );
			spritecolcontext.iscale = RendFixedDiv( IntToRendFixed( 1 ), spritecontext.spryscale );

			if( !fixedcolormap )
			{
				index = (uint32_t)RendFixedMul( spritecontext.spryscale, drs_current->frame_adjusted_light_mul ) >> RENDLIGHTSCALESHIFT;

				if (index >=  MAXLIGHTSCALE )
					index = MAXLIGHTSCALE-1;

				spritecolcontext.colormap = rendercontext.viewpoint.colormaps + walllightoffsets[index];
			}

			// Mental note: Can't use the optimised funcs until we pre-light sprites etc :=(
			// spritecolcontext.colfunc = colfuncs[ M_MIN( ( dc_iscale >> 12 ), 15 ) ];

			// draw the texture
			col = (column_t *)( R_GetRawColumn( texnum,maskedtexturecol[spritecolcontext.x] ) -3 );
			spritecolcontext.sourceheight = IntToRendFixed( col->length );
			
			R_DrawMaskedColumn( spritecontext, spritecolcontext, col );
			maskedtexturecol[spritecolcontext.x] = MASKEDTEXCOL_INVALID;
		}
		spritecontext.spryscale += scalestep;
	}

#if RENDER_PERF_GRAPHING
	endtime = I_GetTimeUS();
	bspcontext.maskedtimetaken += ( endtime - starttime );
#endif // RENDER_PERF_GRAPHING
}




//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked
//  texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling
//  textures.
// CALLED: CORE LOOPING ROUTINE.
//
#define HEIGHTBITS		( RENDFRACBITS - 4 )
#define HEIGHTUNIT		( 1 << HEIGHTBITS )

// Detail maps show me how may reads are going to happen in any 16-byte block.
extern byte lightlevelmaps[32][256];

uint64_t R_RenderSegLoop( rendercontext_t& rendercontext, wallcontext_t& wallcontext, segloopcontext_t* segcontext )
{
	M_PROFILE_FUNC();

	vbuffer_t& dest					= rendercontext.viewbuffer;
	planecontext_t& planecontext	= rendercontext.planecontext;

	angle_t			angle;
	uint32_t		index;
	int32_t			colormapindex;
	int32_t 		yl;
	int32_t 		yh;
	int32_t 		mid;
	int32_t			texturecolumn = 0;
	int32_t			top;
	int32_t			bottom;
	int32_t			currx = segcontext->startx;

	rend_fixed_t	topfrac = segcontext->topfrac;
	rend_fixed_t	bottomfrac = segcontext->bottomfrac;

	colcontext_t	wallcolcontext;

#if RENDER_PERF_GRAPHING
	uint64_t		starttime = I_GetTimeUS();
	uint64_t		endtime;
#endif // RENDER_PERF_GRAPHING

	wallcolcontext.output = dest;

	for ( ; currx < segcontext->stopx ; currx++)
	{
		// mark floor / ceiling areas
		yl = (int32_t)( (topfrac + HEIGHTUNIT - 1 ) >> HEIGHTBITS );

		// no space above wall?
		if (yl < planecontext.ceilingclip[currx]+1)
		{
			yl = planecontext.ceilingclip[currx]+1;
		}
	
		if (segcontext->markceiling)
		{
			top = planecontext.ceilingclip[currx]+1;
			bottom = yl-1;

			if (bottom >= planecontext.floorclip[currx])
			{
				bottom = planecontext.floorclip[currx]-1;
			}

			if (top <= bottom)
			{
				rasterregion_t* region = planecontext.ceilingregion;
				rasterline_t* line = region->lines + ( currx - segcontext->startx );

				region->miny = M_MIN( top, region->miny );
				region->maxy = M_MAX( bottom, region->maxy );
				line->top = top;
				line->bottom = bottom;
			}
		}
		
		yh = (int32_t)( bottomfrac >> HEIGHTBITS );

		if (yh >= planecontext.floorclip[currx])
		{
			yh = planecontext.floorclip[currx]-1;
		}

		if (segcontext->markfloor)
		{
			top = yh+1;
			bottom = planecontext.floorclip[currx]-1;
			if (top <= planecontext.ceilingclip[currx])
			{
				top = planecontext.ceilingclip[currx]+1;
			}
			if (top <= bottom)
			{
				rasterregion_t* region = planecontext.floorregion;
				rasterline_t* line = region->lines + ( currx - segcontext->startx );

				line->top = top;
				line->bottom = bottom;
				region->miny = M_MIN( top, region->miny );
				region->maxy = M_MAX( bottom, region->maxy );
			}
		}
	
		// texturecolumn and lighting are independent of wall tiers
		if (segcontext->segtextured)
		{
			// calculate texture offset
			// Some walls seem to face backwards anyway, so mask out the angle to the range we want
			angle = ( (wallcontext.centerangle + drs_current->xtoviewangle[currx])>>RENDERANGLETOFINESHIFT) & ( RENDERFINEMASK >> 1 );
			texturecolumn = RendFixedToInt( wallcontext.offset - RendFixedMul( renderfinetangent[ angle ], wallcontext.distance ) );
			// calculate lighting
			index = RendFixedMul( wallcontext.scale, drs_current->frame_adjusted_light_mul ) >> RENDLIGHTSCALESHIFT;

			if (index >=  MAXLIGHTSCALE ) index = MAXLIGHTSCALE-1;

			if( fixedcolormapindex )
			{
				colormapindex = fixedcolormapindex;
			}
			else
			{
				colormapindex = wallcontext.lightsindex < NUMLIGHTCOLORMAPS ? drs_current->scalelightindex[ wallcontext.lightsindex * MAXLIGHTSCALE + index ] : wallcontext.lightsindex;
			}
			wallcolcontext.x = currx;
			wallcolcontext.iscale = RendFixedDiv( IntToRendFixed( 1 ), wallcontext.scale );
			wallcolcontext.colormap = rendercontext.viewpoint.colormaps + colormapindex * 256;
		}

		// draw the wall tiers
		if (segcontext->midtexture && yh >= yl)
		{
			// single sided line
			wallcolcontext.yl = yl;
			wallcolcontext.yh = yh;
			wallcolcontext.texturemid = wallcontext.midtexturemid;
			IF_RENDERLIGHTLEVELS
			{
				wallcolcontext.source = colormapindex >= 32 ? R_GetColumn(segcontext->midtexture,texturecolumn) : lightlevelmaps[ colormapindex ];
				wallcolcontext.sourceheight = colormapindex >= 32 ? texturelookup[ segcontext->midtexture ]->renderheight : IntToRendFixed(16);
				wallcolcontext.colfunc = &R_DrawColumn_Colormap_32;
			}
			else
			{
				wallcolcontext.source = R_GetColumn(segcontext->midtexture,texturecolumn);
				wallcolcontext.sourceheight = texturelookup[ segcontext->midtexture ]->renderheight;
				wallcolcontext.colfunc = texturelookup[ segcontext->midtexture ]->wallrender;
			}
			R_RangeCheck();
			wallcolcontext.colfunc( &wallcolcontext );
			planecontext.ceilingclip[currx] = drs_current->viewheight;
			planecontext.floorclip[currx] = -1;
		}
		else
		{
			// two sided line
			if (segcontext->toptexture)
			{
				// top wall
				mid = (int32_t)( segcontext->pixhigh >> HEIGHTBITS );
				segcontext->pixhigh += segcontext->pixhighstep;

				if (mid >= planecontext.floorclip[currx])
				{
					mid = planecontext.floorclip[currx]-1;
				}

				if (mid >= yl)
				{
					wallcolcontext.yl = yl;
					wallcolcontext.yh = mid;
					wallcolcontext.texturemid = wallcontext.toptexturemid;
					IF_RENDERLIGHTLEVELS
					{
						wallcolcontext.source = colormapindex >= 32 ? R_GetColumn(segcontext->toptexture,texturecolumn) : lightlevelmaps[ colormapindex ];
						wallcolcontext.sourceheight = colormapindex >= 32 ? texturelookup[ segcontext->toptexture ]->renderheight : IntToRendFixed(256);
					}
					else
					{
						wallcolcontext.source = R_GetColumn(segcontext->toptexture,texturecolumn);
						wallcolcontext.sourceheight = texturelookup[ segcontext->toptexture ]->renderheight;
						wallcolcontext.colfunc = texturelookup[ segcontext->toptexture ]->wallrender;
					}
					R_RangeCheck();
					wallcolcontext.colfunc( &wallcolcontext );
					planecontext.ceilingclip[currx] = mid;
				}
				else
				{
					planecontext.ceilingclip[currx] = yl-1;
				}
			}
			else
			{
				// no top wall
				if (segcontext->markceiling)
				{
					planecontext.ceilingclip[currx] = yl-1;
				}
			}
			
			if (segcontext->bottomtexture)
			{
				// bottom wall
				mid = (int32_t)( ( segcontext->pixlow + HEIGHTUNIT - 1 ) >> HEIGHTBITS );
				segcontext->pixlow += segcontext->pixlowstep;

				// no space above wall?
				if (mid <= planecontext.ceilingclip[currx])
					mid = planecontext.ceilingclip[currx]+1;
		
				if (mid <= yh)
				{
					wallcolcontext.yl = mid;
					wallcolcontext.yh = yh;
					wallcolcontext.texturemid = wallcontext.bottomtexturemid;
					IF_RENDERLIGHTLEVELS
					{
						wallcolcontext.source = colormapindex >= 32 ? R_GetColumn(segcontext->bottomtexture,texturecolumn) : lightlevelmaps[ colormapindex ];
						wallcolcontext.sourceheight = colormapindex >= 32 ? texturelookup[ segcontext->bottomtexture ]->renderheight : IntToRendFixed(256);
					}
					else
					{
						wallcolcontext.source = R_GetColumn(segcontext->bottomtexture,texturecolumn);
						wallcolcontext.sourceheight = texturelookup[ segcontext->bottomtexture ]->renderheight;
						wallcolcontext.colfunc = texturelookup[ segcontext->bottomtexture ]->wallrender;
					}
					R_RangeCheck();
					wallcolcontext.colfunc( &wallcolcontext );
					planecontext.floorclip[currx] = mid;
				}
				else
				{
					planecontext.floorclip[currx] = yh+1;
				}
			}
			else
			{
				// no bottom wall
				if (segcontext->markfloor)
				{
					planecontext.floorclip[currx] = yh+1;
				}
			}
			
			if (segcontext->maskedtexture)
			{
				// save texturecol
				//  for backdrawing of masked mid texture
				segcontext->maskedtexturecol[currx] = texturecolumn;
			}
		}
		
		wallcontext.scale += wallcontext.scalestep;
		topfrac += segcontext->topstep;
		bottomfrac += segcontext->bottomstep;
	}

#if RENDER_PERF_GRAPHING
	endtime = I_GetTimeUS();
	return endtime - starttime;
#else
	return 0;
#endif // RENDER_PERF_GRAPHING
}

//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
void R_StoreWallRange( rendercontext_t& rendercontext, wallcontext_t& wallcontext, int32_t start, int32_t stop )
{
	M_PROFILE_FUNC();

	viewpoint_t& viewpoint			= rendercontext.viewpoint;
	bspcontext_t& bspcontext		= rendercontext.bspcontext;
	planecontext_t& planecontext	= rendercontext.planecontext;

	rend_fixed_t		hyp;
	rend_fixed_t		sineval;
	angle_t				distangle, offsetangle;
	int32_t				lightnum;
	
	rend_fixed_t		vtop;
	rend_fixed_t		worldtop;
	rend_fixed_t		worldbottom;
	rend_fixed_t		worldhigh;
	rend_fixed_t		worldlow;

	segloopcontext_t	loopcontext;

	uint64_t			walltime;
#if RENDER_PERF_GRAPHING
	uint64_t			starttime = I_GetTimeUS();
	uint64_t			endtime;
	uint64_t			visplanestart;
	uint64_t			visplaneend;
#endif // RENDER_PERF_GRAPHING

	rasterregion_t* ceil = nullptr;
	texturecomposite_t* ceilpic = nullptr;
	bool ceilsky = false;
	rasterregion_t* floor = nullptr;
	texturecomposite_t* floorpic = nullptr;
	bool floorsky = false;

	// don't overflow and crash
	if ( bspcontext.thisdrawseg == &bspcontext.drawsegs[MAXDRAWSEGS])
	{
		return;
	}
		
#if RANGECHECK
	if (start >=drs_current->viewwidth || start > stop)
	{
		I_Error ("Bad R_RenderWallRange: %i to %i", start , stop);
	}
#endif

	{
		M_PROFILE_NAMED( "Setup" );
		bspcontext.sidedef = bspcontext.curline->sidedef;
		bspcontext.sideinst = &rendsides[ bspcontext.curline->sidedef->index ];
		bspcontext.linedef = bspcontext.curline->linedef;

		// mark the segment as visible for auto map
		// Rum and Raisin - This should actually be atomic.
		bspcontext.linedef->flags |= ML_MAPPED;

		// calculate wallcontext.distance for scale calculation
		wallcontext.normalangle = bspcontext.curline->angle + ANG90;
		offsetangle = abs((int32_t)wallcontext.normalangle-(int32_t)wallcontext.angle1);
		offsetangle = M_MIN( offsetangle, ANG90 );

		distangle = ANG90 - offsetangle;
		hyp = R_PointToDist( &viewpoint, bspcontext.curline->v1->rend.x, bspcontext.curline->v1->rend.y );
		sineval = renderfinesine[ distangle >> RENDERANGLETOFINESHIFT ];
		// If this value blows out, renderer go boom. Need to increase resolution of this thing
		wallcontext.distance = RendFixedMul( hyp, sineval );
	
		bspcontext.thisdrawseg->x1 = loopcontext.startx = start;
		bspcontext.thisdrawseg->x2 = stop;
		bspcontext.thisdrawseg->curline = bspcontext.curline;
		loopcontext.stopx = stop+1;

		// calculate scale at both ends and step
		bspcontext.thisdrawseg->scale1 = wallcontext.scale = R_ScaleFromGlobalAngle( viewpoint.angle + drs_current->xtoviewangle[start], wallcontext.distance, viewpoint.angle, wallcontext.normalangle );

		if( stop > start )
		{
			// TODO: calculate second distance? Maybe?
			//wallcontext.distance = FixedMul( R_PointToDist( curline->v2->rend.x, curline->v2->rend.y ), sineval );

			bspcontext.thisdrawseg->scale2 = R_ScaleFromGlobalAngle( viewpoint.angle + drs_current->xtoviewangle[stop], wallcontext.distance, viewpoint.angle, wallcontext.normalangle );
			bspcontext.thisdrawseg->scalestep = wallcontext.scalestep = 
				( bspcontext.thisdrawseg->scale2 - wallcontext.scale ) / ( stop - start );
		}
		else
		{
			bspcontext.thisdrawseg->scale2 = bspcontext.thisdrawseg->scale1;
			bspcontext.thisdrawseg->scalestep = wallcontext.scalestep = 0;
		}

		// calculate texture boundaries
		//  and decide if floor / ceiling marks are needed
		worldtop = bspcontext.frontsectorinst->ceilheight - viewpoint.z;
		worldbottom = bspcontext.frontsectorinst->floorheight - viewpoint.z;
	
		loopcontext.midtexture = loopcontext.toptexture = loopcontext.bottomtexture = loopcontext.maskedtexture = 0;
		bspcontext.thisdrawseg->maskedtexturecol = NULL;
	
		if (!bspcontext.backsector)
		{
			// single sided line
			loopcontext.midtexture = texturetranslation[bspcontext.sidedef->midtexture];
			// a single sided line is terminal, so it must mark ends
			loopcontext.markfloor = loopcontext.markceiling = true;
			if (bspcontext.linedef->flags & ML_DONTPEGBOTTOM && bspcontext.sideinst->midtex)
			{
				vtop = bspcontext.frontsectorinst->floorheight + bspcontext.sideinst->midtex->renderheight;
				// bottom of texture at bottom
				wallcontext.midtexturemid = vtop - viewpoint.z;
			}
			else
			{
				// top of texture at top
				wallcontext.midtexturemid = worldtop;
			}
			wallcontext.midtexturemid += bspcontext.sideinst->rowoffset;

			bspcontext.thisdrawseg->silhouette = SIL_BOTH;
			bspcontext.thisdrawseg->sprtopclip = drs_current->screenheightarray;
			bspcontext.thisdrawseg->sprbottomclip = drs_current->negonearray;
			bspcontext.thisdrawseg->bsilheight = LLONG_MAX;
			bspcontext.thisdrawseg->tsilheight = LLONG_MIN;
		}
		else
		{
			// two sided line
			bspcontext.thisdrawseg->sprtopclip = bspcontext.thisdrawseg->sprbottomclip = NULL;
			bspcontext.thisdrawseg->silhouette = 0;
	
			if (bspcontext.frontsectorinst->floorheight > bspcontext.backsectorinst->floorheight)
			{
				bspcontext.thisdrawseg->silhouette = SIL_BOTTOM;
				bspcontext.thisdrawseg->bsilheight = bspcontext.frontsectorinst->floorheight;
			}
			else if (bspcontext.backsectorinst->floorheight > viewpoint.z )
			{
				bspcontext.thisdrawseg->silhouette = SIL_BOTTOM;
				bspcontext.thisdrawseg->bsilheight = LLONG_MAX;
				// bspcontext.thisdrawseg->sprbottomclip = negonearray;
			}
	
			if (bspcontext.frontsectorinst->ceilheight < bspcontext.backsectorinst->ceilheight)
			{
				bspcontext.thisdrawseg->silhouette |= SIL_TOP;
				bspcontext.thisdrawseg->tsilheight = bspcontext.frontsectorinst->ceilheight;
			}
			else if (bspcontext.backsectorinst->ceilheight < viewpoint.z )
			{
				bspcontext.thisdrawseg->silhouette |= SIL_TOP;
				bspcontext.thisdrawseg->tsilheight = LLONG_MIN;
				// bspcontext.thisdrawseg->sprtopclip = screenheightarray;
			}
		
			if (bspcontext.backsectorinst->ceilheight <= bspcontext.frontsectorinst->floorheight)
			{
				bspcontext.thisdrawseg->sprbottomclip = drs_current->negonearray;
				bspcontext.thisdrawseg->bsilheight = LLONG_MAX;
				bspcontext.thisdrawseg->silhouette |= SIL_BOTTOM;
			}
	
			if (bspcontext.backsectorinst->floorheight >= bspcontext.frontsectorinst->ceilheight)
			{
				bspcontext.thisdrawseg->sprtopclip = drs_current->screenheightarray;
				bspcontext.thisdrawseg->tsilheight = LLONG_MIN;
				bspcontext.thisdrawseg->silhouette |= SIL_TOP;
			}
	
			worldhigh = bspcontext.backsectorinst->ceilheight - viewpoint.z;
			worldlow = bspcontext.backsectorinst->floorheight - viewpoint.z;
		
			// hack to allow height changes in outdoor areas
			if (bspcontext.frontsectorinst->ceiltex == flatlookup[ skyflatnum ]
				&& bspcontext.backsectorinst->ceiltex == flatlookup[ skyflatnum ])
			{
				worldtop = worldhigh;
			}
	
			
			if (worldlow != worldbottom 
				|| bspcontext.backsectorinst->floortex != bspcontext.frontsectorinst->floortex
				|| bspcontext.backsectorinst->floorlightlevel != bspcontext.frontsectorinst->floorlightlevel
				|| bspcontext.backsectorinst->flooroffsetx != bspcontext.frontsectorinst->flooroffsetx
				|| bspcontext.backsectorinst->flooroffsety != bspcontext.frontsectorinst->flooroffsety )
			{
				loopcontext.markfloor = true;
			}
			else
			{
				// same plane on both sides
				loopcontext.markfloor = false;
			}
	
			
			if (worldhigh != worldtop 
				|| bspcontext.backsectorinst->ceiltex != bspcontext.frontsectorinst->ceiltex
				|| bspcontext.backsectorinst->ceillightlevel != bspcontext.frontsectorinst->ceillightlevel
				|| bspcontext.backsectorinst->ceiloffsetx != bspcontext.frontsectorinst->ceiloffsetx
				|| bspcontext.backsectorinst->ceiloffsety != bspcontext.frontsectorinst->ceiloffsety )
			{
				loopcontext.markceiling = true;
			}
			else
			{
				// same plane on both sides
				loopcontext.markceiling = false;
			}
	
			if (bspcontext.backsectorinst->ceilheight <= bspcontext.frontsectorinst->floorheight
				|| bspcontext.backsectorinst->floorheight >= bspcontext.frontsectorinst->ceilheight)
			{
				// closed door
				loopcontext.markceiling = loopcontext.markfloor = true;
			}
	

			if (worldhigh < worldtop)
			{
				// top texture
				loopcontext.toptexture = texturetranslation[bspcontext.sidedef->toptexture];
				if (bspcontext.linedef->flags & ML_DONTPEGTOP)
				{
				// top of texture at top
					wallcontext.toptexturemid = worldtop;
				}
				else
				{
					vtop = bspcontext.backsectorinst->ceilheight + texturelookup[ loopcontext.toptexture ]->renderheight;
		
					// bottom of texture
					wallcontext.toptexturemid = vtop - viewpoint.z;
				}
			}
			if (worldlow > worldbottom)
			{
				// bottom texture
				loopcontext.bottomtexture = texturetranslation[bspcontext.sidedef->bottomtexture];

				if (bspcontext.linedef->flags & ML_DONTPEGBOTTOM )
				{
					// bottom of texture at bottom
					// top of texture at top
					wallcontext.bottomtexturemid = worldtop;
				}
				else	// top of texture at top
				{
					wallcontext.bottomtexturemid = worldlow;
				}
			}
			wallcontext.toptexturemid += bspcontext.sideinst->rowoffset;
			wallcontext.bottomtexturemid += bspcontext.sideinst->rowoffset;

			// allocate space for masked texture tables
			if (bspcontext.sideinst->midtex != NULL)
			{
				// masked midtexture
				loopcontext.maskedtexture = true;
				bspcontext.thisdrawseg->maskedtexturecol = loopcontext.maskedtexturecol = planecontext.lastopening - loopcontext.startx;
				planecontext.lastopening += loopcontext.stopx - loopcontext.startx;
			}
		}

		// calculate wallcontext.offset (only needed for textured lines)
		loopcontext.segtextured = loopcontext.midtexture | loopcontext.toptexture | loopcontext.bottomtexture | loopcontext.maskedtexture;

		if (loopcontext.segtextured)
		{
			offsetangle = wallcontext.normalangle-wallcontext.angle1;
	
			if (offsetangle > ANG180)
			{
				// Performs the negate op on an unsigned type without a warning
				offsetangle = M_NEGATE( offsetangle );
			}

			if (offsetangle > ANG90)
			{
				offsetangle = ANG90;
			}

			sineval = renderfinesine[ offsetangle >> RENDERANGLETOFINESHIFT ];
			wallcontext.offset = RendFixedMul(hyp, sineval);

			if (wallcontext.normalangle-wallcontext.angle1 < ANG180)
				wallcontext.offset = -wallcontext.offset;

			wallcontext.offset += bspcontext.sideinst->coloffset + bspcontext.curline->rend.offset;
			wallcontext.centerangle = ANG90 + viewpoint.angle - wallcontext.normalangle;
	
			// calculate light table
			//  use different light tables
			//  for horizontal / vertical / diagonal
			// OPTIMIZE: get rid of LIGHTSEGSHIFT globally
			if (!fixedcolormap)
			{
				lightnum = (bspcontext.frontsectorinst->lightlevel >> LIGHTSEGSHIFT)+extralight;

				if (bspcontext.curline->v1->y == bspcontext.curline->v2->y)
				{
					lightnum--;
				}
				else if (bspcontext.curline->v1->x == bspcontext.curline->v2->x)
				{
					lightnum++;
				}

				if( fixedcolormapindex )
					wallcontext.lightsindex = fixedcolormapindex;
				else if (lightnum < 0)
					wallcontext.lightsindex = 0;
				else if (lightnum >= LIGHTLEVELS)
					wallcontext.lightsindex = LIGHTLEVELS-1;
				else
					wallcontext.lightsindex = lightnum;
		
				wallcontext.lightsoffset = &drs_current->scalelightoffset[ wallcontext.lightsindex * MAXLIGHTSCALE ];
			}
		}

		// if a floor / ceiling plane is on the wrong side
		//  of the view plane, it is definitely invisible
		//  and doesn't need to be marked.
		if (bspcontext.frontsectorinst->floorheight >= viewpoint.z )
		{
			// above view plane
			loopcontext.markfloor = false;
		}

		if (bspcontext.frontsectorinst->ceilheight <= viewpoint.z && bspcontext.frontsectorinst->ceiltex != flatlookup[ skyflatnum ] )
		{
			// below view plane
			loopcontext.markceiling = false;
		}


		// calculate incremental stepping values for texture edges
		worldtop >>= 4;
		worldbottom >>= 4;
	
		loopcontext.topstep = -RendFixedMul( wallcontext.scalestep, worldtop );
		loopcontext.topfrac = ( drs_current->centeryfrac >> 4 ) - RendFixedMul( worldtop, wallcontext.scale );

		loopcontext.bottomstep = -RendFixedMul( wallcontext.scalestep, worldbottom );
		loopcontext.bottomfrac = ( drs_current->centeryfrac >> 4 ) - RendFixedMul( worldbottom, wallcontext.scale );
	
		if (bspcontext.backsector)
		{	
			worldhigh >>= 4;
			worldlow >>= 4;

			if (worldhigh < worldtop)
			{
				loopcontext.pixhigh = ( drs_current->centeryfrac >> 4 ) - RendFixedMul( worldhigh, wallcontext.scale );
				loopcontext.pixhighstep = -RendFixedMul( wallcontext.scalestep, worldhigh );
			}
	
			if (worldlow > worldbottom)
			{
				loopcontext.pixlow = ( drs_current->centeryfrac >> 4 ) - RendFixedMul( worldlow, wallcontext.scale );
				loopcontext.pixlowstep = -RendFixedMul( wallcontext.scalestep, worldlow );
			}
		}

#if RENDER_PERF_GRAPHING
		visplanestart = I_GetTimeUS();
#endif // RENDER_PERF_GRAPHING

		// render it
		if (loopcontext.markceiling)
		{
			ceil = planecontext.ceilingregion = R_AddNewRasterRegion( planecontext, bspcontext.frontsector->ceilingpic, bspcontext.frontsectorinst->ceilheight, bspcontext.frontsectorinst->ceiloffsetx, bspcontext.frontsectorinst->ceiloffsety, bspcontext.frontsectorinst->ceillightlevel, loopcontext.startx, loopcontext.stopx - 1 );
			ceilpic = bspcontext.frontsectorinst->ceiltex;
			ceilsky = bspcontext.frontsector->ceilingpic == skyflatnum;
		}

		if (loopcontext.markfloor)
		{
			floor = planecontext.floorregion = R_AddNewRasterRegion( planecontext, bspcontext.frontsector->floorpic, bspcontext.frontsectorinst->floorheight, bspcontext.frontsectorinst->flooroffsetx, bspcontext.frontsectorinst->flooroffsety, bspcontext.frontsectorinst->floorlightlevel, loopcontext.startx, loopcontext.stopx - 1 );
			floorpic = bspcontext.frontsectorinst->floortex;
			floorsky = bspcontext.frontsector->floorpic == skyflatnum;
		}

#if RENDER_PERF_GRAPHING
		visplaneend = I_GetTimeUS();
		bspcontext.findvisplanetimetaken += ( visplaneend - visplanestart );
#endif // RENDER_PERF_GRAPHING
	}

	walltime = R_RenderSegLoop( rendercontext, wallcontext, &loopcontext );
#if RENDER_PERF_GRAPHING
	bspcontext.solidtimetaken += walltime;
#endif // RENDER_PERF_GRAPHING

	if( ceil )
	{
		if( !ceilsky )
		{
			ceilpic->floorrender( &rendercontext, ceil, ceilpic );
		}
		else
		{
			R_DrawSky( rendercontext, ceil, bspcontext.frontsector->skyline ? &rendsides[ bspcontext.frontsector->skyline->index ] : nullptr );
		}
	}

	if( floor )
	{
		if( !floorsky )
		{
			floorpic->floorrender( &rendercontext, floor, floorpic );
		}
		else
		{
			R_DrawSky( rendercontext, floor, bspcontext.frontsector->skyline ? &rendsides[ bspcontext.frontsector->skyline->index ] : nullptr );
		}
	}

	{
		M_PROFILE_NAMED( "Reserve openings" );
		// save sprite clipping info
		if ( ((bspcontext.thisdrawseg->silhouette & SIL_TOP) || loopcontext.maskedtexture)
			&& !bspcontext.thisdrawseg->sprtopclip)
		{
			if( ( planecontext.lastopening + loopcontext.stopx - loopcontext.startx ) - planecontext.openings > planecontext.openingscount )
			{
				R_IncreaseOpenings( planecontext );
			}
			memcpy (planecontext.lastopening, planecontext.ceilingclip+loopcontext.startx, sizeof(*planecontext.lastopening)*(loopcontext.stopx-loopcontext.startx));
			bspcontext.thisdrawseg->sprtopclip = planecontext.lastopening - loopcontext.startx;
			planecontext.lastopening += loopcontext.stopx - loopcontext.startx;
		}

		if ( ((bspcontext.thisdrawseg->silhouette & SIL_BOTTOM) || loopcontext.maskedtexture)
			&& !bspcontext.thisdrawseg->sprbottomclip)
		{
			if( ( planecontext.lastopening + loopcontext.stopx - loopcontext.startx ) - planecontext.openings > planecontext.openingscount )
			{
				R_IncreaseOpenings( planecontext );
			}
			memcpy (planecontext.lastopening, planecontext.floorclip+loopcontext.startx, sizeof(*planecontext.lastopening)*(loopcontext.stopx-loopcontext.startx));
			bspcontext.thisdrawseg->sprbottomclip = planecontext.lastopening - loopcontext.startx;
			planecontext.lastopening += loopcontext.stopx - loopcontext.startx;	
		}
	}

	if (loopcontext.maskedtexture && !(bspcontext.thisdrawseg->silhouette&SIL_TOP))
	{
		bspcontext.thisdrawseg->silhouette |= SIL_TOP;
		bspcontext.thisdrawseg->tsilheight = LLONG_MIN;
	}
	if (loopcontext.maskedtexture && !(bspcontext.thisdrawseg->silhouette&SIL_BOTTOM))
	{
		bspcontext.thisdrawseg->silhouette |= SIL_BOTTOM;
		bspcontext.thisdrawseg->bsilheight = LLONG_MAX;
	}
	bspcontext.thisdrawseg++;

#if RENDER_PERF_GRAPHING
	endtime = I_GetTimeUS();

	bspcontext.storetimetaken += ( ( endtime - starttime ) - ( visplaneend - visplanestart ) );
#endif // RENDER_PERF_GRAPHING
}


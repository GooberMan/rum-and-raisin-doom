//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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
//	Gamma correction LUT stuff.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "i_system.h"

#include "doomtype.h"

#include "deh_str.h"
#include "i_input.h"
#include "i_log.h"
#include "i_swap.h"
#include "i_video.h"
#include "m_bbox.h"
#include "m_misc.h"
#include "m_profile.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "r_defs.h"
#include "r_draw.h"

#include "config.h"

#include <png.h>

// TODO: There are separate RANGECHECK defines for different games, but this
// is common code. Fix this.
#define RANGECHECK 1

// Blending table used for fuzzpatch, etc.
// Only used in Heretic/Hexen

byte *tinttable = NULL;

// villsa [STRIFE] Blending table used for Strife
byte *xlatab = NULL;

// The screen buffer that the v_video.c code draws to.

static vbuffer_t default_buffer;
vbuffer_t* dest_buffer;

int dirtybox[4]; 

extern int32_t frame_width;
extern int32_t frame_adjusted_width;
extern int32_t frame_height;
extern int32_t render_width;
extern int32_t render_height;

// haleyjd 08/28/10: clipping callback function for patches.
// This is needed for Chocolate Strife, which clips patches to the screen.
static vpatchclipfunc_t patchclip_callback = NULL;

// TODO: I guess this will break for non-Doom?
void R_VideoEraseRegion( int x, int y, int width, int height );

//
// V_MarkRect 
// 
void V_MarkRect(int x, int y, int width, int height) 
{ 
    // If we are temporarily using an alternate screen, do not 
    // affect the update box.

    if (dest_buffer == &default_buffer)
    {
        M_AddToBox (dirtybox, x, y); 
        M_AddToBox (dirtybox, x + width-1, y + height-1); 
    }
} 
 

//
// V_CopyRect 
// 
void V_CopyRect(int srcx, int srcy, vbuffer_t *source,
                int width, int height,
                int destx, int desty)
{ 
    pixel_t *src;
    pixel_t *dest;

	int adjustedsrcx;
	int adjustedsrcy;
	int adjustedsrcwidth;
	int adjustedsrcheight;
	int adjusteddestx;
	int adjusteddesty;
 
#if RANGECHECK 
    if (srcx < 0
     || srcx + width > V_VIRTUALWIDTH
     || srcy < 0
     || srcy + height > V_VIRTUALHEIGHT 
     || destx < 0
     || destx + width > V_VIRTUALWIDTH
     || desty < 0
     || desty + height > V_VIRTUALHEIGHT)
    {
        I_Error ("Bad V_CopyRect");
    }
#endif 

    V_MarkRect(destx, desty, width, height);

	adjustedsrcx = FixedMul( srcx << FRACBITS, V_WIDTHMULTIPLIER ) >> FRACBITS;
	adjustedsrcy = FixedMul( srcy << FRACBITS, V_HEIGHTMULTIPLIER ) >> FRACBITS;
	adjustedsrcwidth = FixedMul( width << FRACBITS, V_WIDTHMULTIPLIER ) >> FRACBITS;
	adjustedsrcheight = FixedMul( height << FRACBITS, V_HEIGHTMULTIPLIER ) >> FRACBITS;
	adjusteddestx = FixedMul( destx << FRACBITS, V_WIDTHMULTIPLIER ) >> FRACBITS;
	adjusteddesty = FixedMul( desty << FRACBITS, V_HEIGHTMULTIPLIER ) >> FRACBITS;

	src = source->data + source->pitch * adjustedsrcx + adjustedsrcy;
	dest = dest_buffer->data + dest_buffer->pitch * adjusteddestx + adjusteddesty; 
 
	for ( ; adjustedsrcwidth>0 ; adjustedsrcwidth--) 
	{ 
		memcpy(dest, src, adjustedsrcheight * sizeof(*dest));
		src += source->pitch; 
		dest += dest_buffer->pitch; 
	} 
}

//
// V_CopyRect 
// 
// Inflates source buffer to virtual screen space, and transposes it for efficient memcpy
// operations. Written for 64x64 flats, should handle a linear byte array of any dimensions
//
void V_TransposeBuffer( vbuffer_t* source, vbuffer_t* output, int outputmemzone )
{
	if( source->magic_value != vbuffer_magic )
	{
		I_Error( "V_TransposeBuffer: source not initialised" );
	}

	byte* dest;

	int32_t x;
	int32_t y;

	if( output->width != source->width
		|| output->height != source->height
		|| output->pitch != source->pitch
		|| output->data == NULL )
	{
		if( output->data != NULL )
		{
			Z_Free( output->data );
		}

		output->width = source->height;
		output->height = source->width;
		output->pitch = source->width * source->pixel_size_bytes;
		output->pixel_size_bytes = source->pixel_size_bytes;
		output->data = Z_Malloc( output->width * output->height, outputmemzone, &output->data );
		output->magic_value = vbuffer_magic;
	}
	dest = output->data;

	for( x=0; x < output->width; ++x )
	{
		for( y=0; y < output->height; ++y )
		{
			*dest = source->data[ y * source->pitch + x ];
			++dest;
		}
	}
}

void V_TransposeFlat( const char* flat_name, vbuffer_t* output, int outputmemzone )
{
	vbuffer_t src;

	src.data = W_CacheLumpName ( flat_name , PU_CACHE );
	src.width = src.height = src.pitch = 64;
	src.pixel_size_bytes = 1;
	src.magic_value = vbuffer_magic;

	V_TransposeBuffer( &src, output, outputmemzone );
}

void V_TileBuffer( vbuffer_t* source_buffer, int32_t x, int32_t y, int32_t width, int32_t height )
{
	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );

	colcontext_t	column;

	int32_t			widthdiff = ( frame_width - frame_adjusted_width ) >> 1;
	rend_fixed_t	xscale = RendFixedDiv( IntToRendFixed( 1 ), FixedToRendFixed( V_WIDTHMULTIPLIER ) );
	rend_fixed_t	yscale = FixedToRendFixed( V_HEIGHTMULTIPLIER );

	column.output			= *dest_buffer;
	column.source			= source_buffer->data;
	column.sourceheight		= IntToRendFixed( source_buffer->height );
	column.colormap			= NULL;
	column.translation		= NULL;
	column.transparency		= NULL;
	column.texturemid		= IntToRendFixed( ( V_VIRTUALHEIGHT / 2 ) - y );
	column.iscale			= RendFixedDiv( IntToRendFixed( 1 ), yscale );
	column.yl				= FixedToInt( y * V_HEIGHTMULTIPLIER );
	column.yh				= FixedToInt( M_MIN( y + height, V_VIRTUALHEIGHT ) * V_HEIGHTMULTIPLIER ) - 1;

	rend_fixed_t xwidth		= IntToRendFixed( source_buffer->width );
	rend_fixed_t xsource	= 0;

	column.x = widthdiff + FixedToInt( x * V_WIDTHMULTIPLIER );
	int32_t xstop = widthdiff + FixedToInt( M_MIN( x + width, V_VIRTUALWIDTH ) * V_WIDTHMULTIPLIER );

	for( ; column.x < xstop; ++column.x )
	{
		column.source = source_buffer->data + ( xsource >> RENDFRACBITS ) * source_buffer->pitch;
		R_BackbufferDrawColumn( &column );
		xsource += xscale;
		if( xsource >= xwidth )
		{
			xsource -= xwidth;
		}
	}
}

void V_FillBorder( vbuffer_t* source_buffer, int32_t miny, int32_t maxy )
{
	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );

	int32_t			widthdiff = frame_width - frame_adjusted_width;
	if( widthdiff <= 0 )
	{
		M_PROFILE_POP( __FUNCTION__ );
		return;
	}
	widthdiff >>= 1;

	colcontext_t	column;

	rend_fixed_t	xscale = RendFixedDiv( IntToRendFixed( 1 ), FixedToRendFixed( V_WIDTHMULTIPLIER ) );
	rend_fixed_t	yscale = FixedToRendFixed( V_HEIGHTMULTIPLIER );

	column.output			= *dest_buffer;
	column.source			= source_buffer->data;
	column.sourceheight		= IntToRendFixed( source_buffer->height );
	column.colormap			= NULL;
	column.translation		= NULL;
	column.transparency		= NULL;
	column.texturemid		= IntToRendFixed( ( V_VIRTUALHEIGHT / 2 ) - miny );
	column.iscale			= RendFixedDiv( IntToRendFixed( 1 ), yscale );
	column.yl				= FixedToInt( miny * V_HEIGHTMULTIPLIER );
	column.yh				= FixedToInt( maxy * V_HEIGHTMULTIPLIER ) - 1;

	rend_fixed_t width		= IntToRendFixed( source_buffer->width );
	rend_fixed_t diff		= RendFixedDiv( IntToRendFixed( widthdiff ), FixedToRendFixed( V_HEIGHTMULTIPLIER ) );
	rend_fixed_t xsource	= width % diff;
	if( xsource < 0 )
	{
		xsource += width;
	}
	if( xsource >= width )
	{
		xsource -= width;
	}

	for( column.x = 0; column.x < widthdiff; ++column.x )
	{
		column.source = source_buffer->data + ( xsource >> RENDFRACBITS ) * source_buffer->pitch;
		R_BackbufferDrawColumn( &column );
		xsource += xscale;
		if( xsource >= width )
		{
			xsource -= width;
		}
	}

	xsource	= 0;
	for( column.x = frame_adjusted_width + widthdiff; column.x < frame_width; ++column.x )
	{
		column.source = source_buffer->data + ( xsource >> RENDFRACBITS ) * source_buffer->pitch;
		R_BackbufferDrawColumn( &column );
		xsource += xscale;
		if( xsource >= width )
		{
			xsource -= width;
		}
	}

	M_PROFILE_POP( __FUNCTION__ );
}

//
// V_SetPatchClipCallback
//
// haleyjd 08/28/10: Added for Strife support.
// By calling this function, you can setup runtime error checking for patch 
// clipping. Strife never caused errors by drawing patches partway off-screen.
// Some versions of vanilla DOOM also behaved differently than the default
// implementation, so this could possibly be extended to those as well for
// accurate emulation.
//
void V_SetPatchClipCallback(vpatchclipfunc_t func)
{
    patchclip_callback = func;
}

//
// V_DrawPatch
// Masks a column based masked pic to the screen. 
//
void V_DrawPatch(int x, int y, patch_t *patch)
{ 
	V_DrawPatchClipped(x, y, patch, 0, 0, SHORT(patch->width), SHORT(patch->height));
}

//
// V_DrawPatchClipped
// Only want part of a patch? This is your function. As you'll notice, V_DrawPatch wraps in to this.
//
void V_DrawPatchClipped(int x, int y, patch_t *patch, int clippedx, int clippedy, int clippedwidth, int clippedheight)
{
	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );

	extern int32_t remove_limits;

	colcontext_t	column;

	int32_t			widthdiff = ( frame_width - frame_adjusted_width ) >> 1;
	rend_fixed_t	yscale = ( IntToRendFixed( frame_height ) / V_VIRTUALHEIGHT );

	x -= SHORT( patch->leftoffset );
	y -= SHORT( patch->topoffset );

	// haleyjd 08/28/10: Strife needs silent error checking here.
	if(patchclip_callback)
	{
		if(!patchclip_callback(patch, x, y))
		{
			M_PROFILE_POP( __FUNCTION__ );
			return;
		}
	}

#if RANGECHECK
	if( !comp.drawpatch_unbounded )
	{
		if (x < 0
			|| x + clippedwidth > V_VIRTUALWIDTH
			|| y < 0
			|| y + clippedheight > V_VIRTUALHEIGHT
			|| clippedy > 0)
		{
			I_Error("Bad V_DrawPatch");
		}
	}
#endif

	column.output			= *dest_buffer;
	column.sourceheight		= IntToRendFixed( clippedheight );
	column.colormap			= NULL;
	column.translation		= NULL;
	column.transparency		= NULL;
	column.texturemid		= 0;
	column.iscale			= RendFixedDiv( IntToRendFixed( 1 ), yscale ) + 1;

	rend_fixed_t xwidth		= IntToRendFixed( patch->width );
	rend_fixed_t xsource	= IntToRendFixed( clippedx );

	column.x = widthdiff + FixedToInt( FixedRound( x * V_WIDTHMULTIPLIER ) );
	int32_t xstop = widthdiff + FixedToInt( FixedRound( ( x + clippedwidth ) * V_WIDTHMULTIPLIER ) );

	rend_fixed_t	xscale = RendFixedDiv( IntToRendFixed( clippedwidth ), IntToRendFixed( xstop - column.x ) );

	for( ; column.x < xstop; ++column.x, xsource += xscale )
	{
		if( column.x < 0 ) continue;
		if( column.x >= frame_width ) break;

		if( xsource >= xwidth )
		{
			I_Error( "V_DrawPatchClipped: Error with dimensions calculation" );
		}

		column_t* patchcol = (column_t *)( (byte *)patch + LONG( patch->columnofs[ RendFixedToInt( xsource ) ] ) );

		while( patchcol->topdelta != 0xFF )
		{
			int32_t thisy		= y + patchcol->topdelta;
			column.source		= (byte*)patchcol + 3;
			if( thisy >= 0 || -thisy < patchcol->length )
			{
				if( thisy < 0 )
				{
					column.source += -thisy;
				}
				column.yl			= M_CLAMP( FixedToInt( FixedRound( thisy * V_HEIGHTMULTIPLIER ) ), 0, frame_height - 1 );
				column.yh			= M_CLAMP( FixedToInt( FixedRound( M_MIN( thisy + patchcol->length, V_VIRTUALHEIGHT ) * V_HEIGHTMULTIPLIER ) - 1 ), 0, frame_height - 1 );
				R_BackbufferDrawColumn( &column );
			}

			patchcol = (column_t *)( (byte *)patchcol + patchcol->length + 4 );
		}
	}

	M_PROFILE_POP( __FUNCTION__ );
}

//
// V_DrawPatchFlipped
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//

// Used in Doom 2 finale. Nowhere else
void V_DrawPatchFlipped(int x, int y, patch_t *patch)
{
	M_PROFILE_PUSH( __FUNCTION__, __FILE__, __LINE__ );

	extern int32_t remove_limits;

	colcontext_t	column;

	int32_t			widthdiff = ( frame_width - frame_adjusted_width ) >> 1;
	rend_fixed_t	yscale = ( IntToRendFixed( frame_height ) / V_VIRTUALHEIGHT );

	x -= SHORT( patch->leftoffset );
	y -= SHORT( patch->topoffset );

	// haleyjd 08/28/10: Strife needs silent error checking here.
	if(patchclip_callback)
	{
		if(!patchclip_callback(patch, x, y))
		{
			M_PROFILE_POP( __FUNCTION__ );
			return;
		}
	}

#if RANGECHECK
	if( !comp.drawpatch_unbounded )
	{
		if (x < 0
			|| x + SHORT(patch->width) > V_VIRTUALWIDTH
			|| y < 0
			|| y + SHORT(patch->height) > V_VIRTUALHEIGHT)
		{
			I_Error("Bad V_DrawPatchFlipped");
		}
	}
#endif

	column.output			= *dest_buffer;
	column.sourceheight		= IntToRendFixed( SHORT(patch->height) );
	column.colormap			= NULL;
	column.translation		= NULL;
	column.transparency		= NULL;
	column.texturemid		= 0;
	column.iscale			= RendFixedDiv( IntToRendFixed( 1 ), yscale ) + 1;

	rend_fixed_t xwidth		= IntToRendFixed( patch->width );
	rend_fixed_t xsource	= IntToRendFixed( patch->width ) - 1;

	column.x = widthdiff + FixedToInt( FixedRound( x * V_WIDTHMULTIPLIER ) );
	int32_t xstop = widthdiff + FixedToInt( FixedRound( ( x + SHORT(patch->width) ) * V_WIDTHMULTIPLIER ) );

	rend_fixed_t	xscale = RendFixedDiv( IntToRendFixed( SHORT( patch->width ) ), IntToRendFixed( xstop - column.x ) );

	for( ; column.x < xstop; ++column.x, xsource -= xscale )
	{
		if( column.x < 0 ) continue;
		if( column.x >= frame_width ) break;

		if( xsource < 0 )
		{
			I_Error( "V_DrawPatchFlipped: Error with dimensions calculation" );
		}

		column_t* patchcol = (column_t *)( (byte *)patch + LONG( patch->columnofs[ RendFixedToInt( xsource ) ] ) );

		while( patchcol->topdelta != 0xFF )
		{
			int32_t thisy		= y + patchcol->topdelta;
			column.source		= (byte*)patchcol + 3;
			if( thisy >= 0 || -thisy < patchcol->length )
			{
				if( thisy < 0 )
				{
					column.source += -thisy;
				}
				column.yl			= M_CLAMP( FixedToInt( FixedRound( thisy * V_HEIGHTMULTIPLIER ) ), 0, frame_height - 1 );
				column.yh			= M_CLAMP( FixedToInt( FixedRound( M_MIN( thisy + patchcol->length, V_VIRTUALHEIGHT ) * V_HEIGHTMULTIPLIER ) - 1 ), 0, frame_height - 1 );
				R_BackbufferDrawColumn( &column );
			}

			patchcol = (column_t *)( (byte *)patchcol + patchcol->length + 4 );
		}
	}

	M_PROFILE_POP( __FUNCTION__ );
}



//
// V_DrawPatchDirect
// Draws directly to the screen on the pc. 
//

void V_DrawPatchDirect(int x, int y, patch_t *patch)
{
    V_DrawPatch(x, y, patch); 
} 

//
// V_EraseRegion
// Devirtualises screen coordinates and calls in to R_VideoEraseRegion
void V_EraseRegion(int x, int y, int width, int height)
{
	fixed_t virtualx = FixedMul( x << FRACBITS, V_WIDTHMULTIPLIER );
	virtualx += ( frame_width - frame_adjusted_width ) << ( FRACBITS - 1 );
	fixed_t virtualy = FixedMul( y << FRACBITS, V_HEIGHTMULTIPLIER );
	fixed_t virtualwidth = FixedMul( width << FRACBITS, V_WIDTHMULTIPLIER );
	fixed_t virtualheight = FixedMul( height << FRACBITS, V_HEIGHTMULTIPLIER );

	R_VideoEraseRegion( virtualx >> FRACBITS, virtualy >> FRACBITS, virtualwidth >> FRACBITS, virtualheight >> FRACBITS );
}

//
// V_DrawTLPatch
//
// Masks a column based translucent masked pic to the screen.
//

// Used in Heretic and Hexen
void V_DrawTLPatch(int x, int y, patch_t * patch)
{
#if 0
    int count, col;
    column_t *column;
    pixel_t *desttop, *dest;
    byte *source;
    int w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    if (x < 0
     || x + SHORT(patch->width) > frame_width 
     || y < 0
     || y + SHORT(patch->height) > frame_height)
    {
        I_Error("Bad V_DrawTLPatch");
    }

    col = 0;
	// TODO: THIS NEEDS HELP
    desttop = dest_screen + x * frame_height + y;

    w = SHORT(patch->width);
    for (; col < w; x++, col++, desttop++)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));

        // step through the posts in a column

        while (column->topdelta != 0xff)
        {
            source = (byte *) column + 3;
            dest = desttop + column->topdelta;
            count = column->length;

            while (count--)
            {
                *dest = tinttable[((*dest) << 8) + *source++];
                ++dest;
            }
            column = (column_t *) ((byte *) column + column->length + 4);
        }

		desttop += frame_height;
    }
#endif
}

//
// V_DrawXlaPatch
//
// villsa [STRIFE] Masks a column based translucent masked pic to the screen.
//

void V_DrawXlaPatch(int x, int y, patch_t * patch)
{
#if 0
    int count, col;
    column_t *column;
    pixel_t *desttop, *dest;
    byte *source;
    int w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    if(patchclip_callback)
    {
        if(!patchclip_callback(patch, x, y))
            return;
    }

    col = 0;
    desttop = dest_screen + x * frame_height + y;

    w = SHORT(patch->width);
    for(; col < w; x++, col++, desttop++)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));

        // step through the posts in a column

        while(column->topdelta != 0xff)
        {
            source = (byte *) column + 3;
            dest = desttop + column->topdelta;
            count = column->length;

            while(count--)
            {
                *dest = xlatab[*dest + ((*source) << 8)];
                source++;
                dest++;
            }
            column = (column_t *) ((byte *) column + column->length + 4);
        }

		desttop += frame_height;
	}
#endif
}

//
// V_DrawAltTLPatch
//
// Masks a column based translucent masked pic to the screen.
//

// Used in Hexen
void V_DrawAltTLPatch(int x, int y, patch_t * patch)
{
#if 0
    int count, col;
    column_t *column;
    pixel_t *desttop, *dest;
    byte *source;
    int w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    if (x < 0
     || x + SHORT(patch->width) > frame_width
     || y < 0
     || y + SHORT(patch->height) > frame_height)
    {
        I_Error("Bad V_DrawAltTLPatch");
    }

    col = 0;
    desttop = dest_screen + x * frame_height + y;

    w = SHORT(patch->width);
    for (; col < w; x++, col++, desttop++)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));

        // step through the posts in a column

        while (column->topdelta != 0xff)
        {
            source = (byte *) column + 3;
            dest = desttop + column->topdelta;
            count = column->length;

            while (count--)
            {
                *dest = tinttable[((*dest) << 8) + *source++];
                dest++;
            }
            column = (column_t *) ((byte *) column + column->length + 4);
        }

		desttop += frame_height;
	}
#endif
}

//
// V_DrawShadowedPatch
//
// Masks a column based masked pic to the screen.
//

// Used in Heretic and Hexen
void V_DrawShadowedPatch(int x, int y, patch_t *patch)
{
#if 0
    int count, col;
    column_t *column;
    pixel_t *desttop, *dest;
    byte *source;
    pixel_t *desttop2, *dest2;
    int w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    if (x < 0
     || x + SHORT(patch->width) > frame_width
     || y < 0
     || y + SHORT(patch->height) > frame_height)
    {
        I_Error("Bad V_DrawShadowedPatch");
    }

    col = 0;
    desttop = dest_screen + x * frame_height + y;
    desttop2 = dest_screen + (x + 2) * frame_height + y + 2;

    w = SHORT(patch->width);
    for (; col < w; x++, col++, desttop++, desttop2++)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));

        // step through the posts in a column

        while (column->topdelta != 0xff)
        {
            source = (byte *) column + 3;
            dest = desttop + column->topdelta;
            dest2 = desttop2 + column->topdelta;
            count = column->length;

            while (count--)
            {
                *dest2 = tinttable[((*dest2) << 8)];
                dest2++;
                *dest = *source++;
                dest++;

            }
            column = (column_t *) ((byte *) column + column->length + 4);
        }

		desttop += frame_height;
		desttop2 += frame_height;

    }
#endif
}

//
// Load tint table from TINTTAB lump.
//

void V_LoadTintTable(void)
{
    tinttable = W_CacheLumpName("TINTTAB", PU_STATIC);
}

//
// V_LoadXlaTable
//
// villsa [STRIFE] Load xla table from XLATAB lump.
//

void V_LoadXlaTable(void)
{
    xlatab = W_CacheLumpName("XLATAB", PU_STATIC);
}

//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//

// This seems flaky...
void V_DrawBlock(int x, int y, int width, int height, pixel_t *src)
{ 
	// TODO: Only the screen wipe calls this function in Doom.
	// Strife uses it. May explode for strife.

    pixel_t *dest;
 
#if RANGECHECK 
    if (x < 0
     || x + width >frame_width
     || y < 0
     || y + height > frame_height)
    {
		I_Error ("Bad V_DrawBlock");
    }
#endif 
 
    V_MarkRect (x, y, width, height); 
 
    dest = dest_buffer->data + x * dest_buffer->pitch + y; 

    while (height--) 
    { 
		memcpy (dest, src, width * sizeof(*dest));
		src += height; 
		dest += dest_buffer->pitch; 
    } 
} 

void V_DrawFilledBox(int x, int y, int w, int h, int c)
{
#if 0
    pixel_t *buf, *buf1;
    int x1, y1;

    buf = I_VideoBuffer + SCREENWIDTH * y + x;

    for (y1 = 0; y1 < h; ++y1)
    {
        buf1 = buf;

        for (x1 = 0; x1 < w; ++x1)
        {
            *buf1++ = c;
        }

        buf += SCREENWIDTH;
    }
#endif
}

void V_DrawHorizLine(int x, int y, int w, int c)
{
#if 0
    pixel_t *buf;
    int x1;

    buf = I_VideoBuffer + SCREENHEIGHT * x + y;

    for (x1 = 0; x1 < w; ++x1)
    {
        *buf = c;
		buf += SCREENHEIGHT;
    }
#endif
}

void V_DrawVertLine(int x, int y, int h, int c)
{
#if 0
    pixel_t *buf;
    int y1;

    buf = I_VideoBuffer + SCREENHEIGHT * x + y;

    for (y1 = 0; y1 < h; ++y1)
    {
        *buf++ = c;
    }
#endif
}

void V_DrawBox(int x, int y, int w, int h, int c)
{
    V_DrawHorizLine(x, y, w, c);
    V_DrawHorizLine(x, y+h-1, w, c);
    V_DrawVertLine(x, y, h, c);
    V_DrawVertLine(x+w-1, y, h, c);
}

//
// Draw a "raw" screen (lump containing raw data to blit directly
// to the screen)
//
 
// Used in Heretic and Hexen. It's not a raw screen now innit?
void V_DrawRawScreen(pixel_t *raw)
{
#if 0
    memcpy(dest_screen, raw, SCREENWIDTH * SCREENHEIGHT * sizeof(*dest_screen));
#endif
}

//
// V_Init
// 

// THIS IS EVILLLLLLLLLLLLL
void V_Init (void) 
{ 
	default_buffer.width = render_width;
	default_buffer.height = render_height;
	default_buffer.pitch = render_height;
	default_buffer.data = I_VideoBuffer;
	default_buffer.pixel_size_bytes = 1;
	default_buffer.magic_value = vbuffer_magic;

	V_UseBuffer( &default_buffer );
}

// Set the buffer that the code draws to.

void V_UseBuffer(vbuffer_t *buffer)
{
	if( buffer->magic_value != vbuffer_magic )
	{
		I_Error( "V_UseBuffer: Incorrectly initialised buffer passed." );
	}
    dest_buffer = buffer;
}

// Restore screen buffer to the i_video screen buffer.

void V_RestoreBuffer(void)
{
	dest_buffer = &default_buffer;
	// HACK: V_Init doesn't have valid I_VideoBuffer pointer :-(
	default_buffer.width = render_width;
	default_buffer.height = render_height;
	default_buffer.pitch = render_height;
	default_buffer.data = I_VideoBuffer;
	default_buffer.pixel_size_bytes = 1;
	default_buffer.mode = VB_Transposed;
	default_buffer.verticalscale = 1.2f;
	default_buffer.magic_value = vbuffer_magic;
}

//
// SCREEN SHOTS
//

//
// WritePNGfile
//

static void error_fn(png_structp p, png_const_charp s)
{
    printf("libpng error: %s\n", s);
}

static void warning_fn(png_structp p, png_const_charp s)
{
    printf("libpng warning: %s\n", s);
}

void WritePNGfile(char *filename, pixel_t *data,
                  int width, int height,
                  byte *palette)
{
    png_structp ppng;
    png_infop pinfo;
    png_colorp pcolor;
    FILE *handle;
    int i;

    handle = fopen(filename, "wb");
    if (!handle)
    {
        return;
    }

    ppng = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
                                   error_fn, warning_fn);
    if (!ppng)
    {
        fclose(handle);
        return;
    }

    pinfo = png_create_info_struct(ppng);
    if (!pinfo)
    {
        fclose(handle);
        png_destroy_write_struct(&ppng, NULL);
        return;
    }

    png_init_io(ppng, handle);

    png_set_IHDR(ppng, pinfo, width, height,
                 8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    pcolor = malloc(sizeof(*pcolor) * 256);
    if (!pcolor)
    {
        fclose(handle);
        png_destroy_write_struct(&ppng, &pinfo);
        return;
    }

    for (i = 0; i < 256; i++)
    {
        pcolor[i].red   = *(palette + 3 * i);
        pcolor[i].green = *(palette + 3 * i + 1);
        pcolor[i].blue  = *(palette + 3 * i + 2);
    }

    png_set_PLTE(ppng, pinfo, pcolor, 256);
    free(pcolor);

    png_write_info(ppng, pinfo);

	byte** rowpointers = malloc( sizeof( byte* ) * height );

	byte* curr = data;
	for( i = 0; i < height; ++i )
	{
		rowpointers[ i ] = data;
		data += width;
	}

	png_set_rows( ppng, pinfo, rowpointers );
	png_write_png( ppng, pinfo, PNG_TRANSFORM_IDENTITY, NULL );

	free( rowpointers );

    png_destroy_write_struct(&ppng, &pinfo);
    fclose(handle);
}

//
// V_ScreenShot
//

void V_ScreenShot(const char *format)
{
    int i;
    char lbmname[16]; // haleyjd 20110213: BUG FIX - 12 is too small!
    const char *ext = "png";
    
    // find a file name to save it to

    for (i=0; i<=99; i++)
    {
        M_snprintf(lbmname, sizeof(lbmname), format, i, ext);

        if (!M_FileExists(lbmname))
        {
            break;      // file doesn't exist
        }
    }

    if (i == 100)
    {
        I_LogAddEntry( Log_Error, "V_ScreenShot: Couldn't create a PNG" );
    }

	WritePNGfile(lbmname, I_VideoBuffer,
				render_height, render_width,
				W_CacheLumpName (DEH_String("PLAYPAL"), PU_CACHE));
}

#include "cimguiglue.h"

#define MOUSE_SPEED_BOX_WIDTH  300
#define MOUSE_SPEED_BOX_HEIGHT 15
#define MOUSE_SPEED_BOX_X (frame_width - MOUSE_SPEED_BOX_WIDTH - 10)
#define MOUSE_SPEED_BOX_Y 15

//
// V_DrawMouseSpeedBox
//

static void DrawAcceleratingBox(int speed)
{
    int original_speed;
    int redline_x;
    int linelen;

	extern ImGuiContext* imgui_context;

	ImVec2 start = { 0, 0 };
	ImVec2 end = { 0, MOUSE_SPEED_BOX_HEIGHT };
	ImVec2 cursorpos;

	ImU32 red = 0xFF0000FF;
	ImU32 white = 0xFFFFFFFF; 
	ImU32 yellow = 0xFF00FFFF;

    // Calculate the position of the red threshold line when calibrating
    // acceleration.  This is 1/3 of the way along the box.

    redline_x = MOUSE_SPEED_BOX_WIDTH / 3;

    if (speed >= mouse_threshold)
    {
        // Undo acceleration and get back the original mouse speed
        original_speed = speed - mouse_threshold;
        original_speed = (int) (original_speed / mouse_acceleration);
        original_speed += mouse_threshold;

        linelen = (original_speed * redline_x) / mouse_threshold;
    }
    else
    {
        linelen = (speed * redline_x) / mouse_threshold;
    }

	igText( "Acceleration" );
	igNextColumn();

	igGetCursorScreenPos( &cursorpos );
	start = cursorpos;
	end = cursorpos;
	end.y += MOUSE_SPEED_BOX_HEIGHT;

	// Horizontal "thermometer"
	if (linelen > MOUSE_SPEED_BOX_WIDTH - 1)
	{
		linelen = MOUSE_SPEED_BOX_WIDTH - 1;
	}

    if (linelen < redline_x)
    {
		end.x = cursorpos.x + linelen;
		ImDrawList_AddRectFilled( imgui_context->CurrentWindow->DrawList, start, end, white, 0.f, ImDrawFlags_RoundCornersAll );
    }
    else
    {
		end.x = cursorpos.x + redline_x;
		ImDrawList_AddRectFilled( imgui_context->CurrentWindow->DrawList, start, end, white, 0.f, ImDrawFlags_RoundCornersAll );

		start.x = cursorpos.x + redline_x;
		end.x = cursorpos.x + ( linelen - redline_x );
		ImDrawList_AddRectFilled( imgui_context->CurrentWindow->DrawList, start, end, yellow, 0.f, ImDrawFlags_RoundCornersAll );
	}

	start = cursorpos;
	start.y +=  MOUSE_SPEED_BOX_HEIGHT;

	end = cursorpos;
	end.x += redline_x;
	end.y += MOUSE_SPEED_BOX_HEIGHT + 2;

	ImDrawList_AddRectFilled( imgui_context->CurrentWindow->DrawList, start, end, red, 0.f, ImDrawFlags_RoundCornersAll );

	igNextColumn();
}

// Highest seen mouse turn speed. We scale the range of the thermometer
// according to this value, so that it never exceeds the range. Initially
// this is set to a 1:1 setting where 1 pixel = 1 unit of speed.
static int max_seen_speed = MOUSE_SPEED_BOX_WIDTH - 1;

static void DrawNonAcceleratingBox(int speed)
{
	int32_t linelen;
	extern ImGuiContext* imgui_context;

	ImVec2 start = { 0, 0 };
	ImVec2 end = { 0, MOUSE_SPEED_BOX_HEIGHT };

	igGetCursorScreenPos( &start );
	end = start;
	end.y += MOUSE_SPEED_BOX_HEIGHT;

	if (speed > max_seen_speed)
	{
		max_seen_speed = speed;
	}

	// Draw horizontal "thermometer":
	linelen = speed * (MOUSE_SPEED_BOX_WIDTH - 1) / max_seen_speed;
	end.x = (float_t)linelen;

	igText( "Highest seen speed" );
	igNextColumn();
	ImDrawList_AddRectFilled( imgui_context->CurrentWindow->DrawList, start, end, igColorConvertFloat4ToU32( igGetStyle()->Colors[ ImGuiCol_PlotLines ] ), 0.f, ImDrawFlags_RoundCornersAll );
	igNextColumn();
}

void V_DrawMouseSpeedBox(int speed)
{
    extern int32_t usemouse;

	// If the mouse is turned off, don't draw the box at all.
    if (!usemouse)
    {
        return;
    }

	igColumns( 2, "", false );
	igSetColumnWidth( 0, 150.f );

    // If acceleration is used, draw a box that helps to calibrate the
    // threshold point.
    if (mouse_threshold > 0 && fabs(mouse_acceleration - 1) > 0.01)
    {
        DrawAcceleratingBox(speed);
    }
    else
    {
        DrawNonAcceleratingBox(speed);
    }

	igColumns( 1, "", false );
}


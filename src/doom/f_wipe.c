//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020-2022 Ethan Watson
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
//	Mission begin melt/wipe screen special effect.
//

#include <string.h>

#include "z_zone.h"
#include "i_video.h"
#include "v_video.h"
#include "m_random.h"
#include "m_misc.h"

#include "doomtype.h"

#include "w_wad.h"
#include "deh_str.h"

#include "f_wipe.h"

//
//                       SCREEN WIPE PACKAGE
//

// when zero, stop the wipe
static boolean	go = 0;

static pixel_t*	wipe_scr_start;
static pixel_t*	wipe_scr_end;
static pixel_t*	wipe_scr;

typedef struct hsv_s
{
	double_t hue;
	double_t saturation;
	double_t value;
} hsv_t;

static uint64_t wipe_progress;
static rgb_t* wipe_palette;
static hsv_t* wipe_hsv;

extern int32_t render_width;
extern int32_t render_height;

int
wipe_initColorXForm
( int	width,
  int	height,
  uint64_t	ticks )
{
    return 0;
}

int
wipe_doColorXForm
( int	width,
  int	height,
  uint64_t	ticks )
{
    boolean	changed;
    pixel_t*	w;
    pixel_t*	e;
    int32_t		newval;

    changed = false;
    w = wipe_scr_start;
    e = wipe_scr_end;

    while (w!=wipe_scr_start+width*height)
    {
		if (*w != *e)
		{
			if (*w > *e)
			{
			newval = M_MAX( (int32_t)*w - (int32_t)ticks, 0 );
			if (newval < *e)
				*w = *e;
			else
				*w = (pixel_t)newval;
			changed = true;
			}
			else if (*w < *e)
			{
			newval = M_MIN( (int32_t)*w + (int32_t)ticks, 255 );
			if (newval > *e)
				*w = *e;
			else
				*w = (pixel_t)newval;
			changed = true;
			}
		}
		w++;
		e++;
    }

	memcpy( wipe_scr, wipe_scr_start, width * height * sizeof( pixel_t) );

    return !changed;

}

int
wipe_exitColorXForm
( int	width,
  int	height,
  uint64_t	ticks )
{
    return 0;
}


static int*	y;
#define WIPECOLUMNS 160
#define WIPEROWS 200

int
wipe_initMelt
( int	width,
  int	height,
  uint64_t	ticks )
{
    int i, r;

    // setup initial column positions
    // (y<0 => not ready to scroll yet)
    y = (int *) Z_Malloc(WIPECOLUMNS*sizeof(int), PU_STATIC, 0);
    y[0] = -(M_Random() % 16);
    for (i=1;i<WIPECOLUMNS;i++)
    {
		r = ( M_Random() % 3) - 1;
		y[i] = y[i-1] + r;
		if (y[i] > 0) y[i] = 0;
		else if (y[i] <= -16) y[i] = -15;
    }

    return 0;
}

int
wipe_doMelt
( int	width,
  int	height,
  uint64_t	ticks )
{
    int		col;
    int		j;
    int		dy;

    pixel_t*	s;
    pixel_t*	d;
    boolean	done = true;

	// Scale up and then down to handle arbitrary dimensions with integer math
	int vertblocksize = height * 100 / WIPEROWS;
	int horizblocksize = width * 100 / WIPECOLUMNS;
	int currcol;
	int currcolend;
	int currrow;
	int dytranslated;

	memcpy( wipe_scr, wipe_scr_end, width * height * sizeof( pixel_t ) );

    while (ticks--)
    {
		for (col=0;col<WIPECOLUMNS;col++)
		{
			if (y[col]<0)
			{
				y[col]++;
				done = false;

				currcol = col * horizblocksize / 100;
				currcolend = ( col + 1 ) * horizblocksize / 100;
				for(; currcol < currcolend; ++currcol)
				{
					pixel_t* source = wipe_scr_start + ( currcol * height );
					pixel_t* dest = wipe_scr + ( currcol * height );
				
					memcpy( dest, source, height );
				}
			}
			else if (y[col] < WIPEROWS)
			{
				dy = (y[col] < 16) ? y[col] + 1 : 8;
				if (y[col]+dy >= WIPEROWS) dy = WIPEROWS - y[col];
				y[col] += dy;

				currcol = col * horizblocksize / 100;
				currcolend = ( col + 1 ) * horizblocksize / 100;
				
				currrow = y[col] * vertblocksize / 100;
				
				for(; currcol < currcolend; ++currcol)
				{
					pixel_t* source = wipe_scr_start + ( currcol * height );
					pixel_t* dest = wipe_scr + ( currcol * height ) + currrow;
				
					memcpy( dest, source, height - currrow );
				}

				done = false;
			}
		}
    }

    return done;

}

int
wipe_exitMelt
( int	width,
  int	height,
  uint64_t	ticks )
{
    Z_Free(y);
    Z_Free(wipe_scr_start);
    Z_Free(wipe_scr_end);
    return 0;
}

void ToHSV( hsv_t* dest, rgb_t* source )
{
	pixel_t min = M_MIN( M_MIN( source->r, source->g ), source->b );
	pixel_t max = M_MAX( M_MAX( source->r, source->g ), source->b );

	dest->value = max / 255.0;
	double_t delta = max - min;
	if( delta == 0 )
	{
		dest->hue = dest->saturation = 0;
	}
	else
	{
		dest->saturation = max > 0 ? delta / max : 0;

		if( max == source->r )
		{
			dest->hue = 60 * fmod( ( ( source->g - source->b ) / delta ), 6 );
		}
		else if( max == source->g )
		{
			dest->hue = 60 * ( ( ( source->b - source->r ) / delta ) + 2 );
		}
		else
		{
			dest->hue = 60 * ( ( ( source->r - source->g ) / delta ) + 2 );
		}
	}
}

void Blend( rgb_t* output, rgb_t* lhs, rgb_t* rhs, double_t percent )
{
	output->r = ( 1.0 - percent ) * lhs->r + percent * rhs->r;
	output->g = ( 1.0 - percent ) * lhs->g + percent * rhs->g;
	output->b = ( 1.0 - percent ) * lhs->b + percent * rhs->b;
}

int32_t ClosestMatch( hsv_t* source )
{
	double_t distance = 720.0; // Get a better value;
	int32_t result = -1;

	for( hsv_t* val = wipe_hsv; val < wipe_hsv + 256; ++val )
	{
		double_t thisdistance = M_MIN( fabs( source->value - val->value ), 360 - fabs( val->value - source->value ) );
		if( thisdistance < distance )
		{
			result = val - wipe_hsv;
			distance = thisdistance;
		}
	}

	return result;
}

int wipe_initBlend( int width, int height, uint64_t ticks )
{
	wipe_progress = 0;
	wipe_palette = (rgb_t*)W_CacheLumpName( DEH_String( "PLAYPAL" ), PU_CACHE );
	wipe_hsv = Z_Malloc( sizeof( hsv_t ) * 256, PU_STATIC, NULL );

	for( int32_t ind = 0; ind < 256; ++ind )
	{
		ToHSV( &wipe_hsv[ ind ], &wipe_palette[ ind ] );
	}

	return 1;
}

#define WIPE_BLEND_TICS 50

int wipe_updateBlend( int width, int height, uint64_t ticks )
{
	wipe_progress = M_MIN( wipe_progress + 1, WIPE_BLEND_TICS );
	double_t progress = (double_t)wipe_progress / WIPE_BLEND_TICS;

	pixel_t* lhs = wipe_scr_start;
	pixel_t* rhs = wipe_scr_end;
	pixel_t* dest = wipe_scr;

	pixel_t* end = wipe_scr + ( width * height );

	while( dest < end )
	{
		rgb_t* leftentry = wipe_palette + *lhs;
		rgb_t* rightentry = wipe_palette + *rhs;

		rgb_t currrgb;
		hsv_t currhsv;
		Blend( &currrgb, leftentry, rightentry, progress );
		ToHSV( &currhsv, &currrgb );

		*dest = ClosestMatch( &currhsv );

		++lhs;
		++rhs;
		++dest;
	}

	return wipe_progress >= WIPE_BLEND_TICS;
}

int wipe_exitBlend( int width, int height, uint64_t ticks )
{
	Z_Free( wipe_hsv );

	return 1;
}

int
wipe_StartScreen
( int	x,
  int	y,
  int	width,
  int	height )
{
    wipe_scr_start = Z_Malloc(render_width * render_height * sizeof(*wipe_scr_start), PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_start);
    return 0;
}

int
wipe_EndScreen
( int	x,
  int	y,
  int	width,
  int	height )
{
    wipe_scr_end = Z_Malloc(render_width * render_height * sizeof(*wipe_scr_end), PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_end);
	memcpy( I_VideoBuffer, wipe_scr_start, render_width * render_height * sizeof( pixel_t ) );
    return 0;
}


typedef int (*wipefunc_t)( int, int, uint64_t );

typedef struct wipe_s
{
	wipefunc_t		init;
	wipefunc_t		update;
	wipefunc_t		exit;
} wipe_t;

static wipe_t wipes[] =
{
	{ wipe_initColorXForm,	wipe_doColorXForm,	wipe_exitColorXForm		},
	{ wipe_initMelt,		wipe_doMelt,		wipe_exitMelt			},
	{ wipe_initBlend,		wipe_updateBlend,	wipe_exitBlend			},
};

int
wipe_ScreenWipe
( int	wipeno,
  int	x,
  int	y,
  int	width,
  int	height,
  uint64_t	ticks )
{
    int rc;

	wipe_scr = I_VideoBuffer;

	// initial stuff
	if (!go)
	{
		go = 1;
		memcpy(wipe_scr, wipe_scr_start, width*height*sizeof(*wipe_scr));
		(*wipes[wipeno].init)(width, height, ticks);
	}

	// do a piece of wipe-in
	V_MarkRect(0, 0, width, height);
	rc = (*wipes[wipeno].update)(width, height, ticks);
	//  V_DrawBlock(x, y, 0, width, height, wipe_scr); // DEBUG

	// final stuff
	if (rc)
	{
		go = 0;
		(*wipes[wipeno].exit)(width, height, ticks);
	}

	return !go;
}


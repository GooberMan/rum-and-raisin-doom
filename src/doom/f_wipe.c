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
#include "m_fixed.h"
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


#define WIPECOLUMNS 160
#define WIPEROWS 200

static int32_t ybuff1[ WIPECOLUMNS ];
static int32_t ybuff2[ WIPECOLUMNS ];

static int32_t* curry;
static int32_t* prevy;

int
wipe_initMelt
( int	width,
  int	height,
  uint64_t	ticks )
{
    int i, r;

	curry = ybuff1;
	prevy = ybuff2;

    // setup initial column positions
    // (y<0 => not ready to scroll yet)
    curry[0] = -(M_Random() % 16);
    for (i=1;i<WIPECOLUMNS;i++)
    {
		r = ( M_Random() % 3) - 1;
		curry[i] = curry[i-1] + r;
		if (curry[i] > 0) curry[i] = 0;
		else if (curry[i] <= -16) curry[i] = -15;
    }
	memcpy( prevy, curry, sizeof( curry ) );

    return 0;
}

int
wipe_doMelt
( int	width,
  int	height,
  uint64_t	ticks )
{
	boolean	done = true;

	if( ticks > 0 )
	{
		while( ticks-- )
		{
			int32_t* temp = prevy;
			prevy = curry;
			curry = temp;

			for( int32_t col = 0; col < WIPECOLUMNS; ++col )
			{
				if( prevy[ col ] < 0 )
				{
					curry[ col ] = prevy[ col ] + 1;
					done = false;
				}
				else if( prevy[ col ] < WIPEROWS )
				{
					int32_t dy = ( prevy[ col ] < 16 ) ? prevy[ col ] + 1 : 8;
					curry[ col ] = M_MIN( prevy[ col ] + dy, WIPEROWS );

					done = false;
				}
				else
				{
					curry[ col ] = WIPEROWS;
				}
			}
		}
	}
	else
	{
		for( int32_t col = 0; col < WIPECOLUMNS; ++col )
		{
			done &= curry[ col ] >= WIPEROWS;
		}
	}

	return done;
}

int wipe_renderMelt( int width, int height, rend_fixed_t percent )
{
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

	for( int32_t col = 0; col < WIPECOLUMNS; ++col )
	{
		int32_t current = RendFixedToInt( RendFixedLerp( IntToRendFixed( prevy[ col ] ), IntToRendFixed( curry[ col ] ), percent ) );

		if ( current < 0 )
		{
			currcol = col * horizblocksize / 100;
			currcolend = ( col + 1 ) * horizblocksize / 100;
			for(; currcol < currcolend; ++currcol)
			{
				pixel_t* source = wipe_scr_start + ( currcol * height );
				pixel_t* dest = wipe_scr + ( currcol * height );
				
				memcpy( dest, source, height );
			}
		}
		else if ( current < WIPEROWS )
		{
			currcol = col * horizblocksize / 100;
			currcolend = ( col + 1 ) * horizblocksize / 100;
				
			currrow = current * vertblocksize / 100;
				
			for(; currcol < currcolend; ++currcol)
			{
				pixel_t* source = wipe_scr_start + ( currcol * height );
				pixel_t* dest = wipe_scr + ( currcol * height ) + currrow;
				
				memcpy( dest, source, height - currrow );
			}

			done = false;
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
    Z_Free(wipe_scr_start);
    Z_Free(wipe_scr_end);
    return 0;
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
typedef int (*wiperendfunc_t)( int, int, rend_fixed_t );

typedef struct wipe_s
{
	wipefunc_t		init;
	wipefunc_t		update;
	wiperendfunc_t	render;
	wipefunc_t		exit;
} wipe_t;

static wipe_t wipes[] =
{
	{ wipe_initColorXForm,	wipe_doColorXForm,	NULL,				wipe_exitColorXForm		},
	{ wipe_initMelt,		wipe_doMelt,		wipe_renderMelt,	wipe_exitMelt			},
};

int
wipe_ScreenWipe
( int	wipeno,
  int	x,
  int	y,
  int	width,
  int	height,
  uint64_t	ticks,
  rend_fixed_t framepercent )
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
	rc = (*wipes[wipeno].render)(width, height, framepercent );

	// final stuff
	if (rc)
	{
		go = 0;
		(*wipes[wipeno].exit)(width, height, ticks);
	}

	return !go;
}


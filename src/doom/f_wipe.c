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

#include "doomtype.h"

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
    memcpy(wipe_scr, wipe_scr_start, width*height*sizeof(*wipe_scr));
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
    uint64_t		newval;

    changed = false;
    w = wipe_scr;
    e = wipe_scr_end;
    
    while (w!=wipe_scr+width*height)
    {
	if (*w != *e)
	{
	    if (*w > *e)
	    {
		newval = *w - ticks;
		if (newval < *e)
		    *w = *e;
		else
		    *w = (pixel_t)newval;
		changed = true;
	    }
	    else if (*w < *e)
	    {
		newval = *w + ticks;
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

    // copy start screen to main screen
    memcpy(wipe_scr, wipe_scr_start, width*height*sizeof(*wipe_scr));

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
    static int (*wipes[])(int, int, uint64_t) =
    {
	wipe_initColorXForm,
	wipe_doColorXForm,
	wipe_exitColorXForm,

	wipe_initMelt,
	wipe_doMelt,
	wipe_exitMelt
    };

	wipe_scr = I_VideoBuffer;

    // initial stuff
    if (!go)
    {
	go = 1;
	// wipe_scr = (pixel_t *) Z_Malloc(width*height, PU_STATIC, 0); // DEBUG
	(*wipes[wipeno*3])(width, height, ticks);
    }

    // do a piece of wipe-in
    V_MarkRect(0, 0, width, height);
    rc = (*wipes[wipeno*3+1])(width, height, ticks);
    //  V_DrawBlock(x, y, 0, width, height, wipe_scr); // DEBUG

    // final stuff
    if (rc)
    {
	go = 0;
	(*wipes[wipeno*3+2])(width, height, ticks);
    }

    return !go;
}


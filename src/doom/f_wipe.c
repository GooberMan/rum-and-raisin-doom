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

int
wipe_initColorXForm
( int	width,
  int	height,
  int	ticks )
{
    memcpy(wipe_scr, wipe_scr_start, width*height*sizeof(*wipe_scr));
    return 0;
}

int
wipe_doColorXForm
( int	width,
  int	height,
  int	ticks )
{
    boolean	changed;
    pixel_t*	w;
    pixel_t*	e;
    int		newval;

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
		    *w = newval;
		changed = true;
	    }
	    else if (*w < *e)
	    {
		newval = *w + ticks;
		if (newval > *e)
		    *w = *e;
		else
		    *w = newval;
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
  int	ticks )
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
  int	ticks )
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
  int	ticks )
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

    while (ticks--)
    {
		for (col=0;col<WIPECOLUMNS;col++)
		{
			if (y[col]<0)
			{
				y[col]++;
				done = false;
			}
			else if (y[col] < WIPEROWS)
			{
				dy = (y[col] < 16) ? y[col] + 1 : 8;
				if (y[col]+dy >= WIPEROWS) dy = WIPEROWS - y[col];

				currrow = y[col] * vertblocksize / 100;
				dytranslated = dy * vertblocksize / 100;

				currcol = col * horizblocksize / 100;
				currcolend = ( col + 1 ) * horizblocksize / 100;

				for(; currcol < currcolend; ++currcol)
				{
					s = &((pixel_t *)wipe_scr_end)[currcol*height+currrow];
					d = &((pixel_t *)wipe_scr)[currcol*height+currrow];

					for (j=dytranslated;j;j--)
					{
						*d++ = *(s++);
					}
				}
				y[col] += dy;
				currrow = y[col] * vertblocksize / 100;

				currcol = col * horizblocksize / 100;
				currcolend = ( col + 1 ) * horizblocksize / 100;

				for(; currcol < currcolend; ++currcol)
				{
					s = &((pixel_t *)wipe_scr_start)[currcol*height];
					d = &((pixel_t *)wipe_scr)[currcol*height+currrow];

					for (j=height-currrow;j;j--)
					{
						*(d++) = *(s++);
					}
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
  int	ticks )
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
    wipe_scr_start = Z_Malloc(SCREENWIDTH * SCREENHEIGHT * sizeof(*wipe_scr_start), PU_STATIC, NULL);
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
    wipe_scr_end = Z_Malloc(SCREENWIDTH * SCREENHEIGHT * sizeof(*wipe_scr_end), PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_end);
    V_DrawBlock(x, y, width, height, wipe_scr_start); // restore start scr.
    return 0;
}

int
wipe_ScreenWipe
( int	wipeno,
  int	x,
  int	y,
  int	width,
  int	height,
  int	ticks )
{
    int rc;
    static int (*wipes[])(int, int, int) =
    {
	wipe_initColorXForm, wipe_doColorXForm, wipe_exitColorXForm,
	wipe_initMelt, wipe_doMelt, wipe_exitMelt
    };

    // initial stuff
    if (!go)
    {
	go = 1;
	// wipe_scr = (pixel_t *) Z_Malloc(width*height, PU_STATIC, 0); // DEBUG
	wipe_scr = I_VideoBuffer;
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


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
// DESCRIPTION:  heads-up text and input code
//


#include <ctype.h>

#include "doomdef.h"
#include "doomkeys.h"

#include "v_video.h"
#include "i_swap.h"

#include "hu_lib.h"
#include "r_local.h"
#include "r_draw.h"

#include "i_log.h"

// doombool : whether the screen is always erased
#define noterased viewwindowx

extern doombool	automapactive;	// in AM_map.c

void HUlib_init(void)
{
}

void HUlib_clearTextLine(hu_textline_t* t)
{
    t->len = 0;
    t->l[0] = 0;
    t->needsupdate = true;
}

void
HUlib_initTextLine
( hu_textline_t*	t,
  int			x,
  int			y,
  patch_t**		f,
  int			sc )
{
    t->x = x;
    t->y = y;
    t->f = f;
    t->sc = sc;
    HUlib_clearTextLine(t);
}

doombool
HUlib_addCharToTextLine
( hu_textline_t*	t,
  char			ch )
{

    if (t->len == HU_MAXLINELENGTH)
	return false;
    else
    {
	t->l[t->len++] = ch;
	t->l[t->len] = 0;
	t->needsupdate = 4;
	return true;
    }

}

doombool HUlib_delCharFromTextLine(hu_textline_t* t)
{

    if (!t->len) return false;
    else
    {
	t->l[--t->len] = 0;
	t->needsupdate = 4;
	return true;
    }

}

void
HUlib_drawTextLine
( hu_textline_t*	l,
  doombool		drawcursor )
{

    int			i;
    int			w;
    int			x;
    unsigned char	c;

    // draw the new stuff
    x = l->x;
    for (i=0;i<l->len;i++)
    {
	c = toupper(l->l[i]);
	if (c != ' '
	    && c >= l->sc
	    && c <= '_')
	{
	    w = SHORT(l->f[c - l->sc]->width);
	    if (x+w > V_VIRTUALWIDTH)
		break;
	    V_DrawPatchDirect(x, l->y, l->f[c - l->sc]);
	    x += w;
	}
	else
	{
	    x += 4;
	    if (x >= V_VIRTUALWIDTH)
		break;
	}
    }

    // draw the cursor if requested
    if (drawcursor
	&& x + SHORT(l->f['_' - l->sc]->width) <= V_VIRTUALWIDTH)
    {
	V_DrawPatchDirect(x, l->y, l->f['_' - l->sc]);
    }
}


// sorta called by HU_Erase and just better darn get things straight
void HUlib_eraseTextLine(hu_textline_t* l)
{
    int			lh;
    int			y;

    int			viewx = FixedDiv( drs_current->viewwindowx, V_WIDTHMULTIPLIER );
	int			viewy = FixedDiv( drs_current->viewwindowy, V_HEIGHTMULTIPLIER );
	int			viewh = FixedDiv( drs_current->viewheight, V_HEIGHTMULTIPLIER );

    // Only erases when NOT in automap and the screen is reduced,
    // and the text must either need updating or refreshing
    // (because of a recent change back from the automap)

    if (!automapactive &&
	drs_current->viewwindowx && l->needsupdate)
    {
	lh = SHORT(l->f[0]->height) + 1;
	for ( y=l->y; y< l->y + lh; y++ )
	{
	    if (y < viewy || y >= viewy + viewh)
			V_EraseRegion(0, y, V_VIRTUALWIDTH, 1); // erase entire line
	    else
	    {
			V_EraseRegion(0, y, viewx, 1); // erase left border
			V_EraseRegion(V_VIRTUALWIDTH - viewx, y, viewx, 1); // erase right border
	    }
	}
    }

    if (l->needsupdate) l->needsupdate--;

}

void
HUlib_initSText
( hu_stext_t*	s,
  int		x,
  int		y,
  int		h,
  patch_t**	font,
  int		startchar,
  doombool*	on )
{

    int i;

    s->h = h;
    s->on = on;
    s->laston = true;
    s->cl = 0;
    for (i=0;i<h;i++)
	HUlib_initTextLine(&s->l[i],
			   x, y - i*(SHORT(font[0]->height)+1),
			   font, startchar);

}

void HUlib_addLineToSText(hu_stext_t* s)
{

    int i;

    // add a clear line
    if (++s->cl == s->h)
	s->cl = 0;
    HUlib_clearTextLine(&s->l[s->cl]);

    // everything needs updating
    for (i=0 ; i<s->h ; i++)
	s->l[i].needsupdate = 4;

}

void
HUlib_addMessageToSText
( hu_stext_t*	s,
  const char*	prefix,
  const char*	msg )
{
    HUlib_addLineToSText(s);
    if (prefix)
	while (*prefix)
	    HUlib_addCharToTextLine(&s->l[s->cl], *(prefix++));

    while (*msg)
	HUlib_addCharToTextLine(&s->l[s->cl], *(msg++));

	I_LogAddEntry( Log_InGameMessage, s->l[s->cl].l );
}

void HUlib_drawSText(hu_stext_t* s)
{
    int i, idx;
    hu_textline_t *l;

    if (!*s->on)
	return; // if not on, don't draw

    // draw everything
    for (i=0 ; i<s->h ; i++)
    {
	idx = s->cl - i;
	if (idx < 0)
	    idx += s->h; // handle queue of lines
	
	l = &s->l[idx];

	// need a decision made here on whether to skip the draw
	HUlib_drawTextLine(l, false); // no cursor, please
    }

}

void HUlib_eraseSText(hu_stext_t* s)
{

    int i;

    for (i=0 ; i<s->h ; i++)
    {
	if (s->laston && !*s->on)
	    s->l[i].needsupdate = 4;
	HUlib_eraseTextLine(&s->l[i]);
    }
    s->laston = *s->on;

}

void
HUlib_initIText
( hu_itext_t*	it,
  int		x,
  int		y,
  patch_t**	font,
  int		startchar,
  doombool*	on )
{
    it->lm = 0; // default left margin is start of text
    it->on = on;
    it->laston = true;
    HUlib_initTextLine(&it->l, x, y, font, startchar);
}


// The following deletion routines adhere to the left margin restriction
void HUlib_delCharFromIText(hu_itext_t* it)
{
    if (it->l.len != it->lm)
	HUlib_delCharFromTextLine(&it->l);
}

void HUlib_eraseLineFromIText(hu_itext_t* it)
{
    while (it->lm != it->l.len)
	HUlib_delCharFromTextLine(&it->l);
}

// Resets left margin as well
void HUlib_resetIText(hu_itext_t* it)
{
    it->lm = 0;
    HUlib_clearTextLine(&it->l);
}

void
HUlib_addPrefixToIText
( hu_itext_t*	it,
  char*		str )
{
    while (*str)
	HUlib_addCharToTextLine(&it->l, *(str++));
    it->lm = it->l.len;
}

// wrapper function for handling general keyed input.
// returns true if it ate the key
doombool
HUlib_keyInIText
( hu_itext_t*	it,
  unsigned char ch )
{
    ch = toupper(ch);

    if (ch >= ' ' && ch <= '_') 
  	HUlib_addCharToTextLine(&it->l, (char) ch);
    else 
	if (ch == KEY_BACKSPACE) 
	    HUlib_delCharFromIText(it);
	else 
	    if (ch != KEY_ENTER) 
		return false; // did not eat key

    return true; // ate the key

}

void HUlib_drawIText(hu_itext_t* it)
{

    hu_textline_t *l = &it->l;

    if (!*it->on)
	return;
    HUlib_drawTextLine(l, true); // draw the line w/ cursor

}

void HUlib_eraseIText(hu_itext_t* it)
{
    if (it->laston && !*it->on)
	it->l.needsupdate = 4;
    HUlib_eraseTextLine(&it->l);
    it->laston = *it->on;
}


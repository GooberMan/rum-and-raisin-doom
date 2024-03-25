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
// 	The status bar widget code.
//

#ifndef __STLIB__
#define __STLIB__


// We are referring to patches.
#include "r_defs.h"

//
// Typedefs of widgets
//

// Number widget

DOOM_C_API typedef struct
{
    // upper right-hand corner
    //  of the number (right-justified)
    int		x;
    int		y;

    // max # of digits in number
    int width;    

    // last number value
    int		oldnum;
    
    // pointer to current value
    int*	num;

    // pointer to doombool stating
    //  whether to update number
    doombool*	on;

    // list of patches for 0-9
    patch_t**	p;

    // user data
    int data;
    
} st_number_t;



// Percent widget ("child" of number widget,
//  or, more precisely, contains a number widget.)
DOOM_C_API typedef struct
{
    // number information
    st_number_t		n;

    // percent sign graphic
    patch_t*		p;
    
} st_percent_t;



// Multiple Icon widget
DOOM_C_API typedef struct
{
     // center-justified location of icons
    int			x;
    int			y;

    // last icon number
    int			oldinum;

    // pointer to current icon
    int*		inum;

    // pointer to doombool stating
    //  whether to update icon
    doombool*		on;

    // list of icons
    patch_t**		p;
    
    // user data
    int			data;
    
} st_multicon_t;




// Binary Icon widget

DOOM_C_API typedef struct
{
    // center-justified location of icon
    int			x;
    int			y;

    // last icon value
    doombool		oldval;

    // pointer to current icon status
    doombool*		val;

    // pointer to doombool
    //  stating whether to update icon
    doombool*		on;  


    patch_t*		p;	// icon
    int			data;   // user data
    
} st_binicon_t;



//
// Widget creation, access, and update routines
//

// Initializes widget library.
// More precisely, initialize STMINUS,
//  everything else is done somewhere else.
//
DOOM_C_API void STlib_init(void);



// Number widget routines
DOOM_C_API void
STlib_initNum
( st_number_t*		n,
  int			x,
  int			y,
  patch_t**		pl,
  int*			num,
  doombool*		on,
  int			width );

DOOM_C_API void
STlib_updateNum
( st_number_t*		n,
  doombool		refresh );


// Percent widget routines
DOOM_C_API void
STlib_initPercent
( st_percent_t*		p,
  int			x,
  int			y,
  patch_t**		pl,
  int*			num,
  doombool*		on,
  patch_t*		percent );


DOOM_C_API void
STlib_updatePercent
( st_percent_t*		per,
  int			refresh );


// Multiple Icon widget routines
DOOM_C_API void
STlib_initMultIcon
( st_multicon_t*	mi,
  int			x,
  int			y,
  patch_t**		il,
  int*			inum,
  doombool*		on );


DOOM_C_API void
STlib_updateMultIcon
( st_multicon_t*	mi,
  doombool		refresh );

// Binary Icon widget routines

DOOM_C_API void
STlib_initBinIcon
( st_binicon_t*		b,
  int			x,
  int			y,
  patch_t*		i,
  doombool*		val,
  doombool*		on );

DOOM_C_API void
STlib_updateBinIcon
( st_binicon_t*		bi,
  doombool		refresh );

#endif

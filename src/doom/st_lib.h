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

#define ALLOW_SBARDEF 1

#if ALLOW_SBARDEF

#if defined( __cplusplus )

#include "m_container.h"

enum sbarnumfonttype_t
{
	sbf_none = -1,
	sbf_mono0,
	sbf_monomax,
	sbf_proportional,
};

struct sbarnumfont_t
{
	const char*			name;
	sbarnumfonttype_t	type;
	int32_t				monowidth;
	patch_t*			numbers[10];
	patch_t*			percent;
	patch_t*			minus;
};

enum sbarconditiontype_t
{
	sbc_none = -1,
	sbc_weaponowned,
	sbc_weaponselected,
	sbc_weaponnotselected,
	sbc_weaponhasammo,
	sbc_selectedweaponhasammo,
	sbc_selectedweaponammotype,
	sbc_weaponslotowned,
	sbc_weaponslotnotowned,
	sbc_weaponslotselected,
	sbc_weaponslotnotselected,
	sbc_itemowned,
	sbc_itemnotowned,
	sbc_featurelevelgreaterequal,
	sbc_featurelevelless,
	sbc_sessiontypeeequal,
	sbc_sessiontypenotequal,
	sbc_modeeequal,
	sbc_modenotequal,
	sbc_hudmodeequal,

	sbc_max,
};

enum sbargamefeatures_t
{
	featuers_none = -1,
	features_vanilla,
	features_limit_removing,
	features_limit_removing_fixed,
	features_boom_2_02,
	features_complevel9,
	features_mbf,
	features_mbf_dehextra,
	features_mbf21,
	features_mbf21_extended,
	features_rnr24,
};

struct sbarcondition_t
{
	int32_t condition;
	int32_t param;
};

enum sbarnumbertype_t
{
	sbn_none = -1,
	sbn_health,
	sbn_armor,
	sbn_frags,
	sbn_ammo,
	sbn_ammoselected,
	sbn_maxammo,
	sbn_weaponammo,
	sbn_weaponmaxammo,

	sbn_max,
};

enum sbarelementtype_t
{
	sbe_none = -1,
	sbe_canvas,
	sbe_graphic,
	sbe_animation,
	sbe_face,
	sbe_facebackground,
	sbe_number,
	sbe_percentage,

	sbe_max,
};

enum sbaralignment_t
{
	sbe_h_left		= 0x00,
	sbe_h_middle	= 0x01,
	sbe_h_right		= 0x02,

	sbe_h_mask		= 0x03,

	sbe_v_top		= 0x00,
	sbe_v_middle	= 0x04,
	sbe_v_bottom	= 0x08,

	sbe_v_mask		= 0x0C,
};

typedef struct sbarelement_s sbarelement_t;

struct sbaranimframe_t
{
	const char*			lump;
	int32_t				tics;
};

struct sbarelement_s
{
	sbarelementtype_t	type;
	int32_t				xpos;
	int32_t				ypos;
	sbaralignment_t		alignment;
	sbarcondition_t*	conditions;
	size_t				numconditions;
	sbarelement_t*		children;
	size_t				numchildren;

	const char*			translation;
	const char*			tranmap;

	// sbe_graphic
	const char*			patch;

	// sbe_animation
	sbaranimframe_t*	frames;
	size_t				numframes;

	// sbe_number, sbe_percentage
	const char*			numfont;
	sbarnumbertype_t	numtype;
	int32_t				numparam;
	int32_t				numlength;
};

struct statusbarbase_t
{
	const char*			fillflat;
	int32_t				height;
	doombool			fullscreen;
	sbarelement_t*		children;
	size_t				numchildren;
};

struct statusbars_t
{
	sbarnumfont_t*		fonts;
	size_t				numfonts;

	statusbarbase_t*	statusbars;
	size_t				numstatusbars;
};

statusbars_t* STlib_LoadSBARDEF();

#endif // defined( __cplusplus )

#endif // ALLOW_SBARDEF

#endif

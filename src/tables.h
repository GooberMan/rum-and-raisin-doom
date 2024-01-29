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
//	Lookup tables.
//	Do not try to look them up :-).
//	In the order of appearance: 
//
//	int finetangent[4096]	- Tangens LUT.
//	 Should work with BAM fairly well (12 of 16bit,
//      effectively, by shifting).
//
//	int finesine[10240]		- Sine lookup.
//	 Guess what, serves as cosine, too.
//	 Remarkable thing is, how to use BAMs with this? 
//
//	int tantoangle[2049]	- ArcTan LUT,
//	  maps tan(angle) to angle fast. Gotta search.	
//    


#ifndef __TABLES__
#define __TABLES__

#include "doomtype.h"
#include "m_fixed.h"

#if defined( __cplusplus )
extern "C" {
#endif // defined( __cplusplus )

#define FINEANGLES				( 8192 )
#define FINEMASK				( FINEANGLES-1 )

#define FINESINECOUNT			( 5*FINEANGLES/4 )
#define FINETANGENTCOUNT		( FINEANGLES/2 )

// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT		( 19 )

#define FINEANGLE( angle )		( ( angle ) >> ANGLETOFINESHIFT )

// Effective size is 10240.
extern const fixed_t finesine[ FINESINECOUNT ];

// Re-use data, is just PI/2 pahse shift.
extern const fixed_t *finecosine;

// Effective size is 4096.
extern const fixed_t finetangent[ FINETANGENTCOUNT ];

// Gamma correction tables.
extern const byte gammatable[5][256];

// Binary Angle Measument, BAM.

#define ANG45           0x20000000
#define ANG90           0x40000000
#define ANG180          0x80000000
#define ANG270          0xc0000000
#define ANG_MAX         0xffffffff

#define ANG1            (ANG45 / 45)
#define ANG60           (ANG180 / 3)

// Heretic code uses this definition as though it represents one 
// degree, but it is not!  This is actually ~1.40 degrees.

#define ANG1_X          0x01000000

#define SLOPERANGE		2048
#define SLOPEBITS		11
#define DBITS			(FRACBITS-SLOPEBITS)
#define TANTOANGLECOUNT ( SLOPERANGE + 1 )

typedef uint32_t angle_t;

// Effective size is 2049;
// The +1 size is to handle the case when x==y
//  without additional checking.
extern const angle_t tantoangle[TANTOANGLECOUNT];


// Utility function, called by R_PointToAngle.
int SlopeDiv_Render(rend_fixed_t num, rend_fixed_t den);
// BSP_PointToAngle calls this. Search reveals it was only called by playsim, so we drop back to normal for it.
int SlopeDiv_Playsim(unsigned int num, unsigned int den);

// Quick hack to get Heretic/Hexen/Strife running again
//#define SlopeDiv( x, y ) SlopeDiv_Playsim( x, y )

// Separate render tables are wanted, since altering the other tables
// will immediately break vanilla compatibility.
#define RENDERQUALITYSHIFT			4
#define RENDERSLOPEQUALITYSHIFT		3

#if RENDERQUALITYSHIFT > 0
	#define RENDERFINEANGLES			( FINEANGLES << RENDERQUALITYSHIFT )
	#define RENDERFINEMASK				( RENDERFINEANGLES-1 )
	#define RENDERFINESINECOUNT			( 5*RENDERFINEANGLES/4 )
	#define RENDERFINETANGENTCOUNT		( RENDERFINEANGLES/2 )
	#define RENDERANGLETOFINESHIFT		( ANGLETOFINESHIFT - RENDERQUALITYSHIFT )

	extern rend_fixed_t					renderfinesine[ RENDERFINESINECOUNT ];
	extern rend_fixed_t					*renderfinecosine;
	extern rend_fixed_t					renderfinetangent[ RENDERFINETANGENTCOUNT ];
#else // RENDERQUALITYSHIFT <= 0
	#define RENDERFINEANGLES			FINEANGLES
	#define RENDERFINEMASK				FINEMASK
	#define RENDERFINESINECOUNT			FINESINECOUNT
	#define RENDERFINETANGENTCOUNT		FINETANGENTCOUNT
	#define RENDERANGLETOFINESHIFT		ANGLETOFINESHIFT

	#define renderfinesine				finesine
	#define renderfinecosine			finecosine
	#define renderfinetangent			finetangent
#endif // RENDERQUALITYSHIFT > 0

#if RENDERSLOPEQUALITYSHIFT > 0
	#define RENDERSLOPERANGE			( SLOPERANGE << RENDERSLOPEQUALITYSHIFT )
	#define RENDERSLOPEBITS				( SLOPEBITS + RENDERSLOPEQUALITYSHIFT )
	#define RENDERDBITS					( FRACBITS - RENDERSLOPEBITS )
	#define RENDERTANTOANGLECOUNT		( RENDERSLOPERANGE + 1 )

	extern angle_t						rendertantoangle[ RENDERTANTOANGLECOUNT ];
#else // RENDERSLOPEQUALITYSHIFT <= 0
	#define RENDERSLOPERANGE			SLOPERANGE
	#define RENDERSLOPEBITS				SLOPEBITS
	#define RENDERDBITS					DBITS
	#define RENDERTANTOANGLECOUNT		TANTOANGLECOUNT

	#define rendertantoangle			tantoangle
#endif // RENDERSLOPEQUALITYSHIFT > 0

#if defined( __cplusplus )
}
#endif // defined( __cplusplus )

#endif


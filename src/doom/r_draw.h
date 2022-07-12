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
//	System specific interface stuff.
//


#ifndef __R_DRAW__
#define __R_DRAW__

#if defined( __cplusplus )
extern "C" {
#endif // defined( __cplusplus )

// The span blitting interface.
// Hook in assembler or system specific BLT
//  here.
void R_DrawColumn ( colcontext_t* context );
void R_DrawColumn_Untranslated ( colcontext_t* context );
void R_LimitRemovingDrawColumn ( colcontext_t* context );
void R_LimitRemovingDrawColumn_Untranslated ( colcontext_t* context );
void R_DrawColumnLow ( colcontext_t* context );

// Rum and raisin extensions.
// Needs a whole overhaul of how we define output buffers.
// For now, let's just SIMD output to the right location.

#define R_SIMD_NONE 0
#define R_SIMD_AVX 1
#define R_SIMD_NEON 2

#if defined( __i386__ ) || defined( __x86_64__ ) || defined( _M_IX86 ) || defined( _M_X64 )
	#define R_DRAWCOLUMN_SIMDOPTIMISED 1
	#define R_SIMD R_SIMD_AVX
#elif defined( __ARM_NEON__ ) || defined( __ARM_NEON )
	#define R_DRAWCOLUMN_SIMDOPTIMISED 0
	#define R_SIMD R_SIMD_NEON
#else
	#define R_DRAWCOLUMN_SIMDOPTIMISED 0
	#define R_SIMD R_SIMD_NONE
#endif

#define R_SIMD_TYPE( x ) ( R_DRAWCOLUMN_SIMDOPTIMISED && R_SIMD == ( R_SIMD_ ## x ) )

#define R_DRAWCOLUMN_DEBUGDISTANCES 0
#define R_DRAWCOLUMN_LIGHTLEVELS 0

#if R_DRAWCOLUMN_SIMDOPTIMISED

void R_DrawColumn_OneSample ( colcontext_t* context );
#endif


// The Spectre/Invisibility effect.
void R_CacheFuzzColumn (void);
void R_DrawFuzzColumn ( colcontext_t* context );
void R_DrawFuzzColumnLow ( colcontext_t* context );

void R_DrawAdjustedFuzzColumn( colcontext_t* context );
void R_DrawHeatwaveFuzzColumn( colcontext_t* context );

// Draw with color translation tables,
//  for player sprite rendering,
//  Green/Red/Blue/Indigo shirts.
void R_DrawTranslatedColumn ( colcontext_t* context );
void R_DrawTranslatedColumnLow ( colcontext_t* context );

void R_VideoEraseRegion( int x, int y, int width, int height );

extern byte*				translationtables;
extern cachedflat_t*		precachedflats;

// Span blitting for rows, floor/ceiling.
// No Sepctre effect needed.
void R_DrawSpan( spancontext_t* context );

// Low resolution mode, 160x200?
void R_DrawSpanLow( spancontext_t* context );


void R_InitBuffer( int width, int height );


// Initialize color translation tables,
//  for player rendering etc.
void R_InitTranslationTables( void );

// Rendering function.
void R_FillBackScreen( void );

// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder( void );

#if defined( __cplusplus )
}
#endif // defined( __cplusplus )

#endif

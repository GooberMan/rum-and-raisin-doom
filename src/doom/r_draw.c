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
//	The actual span/column drawing functions.
//	Here find the main potential for optimization,
//	 e.g. inline assembly, different algorithms.
//




#include "doomdef.h"
#include "deh_main.h"

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "r_local.h"

// Needs access to LFB (guess what).
#include "v_video.h"
#include "st_stuff.h"
#include "i_swap.h"
#include "r_state.h"

// State.
#include "doomstat.h"


// ?
// This does precisely nothing but hard limit what you can do with a given executable
#define MAXWIDTH			(SCREENWIDTH + SCREENWIDTH >> 1)
#define MAXHEIGHT			(SCREENHEIGHT + SCREENHEIGHT >> 1)

// status bar height at bottom of screen
#define SBARHEIGHT		( ( ( (int64_t)( ST_HEIGHT << FRACBITS ) * (int64_t)V_HEIGHTMULTIPLIER ) >> FRACBITS ) >> FRACBITS )

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//  and we need only the base address,
//  and the total size == width*height*depth/8.,
//


byte*		viewimage; 
int		viewwidth;
int		scaledviewwidth;
int		viewheight;
int		viewwindowx;
int		viewwindowy; 
pixel_t*		xlookup[MAXWIDTH];
int		rowofs[MAXHEIGHT]; 

// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//
byte		translations[3][256];	
 
// Backing buffer containing the bezel drawn around the screen and 
// surrounding background.

static vbuffer_t background_data;


//
// R_DrawColumn
// Source is the top of the column to scale.
//
lighttable_t*		dc_colormap; 
int			dc_x; 
int			dc_yl; 
int			dc_yh; 
fixed_t			dc_iscale;
fixed_t			dc_scale;
fixed_t			dc_texturemid;

// first pixel in a column (possibly virtual) 
byte*			dc_source;		

// just for profiling 
int			dccount;

// Rum and raisin extensions
#if R_DRAWCOLUMN_SIMDOPTIMISED && ( defined( __i386__ ) || defined( __x86_64__ ) || defined( _M_IX86 ) || defined( _M_X64 ) )
	#define COLUMN_AVX 1
	#include <nmmintrin.h>
#else
	#define COLUMN_AVX 0
#endif

#define F_MIN( x, y ) ( ( y ) ^ ( ( ( x ) ^ ( y ) ) & -( ( x ) < ( y ) ) ) )
#define F_MAX( x, y ) ( ( x ) ^ ( ( ( x ) ^ ( y ) ) & -( ( x ) < ( y ) ) ) )
#define F_BITMASK( numbits ) ( ~( ~0u << numbits ) )
#define F_BITMASK64( numbits ) ( ~( ~0ull << numbits ) )

#if R_DRAWCOLUMN_SIMDOPTIMISED
void R_DrawColumn_OneSample( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	__m128i sample_increment;

	__m128i sample_zero_three;
	__m128i sample_four_seven;
	__m128i sample_eight_eleven;
	__m128i sample_twelve_fifteen;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest & ( sizeof( __m128i ) - 1 );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale << 4; 
	frac		= dc_texturemid + ( dc_yl - centery ) * ( dc_iscale * 7 );

	sample_increment = _mm_set1_epi32( dc_iscale << 6 );
	sample_zero_three = _mm_set_epi32( frac, frac * 2, frac * 3, frac * 4 );
	sample_four_seven = _mm_add_epi32( sample_zero_three, sample_increment );
	sample_eight_eleven = _mm_add_epi32( sample_four_seven, sample_increment );
	sample_twelve_fifteen = _mm_add_epi32( sample_eight_eleven, sample_increment );

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = currsample;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_TwoSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale << 2; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_ThreeSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 5;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 6;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep * 5;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_FourSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale << 2; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_FiveSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 3;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 3;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 3;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 4;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep * 3;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_SixSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 3;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 3;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 3;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep * 3;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_SevenSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 3;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep * 2;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_EightSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale << 1; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_NineSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep * 2;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_TenSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_ElevenSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_TwelveSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_ThirteenSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_FourteenSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_FifteenSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 2;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}

void R_DrawColumn_SixteenSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	__m128i* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	__m128i prevsample;
	__m128i currsample;
	__m128i writesample;
	__m128i selectmask;

	const __m128i fullmask = _mm_set1_epi8( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( __m128i );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _mm_set_epi64x( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _mm_load_si128( simddest );
	prevsample = _mm_and_si128( selectmask, prevsample );
	selectmask = _mm_xor_si128( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _mm_set1_epi8( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _mm_or_si128( prevsample, _mm_and_si128( currsample, selectmask ) );
		prevsample = _mm_setzero_si128();
		selectmask = fullmask;

		frac += fracstep;
		_mm_store_si128( simddest, writesample );
		//prevsample = currsample;
		curr += 16;
		++simddest;
	} while( curr < dc_yh );
}
#endif // COLUMN_AVX

//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
// 
void R_DrawColumn (void) 
{ 
    int			count; 
    pixel_t*		dest;
    fixed_t		frac;
    fixed_t		fracstep;	 

    count = dc_yh - dc_yl; 

    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows? 
    dest = xlookup[dc_x] + rowofs[dc_yl];  

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do 
    {
		// Re-map color indices from wall texture column
		//  using a lighting/special effects LUT.
		*dest = dc_source[(frac>>FRACBITS)&127];
		
		dest += 1; 
		frac += fracstep;
	
    } while (count--); 
} 

void R_DrawColumn_Untranslated (void) 
{ 
    int			count; 
    pixel_t*		dest;
    fixed_t		frac;
    fixed_t		fracstep;	 

    count = dc_yh - dc_yl; 

    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows? 
    dest = xlookup[dc_x] + rowofs[dc_yl];  

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do 
    {
		// Re-map color indices from wall texture column
		//  using a lighting/special effects LUT.
		*dest = dc_colormap[dc_source[(frac>>FRACBITS)&127]];
		
		dest += 1; 
		frac += fracstep;
	
    } while (count--); 
} 

void R_DrawColumnLow (void) 
{ 
    int			count; 
    pixel_t*	dest;
    pixel_t*	dest2;
    fixed_t		frac;
    fixed_t		fracstep;
	byte sample;

#if COLUMN_AVX
	int algoselect;
#endif // COLUMN_AVX
 
    count = dc_yh - dc_yl; 

#if COLUMN_AVX
	// This should be further down the stack, select d_drawcolumn function based off number of pixels that need to be read for each 16 byte chunk
	algoselect = F_MIN( ( dc_scale ) >> 12, 16 ) >> 1;
#endif

    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows? 
    dest = xlookup[ dc_x * 2 ] + rowofs[dc_yl];  
    dest2 = xlookup[ dc_x * 2 + 1 ] + rowofs[dc_yl];  

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do 
    {
		// Re-map color indices from wall texture column
		//  using a lighting/special effects LUT.
		sample = dc_colormap[dc_source[(frac>>FRACBITS)&127]];
		*dest++ = sample; 
		*dest2++ = sample; 
		frac += fracstep;
	
    } while (count--); 
}


//
// Spectre/Invisibility.
//
#define FUZZTABLE		50 
#define FUZZOFF	(SCREENHEIGHT)


int	fuzzoffset[FUZZTABLE] =
{
    FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
    FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
    FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
    FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
    FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF 
}; 

#if ADJUSTED_FUZZ
#define FUZZ_FRACBASEX 320
#define FUZZ_FRACBASEY 200

fixed_t	fuzzpos = 0;
fixed_t cachedfuzzpos = 0;
fixed_t fuzzstepx = (fixed_t)( ((int64_t)FUZZ_FRACBASEX << FRACBITS) / SCREENWIDTH );
fixed_t fuzzstepy = (fixed_t)( ((int64_t)FUZZ_FRACBASEY << FRACBITS) / SCREENHEIGHT );

#define FIXED_FUZZTABLE ( 50 << FRACBITS )
#else // !ADJUSTED_FUZZ
int fuzzpos = 0;
#endif // ADJUSTED_FUZZ

//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//
void R_CacheFuzzColumn (void)
{
#if ADJUSTED_FUZZ
	cachedfuzzpos = fuzzpos;
#endif // ADJUSTED_FUZZ
}

void R_DrawFuzzColumn (void) 
{ 
#if ADJUSTED_FUZZ
	int			thisfuzzpos;
#else
	#define thisfuzzpos fuzzpos
#endif // ADJUSTED_FUZZ
    int			count; 
    pixel_t*	dest;
    fixed_t		frac;
    fixed_t		fracstep;	 

#if ADJUSTED_FUZZ
	fuzzpos = cachedfuzzpos;
#endif // ADJUSTED_FUZZ
    count = dc_yh - dc_yl; 

	// One interesting side effect of transposing the buffer:
	// It's now the left and right edges of the screen that we need to not sample
	if (!dc_x || dc_x == viewwidth-1)
		return;
		 
    dest = xlookup[dc_x] + rowofs[dc_yl];  

    // Looks familiar.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do 
    {
#if ADJUSTED_FUZZ
		thisfuzzpos = fuzzpos >> FRACBITS;
#endif // ADJUSTED_FUZZ
		// Lookup framebuffer, and retrieve
		//  a pixel that is either one column
		//  left or right of the current one.
		// Add index from colormap to index.
		*dest = colormaps[6*256+dest[fuzzoffset[thisfuzzpos]]]; 

#if ADJUSTED_FUZZ
		// Clamp table lookup index.
		fuzzpos += fuzzstepy;
		if (fuzzpos >= FIXED_FUZZTABLE)
		    fuzzpos = 0;
#else
		if( ++fuzzpos == FUZZTABLE )
			fuzzpos = 0;
#endif
	
		dest += 1;

		frac += fracstep; 
    } while (count--); 
} 

// low detail mode version
 
void R_DrawFuzzColumnLow (void) 
{ 
#if 0
    int			count; 
    pixel_t*		dest;
    pixel_t*		dest2;
    fixed_t		frac;
    fixed_t		fracstep;	 
    int x;

    // Adjust borders. Low... 
    if (!dc_yl) 
	dc_yl = 1;

    // .. and high.
    if (dc_yh == viewheight-1) 
	dc_yh = viewheight - 2; 
		 
    count = dc_yh - dc_yl; 

    // low detail mode, need to multiply by 2
    
    x = dc_x << 1;
    
    dest = ylookup[dc_yl] + columnofs[x];
    dest2 = ylookup[dc_yl] + columnofs[x+1];

    // Looks familiar.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do 
    {
	// Lookup framebuffer, and retrieve
	//  a pixel that is either one column
	//  left or right of the current one.
	// Add index from colormap to index.
	*dest = colormaps[6*256+dest[fuzzoffset[fuzzpos]]]; 
	*dest2 = colormaps[6*256+dest2[fuzzoffset[fuzzpos]]]; 

	// Clamp table lookup index.
	if (++fuzzpos == FUZZTABLE) 
	    fuzzpos = 0;
	
	dest += SCREENWIDTH;
	dest2 += SCREENWIDTH;

	frac += fracstep; 
    } while (count--); 
#endif
} 
 
  
  
 

//
// R_DrawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//
byte*	dc_translation;
byte*	translationtables;

void R_DrawTranslatedColumn (void) 
{ 
    int			count; 
    pixel_t*		dest;
    fixed_t		frac;
    fixed_t		fracstep;	 
 
    count = dc_yh - dc_yl; 

    dest = xlookup[dc_x] + rowofs[dc_yl];  

    // Looks familiar.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Here we do an additional index re-mapping.
    do 
    {
		// Translation tables are used
		//  to map certain colorramps to other ones,
		//  used with PLAY sprites.
		// Thus the "green" ramp of the player 0 sprite
		//  is mapped to gray, red, black/indigo. 
		*dest++ = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
	
		frac += fracstep; 
    } while (count--); 
} 

void R_DrawTranslatedColumnLow (void) 
{ 
#if 0
    int			count; 
    pixel_t*		dest;
    pixel_t*		dest2;
    fixed_t		frac;
    fixed_t		fracstep;	 
    int                 x;
 
    count = dc_yh - dc_yl; 

    // low detail, need to scale by 2
    x = dc_x << 1;
				 
    dest = ylookup[dc_yl] + columnofs[x]; 
    dest2 = ylookup[dc_yl] + columnofs[x+1]; 

    // Looks familiar.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Here we do an additional index re-mapping.
    do 
    {
	// Translation tables are used
	//  to map certain colorramps to other ones,
	//  used with PLAY sprites.
	// Thus the "green" ramp of the player 0 sprite
	//  is mapped to gray, red, black/indigo. 
	*dest = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
	*dest2 = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
	dest += SCREENWIDTH;
	dest2 += SCREENWIDTH;
	
	frac += fracstep; 
    } while (count--); 
#endif
} 




//
// R_InitTranslationTables
// Creates the translation tables to map
//  the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void R_InitTranslationTables (void)
{
    int		i;
	
    translationtables = Z_Malloc (256*3, PU_STATIC, 0);
    
    // translate just the 16 green colors
    for (i=0 ; i<256 ; i++)
    {
	if (i >= 0x70 && i<= 0x7f)
	{
	    // map green ramp to gray, brown, red
	    translationtables[i] = 0x60 + (i&0xf);
	    translationtables [i+256] = 0x40 + (i&0xf);
	    translationtables [i+512] = 0x20 + (i&0xf);
	}
	else
	{
	    // Keep all other colors as is.
	    translationtables[i] = translationtables[i+256] 
		= translationtables[i+512] = i;
	}
    }
}




//
// R_DrawSpan 
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//
int			ds_y; 
int			ds_x1; 
int			ds_x2;

lighttable_t*		ds_colormap; 

fixed_t			ds_xfrac; 
fixed_t			ds_yfrac; 
fixed_t			ds_xstep; 
fixed_t			ds_ystep;

// start of a 64*64 tile image 
byte*			ds_source;	

// just for profiling
int			dscount;


//
// Draws the actual span.
void R_DrawSpan (void) 
{ 
    unsigned int position, step;
    pixel_t *dest;
    int count;
    int spot;
    unsigned int xtemp, ytemp;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1
	|| ds_x1<0
	|| ds_x2>=SCREENWIDTH
	|| (unsigned)ds_y>SCREENHEIGHT)
    {
	I_Error( "R_DrawSpan: %i to %i at %i",
		 ds_x1,ds_x2,ds_y);
    }
//	dscount++;
#endif

    // Pack position and step variables into a single 32-bit integer,
    // with x in the top 16 bits and y in the bottom 16 bits.  For
    // each 16-bit part, the top 6 bits are the integer part and the
    // bottom 10 bits are the fractional part of the pixel position.

    position = ((ds_xfrac << 10) & 0xffff0000)
             | ((ds_yfrac >> 6)  & 0x0000ffff);
    step = ((ds_xstep << 10) & 0xffff0000)
         | ((ds_ystep >> 6)  & 0x0000ffff);

    dest = xlookup[ds_x1] + rowofs[ds_y];

    // We do not check for zero spans here?
    count = ds_x2 - ds_x1;

    do
    {
		// Calculate current texture index in u,v.
		ytemp = (position >> 4) & 0x0fc0;
		xtemp = (position >> 26);
		spot = xtemp | ytemp;

		// Lookup pixel from flat texture tile,
		*dest = ds_source[spot];
		dest += SCREENHEIGHT;

		position += step;

	} while (count--);
}



// UNUSED.
// Loop unrolled by 4.
#if 0
void R_DrawSpan (void) 
{ 
    unsigned	position, step;

    byte*	source;
    byte*	colormap;
    pixel_t*	dest;
    
    unsigned	count;
    usingned	spot; 
    unsigned	value;
    unsigned	temp;
    unsigned	xtemp;
    unsigned	ytemp;
		
    position = ((ds_xfrac<<10)&0xffff0000) | ((ds_yfrac>>6)&0xffff);
    step = ((ds_xstep<<10)&0xffff0000) | ((ds_ystep>>6)&0xffff);
		
    source = ds_source;
    colormap = ds_colormap;
    dest = ylookup[ds_y] + columnofs[ds_x1];	 
    count = ds_x2 - ds_x1 + 1; 
	
    while (count >= 4) 
    { 
	ytemp = position>>4;
	ytemp = ytemp & 4032;
	xtemp = position>>26;
	spot = xtemp | ytemp;
	position += step;
	dest[0] = colormap[source[spot]]; 

	ytemp = position>>4;
	ytemp = ytemp & 4032;
	xtemp = position>>26;
	spot = xtemp | ytemp;
	position += step;
	dest[1] = colormap[source[spot]];
	
	ytemp = position>>4;
	ytemp = ytemp & 4032;
	xtemp = position>>26;
	spot = xtemp | ytemp;
	position += step;
	dest[2] = colormap[source[spot]];
	
	ytemp = position>>4;
	ytemp = ytemp & 4032;
	xtemp = position>>26;
	spot = xtemp | ytemp;
	position += step;
	dest[3] = colormap[source[spot]]; 
		
	count -= 4;
	dest += 4;
    } 
    while (count > 0) 
    { 
	ytemp = position>>4;
	ytemp = ytemp & 4032;
	xtemp = position>>26;
	spot = xtemp | ytemp;
	position += step;
	*dest++ = colormap[source[spot]]; 
	count--;
    } 
} 
#endif


//
// Again..
//
void R_DrawSpanLow (void)
{
#if 0
    unsigned int position, step;
    unsigned int xtemp, ytemp;
    pixel_t *dest;
    int count;
    int spot;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1
	|| ds_x1<0
	|| ds_x2>=SCREENWIDTH
	|| (unsigned)ds_y>SCREENHEIGHT)
    {
	I_Error( "R_DrawSpan: %i to %i at %i",
		 ds_x1,ds_x2,ds_y);
    }
//	dscount++; 
#endif

    position = ((ds_xfrac << 10) & 0xffff0000)
             | ((ds_yfrac >> 6)  & 0x0000ffff);
    step = ((ds_xstep << 10) & 0xffff0000)
         | ((ds_ystep >> 6)  & 0x0000ffff);

    count = (ds_x2 - ds_x1);

    // Blocky mode, need to multiply by 2.
    ds_x1 <<= 1;
    ds_x2 <<= 1;

    dest = ylookup[ds_y] + columnofs[ds_x1];

    do
    {
	// Calculate current texture index in u,v.
        ytemp = (position >> 4) & 0x0fc0;
        xtemp = (position >> 26);
        spot = xtemp | ytemp;

	// Lowres/blocky mode does it twice,
	//  while scale is adjusted appropriately.
	*dest++ = ds_colormap[ds_source[spot]];
	*dest++ = ds_colormap[ds_source[spot]];

	position += step;

    } while (count--);
#endif	
}

//
// R_InitBuffer 
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//
void
R_InitBuffer
( int		width,
  int		height ) 
{ 
    int		i; 

    // Handle resize,
    //  e.g. smaller view windows
    //  with border and/or status bar.
    viewwindowx = (SCREENWIDTH-width) >> 1; 
    // Samw with base row offset.
    if (width == SCREENWIDTH) 
		viewwindowy = 0;
    else 
		viewwindowy = (SCREENHEIGHT-SBARHEIGHT-height) >> 1;

    // Preclaculate all column offsets.
    for (i=0 ; i<width ; i++) 
		xlookup[i] = I_VideoBuffer + (i+viewwindowx)*SCREENHEIGHT; 

    // Row offset. For windows.
    for (i=0 ; i<height ; i++)
		rowofs[i] = viewwindowy + i;

	background_data.width = background_data.height = 0;
	if (background_data.data != NULL)
	{
		Z_Free(background_data.data);
	}

	background_data.data = NULL;
} 
 
 
static void R_RemapBackBuffer( int32_t virtualx, int32_t virtualy, int32_t virtualwidth, int32_t virtualheight, lighttable_t* colormap )
{
	extern vbuffer_t* dest_buffer;

	int32_t numcols = FixedMul( virtualwidth << FRACBITS, V_WIDTHMULTIPLIER ) >> FRACBITS;
	int32_t numrows;

	byte* outputbase = dest_buffer->data + ( FixedMul( virtualx << FRACBITS, V_WIDTHMULTIPLIER ) >> FRACBITS ) * dest_buffer->height
										+ ( FixedMul( virtualy << FRACBITS, V_HEIGHTMULTIPLIER ) >> FRACBITS );
	byte* output;

	for( ; numcols > 0; --numcols )
	{
		output = outputbase;
		numrows = FixedMul( virtualheight << FRACBITS, V_HEIGHTMULTIPLIER ) >> FRACBITS;

		for( ; numrows > 0; --numrows )
		{
			*output = colormap[ *output ];
			++output;
		}
		outputbase += dest_buffer->height;
	}
}

//
// R_FillBackScreen
// Fills the back screen with a pattern
//  for variable screen sizes
// Also draws a beveled edge.
//

void R_FillBackScreen (void) 
{ 
	vbuffer_t src;
	vbuffer_t inflated;
	int32_t width;
	int32_t height;

	int32_t viewx = FixedDiv( viewwindowx << FRACBITS, V_WIDTHMULTIPLIER ) >> FRACBITS;
	int32_t viewy = FixedDiv( viewwindowy << FRACBITS, V_HEIGHTMULTIPLIER ) >> FRACBITS;
	int32_t vieww = FixedDiv( scaledviewwidth << FRACBITS, V_WIDTHMULTIPLIER ) >> FRACBITS;
	int32_t viewh = FixedDiv( viewheight << FRACBITS, V_HEIGHTMULTIPLIER ) >> FRACBITS;

    int		x;
    int		y; 
    patch_t*	patch;

	extern int border_style;
	extern int border_bezel_style;

    // DOOM border patch.
    const char *name1 = DEH_String("FLOOR7_2");

    // DOOM II border patch.
    const char *name2 = DEH_String("GRNROCK");

    const char *name;

    // If we are running full screen, there is no need to do any of this,
    // and the background buffer can be freed if it was previously in use.

    if (scaledviewwidth == SCREENWIDTH)
    {
        if (background_data.data != NULL)
        {
            Z_Free(background_data.data);
            background_data.data = NULL;
        }

	return;
    }

    // Allocate the background buffer if necessary
	
    if (background_data.data == NULL)
    {
        background_data.data = Z_Malloc(SCREENWIDTH * SCREENHEIGHT * sizeof(*background_data.data),
                                     PU_STATIC, NULL);
		background_data.width = SCREENWIDTH;
		background_data.height = SCREENHEIGHT;
    }

    if (gamemode == commercial)
	{
		name = name2;
	}
    else
	{
		name = name1;
	}

	V_UseBuffer( &background_data );

	if( border_style == 0 )
	{
		src.data = W_CacheLumpName( name, PU_LEVEL );
		src.width = src.height = 64;

		V_InflateAndTransposeBuffer( &src, &inflated, PU_CACHE );

		for ( x=0 ; x<V_VIRTUALWIDTH ; x += 64 )
		{
			width = F_MIN( V_VIRTUALWIDTH - x, 64 );

			for ( y=0 ; y<V_VIRTUALHEIGHT ; y += 64 )
			{
				height = F_MIN( V_VIRTUALHEIGHT - y, 64 );

				V_CopyRect( 0, 0, &inflated, width, height, x, y );
			}
		}
	}
	else
	{
		const char* lookup = ( gamemode == retail || gamemode == commercial ) ? "INTERPIC" :"WIMAP0";
		patch_t* interpic = W_CacheLumpName( DEH_String( lookup ), PU_CACHE );
		V_DrawPatch( 0, 0, interpic );
	}

	if( border_bezel_style == 0 )
	{
		// Draw screen and bezel; this is done to a separate screen buffer.
		patch = W_CacheLumpName(DEH_String("brdr_t"),PU_CACHE);
		for (x=0 ; x < vieww; x+=8)
			V_DrawPatch( viewx + x, viewy - 8, patch);

		patch = W_CacheLumpName(DEH_String("brdr_b"),PU_CACHE);
		for (x=0 ; x < vieww; x+=8)
			V_DrawPatch( viewx + x, viewy + viewh, patch);

		patch = W_CacheLumpName(DEH_String("brdr_l"),PU_CACHE);
		for (y=0 ; y < viewh; y+=8)
			V_DrawPatchClipped( viewx - 8, viewy + y, patch, 0, 0, SHORT(patch->width), SHORT(patch->height));

		patch = W_CacheLumpName(DEH_String("brdr_r"),PU_CACHE);
		for (y=0 ; y < viewh; y+=8)
			V_DrawPatchClipped( viewx + vieww, viewy + y, patch, 0, 0, SHORT(patch->width), SHORT(patch->height));

		// Draw beveled edge. 
		V_DrawPatch( viewx - 8,
					 viewy - 8,
					W_CacheLumpName(DEH_String("brdr_tl"),PU_CACHE));
    
		V_DrawPatch( viewx + vieww,
					 viewy - 8,
					W_CacheLumpName(DEH_String("brdr_tr"),PU_CACHE));
    
		V_DrawPatch( viewx - 8,
					 viewy + viewh,
					W_CacheLumpName(DEH_String("brdr_bl"),PU_CACHE));
    
		V_DrawPatch( viewx + vieww,
					 viewy + viewh,
					W_CacheLumpName(DEH_String("brdr_br"),PU_CACHE));
	}
	else
	{
		// I cannot get this looking right at the moment. Le sigh.
		R_RemapBackBuffer( viewx - 8, viewy - 8, vieww + 16, viewh + 16, colormaps + 2 * 256 );
		R_RemapBackBuffer( viewx - 7, viewy - 7, vieww + 14, viewh + 14, colormaps + 4 * 256 );
		R_RemapBackBuffer( viewx - 6, viewy - 6, vieww + 12, viewh + 12, colormaps + 6 * 256 );
		R_RemapBackBuffer( viewx - 5, viewy - 5, vieww + 10, viewh + 10, colormaps + 7 * 256 );
		R_RemapBackBuffer( viewx - 4, viewy - 4, vieww + 8, viewh + 8, colormaps + 8 * 256 );
		R_RemapBackBuffer( viewx - 3, viewy - 3, vieww + 6, viewh + 6, colormaps + 9 * 256 );
		R_RemapBackBuffer( viewx - 2, viewy - 2, vieww + 4, viewh + 4, colormaps + 10 * 256 );
		R_RemapBackBuffer( viewx - 1, viewy - 1, vieww + 2, viewh + 2, colormaps + 11 * 256 );
	}

    V_RestoreBuffer();
} 
 

//
// Copy a screen buffer.
//
static void R_VideoErase( unsigned ofs, int count ) 
{ 
  // LFB copy.
  // This might not be a good idea if memcpy
  //  is not optiomal, e.g. byte by byte on
  //  a 32bit CPU, as GNU GCC/Linux libc did
  //  at one point.
    if (background_data.data != NULL)
    {
        memcpy(I_VideoBuffer + ofs, background_data.data + ofs, count * sizeof(*I_VideoBuffer));
    }
} 

void R_VideoEraseRegion( int x, int y, int width, int height )
{
	int col = 0;
	int ofs = ( x * SCREENHEIGHT ) + y;

	for( ; col < width; ++col )
	{
		R_VideoErase( ofs, height );
		ofs += SCREENHEIGHT;
	}
}

//
// R_DrawViewBorder
// Draws the border around the view
//  for different size windows?
//
void R_DrawViewBorder (void) 
{ 
    int		top;
    int		side;
    int		ofs;
    int		i; 
 
    if (scaledviewwidth == SCREENWIDTH) 
	return; 
  
    top = (SCREENHEIGHT-SBARHEIGHT-viewheight)/2;
    side = (SCREENWIDTH-scaledviewwidth)/2;

	ofs = 0;
	for( i = 0; i < side; ++i )
	{
		R_VideoErase( ofs, SCREENHEIGHT - SBARHEIGHT );
		ofs += SCREENHEIGHT;
	}

	for( i = side; i < SCREENWIDTH - side; ++i )
	{
		R_VideoErase( ofs, top );
		R_VideoErase( ofs + SCREENHEIGHT - SBARHEIGHT - top, top );
		ofs += SCREENHEIGHT;
	}

	for( i = SCREENWIDTH - side; i < SCREENWIDTH; ++i )
	{
		R_VideoErase( ofs, SCREENHEIGHT - SBARHEIGHT );
		ofs += SCREENHEIGHT;
	}

    V_MarkRect (0,0,SCREENWIDTH, SCREENHEIGHT-SBARHEIGHT); 
} 
 
 

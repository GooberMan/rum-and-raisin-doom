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

// Rum and raisin extensions
// Note that int8x16_t is sensible... But I'd rather undefined platforms
// typedef in to simd_int8x16_t than have random platforms assuming that
// they're correct. Old instructions sets like Altivec won't come back,
// but future compatibility will benefit from this decision.
#if R_SIMD_TYPE( AVX )
	#define COLUMN_AVX 1
	#define COLUMN_NEON 0
	#include <nmmintrin.h>

	typedef __m128i simd_int8x16_t;

	#define _load_int8x16 			_mm_load_si128
	#define _set_int64x2 			_mm_set_epi64x
	#define _set_int8_all 			_mm_set1_epi8
	#define _and_int8x16			_mm_and_si128
	#define _xor_int8x16			_mm_xor_si128
	#define _or_int8x16				_mm_or_si128
	#define _zero_int8x16			_mm_setzero_si128
	#define _store_int8x16			_mm_store_si128
#elif R_SIMD_TYPE( NEON )
	#define COLUMN_AVX 0
	#define COLUMN_NEON 1
	#include <arm_neon.h>

	typedef int8x16_t simd_int8x16_t;

	int8x16_t construct( int64_t a, int64_t b )
	{
		int64x2_t temp = { ( a ), ( b ) };
		return vreinterpretq_s8_s64( temp );
	}

	int8x16_t zero_int8x16()
	{
		int8x16_t temp = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		return temp;
	}

	#define _load_int8x16 			vld1q_s8
	#define _set_int64x2( a, b )	construct( a, b )
	#define _set_int8_all 			vdupq_n_s8
	#define _and_int8x16			vandq_s8
	#define _xor_int8x16			veorq_s8
	#define _or_int8x16				vorrq_s8
	#define _zero_int8x16			zero_int8x16
	#define _store_int8x16			vst1q_s8
#else
	#define COLUMN_AVX 0
	#define COLUMN_NEON 0
#endif

#define F_MIN( x, y ) ( ( y ) ^ ( ( ( x ) ^ ( y ) ) & -( ( x ) < ( y ) ) ) )
#define F_MAX( x, y ) ( ( x ) ^ ( ( ( x ) ^ ( y ) ) & -( ( x ) < ( y ) ) ) )
#define F_BITMASK( numbits ) ( ~( ~0 << numbits ) )
#define F_BITMASK64( numbits ) ( ~( ~0ll << numbits ) )

#if R_DRAWCOLUMN_SIMDOPTIMISED

// Introduce another branch in this function, and I will find you
void R_DrawColumn_OneSample( void )
{
	int32_t		curr;
	size_t		basedest;
	size_t		enddest;
	ptrdiff_t	overlap;
	simd_int8x16_t*	simddest;
	fixed_t		frac;
	fixed_t		fracbase;
	fixed_t		fracstep;

	simd_int8x16_t		prevsample;
	simd_int8x16_t		currsample;
	simd_int8x16_t		writesample;
	simd_int8x16_t		selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	enddest		= basedest + (dc_yh - dc_yl);
	overlap		= basedest & ( sizeof( simd_int8x16_t ) - 1 );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	fracstep	= dc_iscale << 4;
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	fracbase	= frac - dc_iscale * overlap;

	if( overlap != 0 )
	{
		overlap <<= 3;

		// MSVC 32-bit compiles were not generating 64-bit values when using the macros. So manual code here >:-/
		selectmask = _set_int64x2( ~( ~0ll << F_MAX( overlap - 64, 0 ) ), ~( ~0ll << F_MIN( overlap, 64 ) ) );
		//selectmask = _store_int8x16( F_BITMASK64( F_MAX( overlap - 64, 0 ) ), F_BITMASK64( overlap & 63 ) );

		prevsample = _load_int8x16( simddest );
		prevsample = _and_int8x16( selectmask, prevsample );
		selectmask = _xor_int8x16( selectmask, fullmask );
		// TODO: Sample the first pixel, mix in to prevsample, and prime a select mask based on when the next sample occurs...
		currsample = _set_int8_all( dc_source[ ( frac >> FRACBITS ) & 127 ] );
		prevsample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );

		// frac should advance to last output pixel in 16-byte block at this point
		frac		= fracbase + dc_iscale * 15;
		overlap		= ( 15 - ( ( frac & 0xF000 ) >> 12 ) ) << 3;
		selectmask	= _set_int64x2( ~( ~0ll << F_MAX( overlap - 64, 0 ) ), ~( ~0ll << F_MIN( overlap, 64 ) ) );
		prevsample = _and_int8x16( selectmask, prevsample );
		selectmask = _xor_int8x16( selectmask, fullmask );
	}
	else
	{
		selectmask = fullmask;
		prevsample = _zero_int8x16();
	}

	// frac should advance to last output pixel in 16-byte block at this point
	frac		= fracbase + dc_iscale * 15;

	do
	{
		currsample = _set_int8_all( dc_source[ ( frac >> FRACBITS ) & 127 ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );

		frac		+= fracstep;
		overlap		= ( 15 - ( ( frac & 0xF000 ) >> 12 ) ) << 3;
		selectmask	= _set_int64x2( ( ~0ll << F_MAX( overlap - 64, 0 ) ), ( ~0ll << F_MIN( overlap , 64 ) ) );
		prevsample	= _and_int8x16( currsample, _xor_int8x16( selectmask, fullmask ) );

		_store_int8x16( simddest, writesample );
		++simddest;
	} while( simddest <= enddest );
}

#if 0
// This function proves that reads are in fact the bottleneck
void R_DrawColumn_NaiveSIMD( void )
{
	int32_t		curr;
	size_t		basedest;
	size_t		enddest;
	ptrdiff_t	overlap;
	simd_int8x16_t*	simddest;
	fixed_t		frac;
	fixed_t		fracbase;
	fixed_t		fracstep;

	simd_int8x16_t		prevsample;
	simd_int8x16_t		currsample;
	simd_int8x16_t		writesample;
	simd_int8x16_t		selectmask;

	simd_int8x16_t		sample_increment;

	simd_int8x16_t		sample_0_1_2_3;
	simd_int8x16_t		sample_4_5_6_7;
	simd_int8x16_t		sample_8_9_10_11;
	simd_int8x16_t		sample_12_13_14_15;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	enddest		= basedest + (dc_yh - dc_yl);
	overlap		= basedest & ( sizeof( simd_int8x16_t ) - 1 );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	fracstep	= dc_iscale << 4;
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	fracbase	= frac - dc_iscale * overlap;

	overlap <<= 3;

	// MSVC 32-bit compiles were not generating 64-bit values when using the macros. So manual code here >:-/
	selectmask = _store_int8x16( ~( ~0ll << F_MAX( overlap - 64, 0 ) ), ~( ~0ll << ( overlap & 63 ) ) );
	//selectmask = _store_int8x16( F_BITMASK64( F_MAX( overlap - 64, 0 ) ), F_BITMASK64( overlap & 63 ) );

	sample_increment = _mm_set1_epi32( dc_iscale * 4 );
	sample_0_1_2_3 = _mm_set_epi32( fracbase, fracbase + dc_iscale, fracbase + dc_iscale * 2, fracbase + dc_iscale * 3 );
	sample_4_5_6_7 = _mm_add_epi32( sample_0_1_2_3, sample_increment );
	sample_8_9_10_11 = _mm_add_epi32( sample_4_5_6_7, sample_increment );
	sample_12_13_14_15 = _mm_add_epi32( sample_8_9_10_11, sample_increment );
	sample_increment = _mm_set1_epi32( fracstep );

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

	do
	{
		currsample = _mm_set_epi8( dc_source[ sample_12_13_14_15.m128i_i8[ 2 ] ]
								,  dc_source[ sample_12_13_14_15.m128i_i8[ 6 ] ]
								,  dc_source[ sample_12_13_14_15.m128i_i8[ 10 ] ]
								,  dc_source[ sample_12_13_14_15.m128i_i8[ 14 ] ]
								,  dc_source[ sample_8_9_10_11.m128i_i8[ 2 ] ]
								,  dc_source[ sample_8_9_10_11.m128i_i8[ 6 ] ]
								,  dc_source[ sample_8_9_10_11.m128i_i8[ 10 ] ]
								,  dc_source[ sample_8_9_10_11.m128i_i8[ 14 ] ]
								,  dc_source[ sample_4_5_6_7.m128i_i8[ 2 ] ]
								,  dc_source[ sample_4_5_6_7.m128i_i8[ 6 ] ]
								,  dc_source[ sample_4_5_6_7.m128i_i8[ 10 ] ]
								,  dc_source[ sample_4_5_6_7.m128i_i8[ 14 ] ]
								,  dc_source[ sample_0_1_2_3.m128i_i8[ 2 ] ]
								,  dc_source[ sample_0_1_2_3.m128i_i8[ 6 ] ]
								,  dc_source[ sample_0_1_2_3.m128i_i8[ 10 ] ]
								,  dc_source[ sample_0_1_2_3.m128i_i8[ 14 ] ]
		);

		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _and_int8x16( currsample, _xor_int8x16( selectmask, fullmask ) );
		selectmask = fullmask;

		sample_0_1_2_3 = _mm_add_epi32( sample_0_1_2_3, sample_increment );
		sample_4_5_6_7 = _mm_add_epi32( sample_4_5_6_7, sample_increment );
		sample_8_9_10_11 = _mm_add_epi32( sample_8_9_10_11, sample_increment );
		sample_12_13_14_15 = _mm_add_epi32( sample_12_13_14_15, sample_increment );

		_store_int8x16( simddest, writesample );

		++simddest;
	} while( simddest < enddest );
}
#endif // 0

void R_DrawColumn_TwoSamples( void )
{
	int32_t curr;
	size_t	basedest;
	size_t	overlap;
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale << 2; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 5;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep * 6;

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep * 5;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale << 2; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

	do
	{
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;
		sample = dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ];
		frac += fracstep;

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep * 3;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep * 3;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep * 2;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale << 1; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep * 2;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep;
		_store_int8x16( simddest, writesample );
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
	simd_int8x16_t* simddest;
	fixed_t frac;
	fixed_t fracstep;
	byte sample;

	simd_int8x16_t prevsample;
	simd_int8x16_t currsample;
	simd_int8x16_t writesample;
	simd_int8x16_t selectmask;

	const simd_int8x16_t fullmask = _set_int8_all( 0xFF );

	basedest	= xlookup[dc_x] + rowofs[dc_yl];
	overlap		= basedest % sizeof( simd_int8x16_t );

	// HACK FOR NOW
	simddest	= basedest - overlap;
	curr		= dc_yl - overlap;

	overlap <<= 3;

	selectmask = _set_int64x2( F_BITMASK64( overlap - 64 ), F_BITMASK64( overlap & 63ull ) );

	fracstep	= dc_iscale; 
	frac		= dc_texturemid + ( dc_yl - centery ) * dc_iscale;

	prevsample = _load_int8x16( simddest );
	prevsample = _and_int8x16( selectmask, prevsample );
	selectmask = _xor_int8x16( selectmask, fullmask );

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

		currsample = _set_int8_all( dc_colormap[ dc_source[ ( frac >> FRACBITS ) & 127 ] ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );
		prevsample = _zero_int8x16();
		selectmask = fullmask;

		frac += fracstep;
		_store_int8x16( simddest, writesample );
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
 
 

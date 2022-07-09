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


extern "C"
{
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
#include "i_thread.h"

// State.
#include "doomstat.h"
#include "m_misc.h"
#include "m_random.h"

extern int32_t border_style;
extern int32_t border_bezel_style;

extern vbuffer_t* dest_buffer;

// ?
// This does precisely nothing but hard limit what you can do with a given executable
#define MAXWIDTH			( MAXSCREENWIDTH + ( MAXSCREENWIDTH >> 1) )
#define MAXHEIGHT			( MAXSCREENHEIGHT + ( MAXSCREENHEIGHT >> 1) )

size_t			xlookup[MAXWIDTH];
size_t			rowofs[MAXHEIGHT]; 

}

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


int32_t			viewwidth;
int32_t			scaledviewwidth;
int32_t			viewheight;
int32_t			viewwindowx;
int32_t			viewwindowy; 

// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//
byte			translations[3][256];	
 
// Backing buffer containing the bezel drawn around the screen and 
// surrounding background.

static vbuffer_t background_data;

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
	typedef __m128i simd_int64x2_t;

	#define _load_int8x16 			_mm_load_si128
	#define _set_int64x2 			_mm_set_epi64x
	#define _set1_int8x16 			_mm_set1_epi8
	#define _and_int8x16			_mm_and_si128
	#define _xor_int8x16			_mm_xor_si128
	#define _or_int8x16				_mm_or_si128
	#define _zero_int8x16			_mm_setzero_si128
	#define _store_int8x16			_mm_store_si128

	#define _set_int8x16( a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p )		_mm_set_epi8( p, o, n, m, l, k, j, i, h, g, f, e, d, c, b, a )
	#define literal_int8x16( a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p )	{ a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p }
	#define literal1_int8x16( a )												{ a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a }
#elif R_SIMD_TYPE( NEON )
	#define COLUMN_AVX 0
	#define COLUMN_NEON 1
	#include <arm_neon.h>

	typedef int8x16_t simd_int8x16_t;
	typedef int32x4_t simd_int32x4_t;

	inline int8x16_t construct( int64_t a, int64_t b )
	{
		int64x2_t temp = { ( a ), ( b ) };
		return vreinterpretq_s8_s64( temp );
	}

	const int8x16_t zero_int8x16_v = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	#define _load_int8x16 			vld1q_s8
	#define _set_int64x2( a, b )	construct( a, b )
	#define _set1_int8x16 			vdupq_n_s8
	#define _and_int8x16			vandq_s8
	#define _xor_int8x16			veorq_s8
	#define _or_int8x16				vorrq_s8
	#define _zero_int8x16()			zero_int8x16_v
	#define _store_int8x16			vst1q_s8

	#define _set_int8x16( a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p )		{ a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p }
	#define literal_int8x16( a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p )	{ a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p }
	#define literal1_int8x16( a )												{ a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a }
#else
	#define COLUMN_AVX 0
	#define COLUMN_NEON 0
#endif


#if R_DRAWCOLUMN_SIMDOPTIMISED

constexpr simd_int8x16_t MergeLookup[ 32 ] =
{
	literal_int8x16( -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
	literal_int8x16(  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ),
};

// Introduce another branch in this function, and I will find you
void R_DrawColumn_OneSample( colcontext_t* context )
{
	size_t				basedest;
	size_t				enddest;
	ptrdiff_t			overlap;
	simd_int8x16_t*		simddest;
	rend_fixed_t		frac;
	rend_fixed_t		fracbase;
	rend_fixed_t		fracstep;

	simd_int8x16_t		prevsample;
	simd_int8x16_t		currsample;
	simd_int8x16_t		writesample;
	simd_int8x16_t		selectmask;

	constexpr simd_int8x16_t fullmask = literal1_int8x16( -1 );

	basedest	= (size_t)context->output.data + xlookup[context->x] + rowofs[context->yl];
	enddest		= basedest + ( context->yh - context->yl);
	overlap		= basedest & ( sizeof( simd_int8x16_t ) - 1 );

	// HACK FOR NOW
	simddest	= ( simd_int8x16_t* )( basedest - overlap );

	fracstep	= context->iscale << 4;
	frac		= context->texturemid + ( context->yl - centery ) * context->iscale;

	fracbase	= frac - context->iscale * overlap;

	if( overlap != 0 )
	{
		overlap <<= 3;
		selectmask = _set_int64x2( ~( ~0ll << M_MAX( overlap - 64, 0 ) ), ~( ~0ll << M_MIN( overlap, 64 ) ) );

		// Sample the backbuffer so we can blend...
		prevsample = _load_int8x16( simddest );
		prevsample = _and_int8x16( selectmask, prevsample );
		selectmask = _xor_int8x16( selectmask, fullmask );

		// ...with the first valid output texel...
		currsample = _set1_int8x16( context->source[ ( frac >> RENDFRACBITS ) & 127 ] );
		prevsample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );

		// ...then advance to where the algorithm expects to be and continue with the rest
		frac		= fracbase + context->iscale * 15;
		selectmask	= MergeLookup[ ( frac & 0x1F0000 ) >> 12 ];
	}
	else
	{
		selectmask	= fullmask;
		prevsample	= _zero_int8x16();
		frac		= fracbase + context->iscale * 15;
	}

	do
	{
		currsample = _set1_int8x16( context->source[ ( frac >> RENDFRACBITS ) & 127 ] );
		writesample = _or_int8x16( prevsample, _and_int8x16( currsample, selectmask ) );

		frac		+= fracstep;

		selectmask	= MergeLookup[ ( frac & 0x1F0000 ) >> 12 ];
		_store_int8x16( simddest, writesample );
		++simddest;

		prevsample	= _and_int8x16( currsample, _xor_int8x16( selectmask, fullmask ) );

	} while( (size_t)simddest <= enddest );
}

#endif // R_DRAWCOLUMN_SIMDOPTIMISED

//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
// 

namespace DrawColumn
{
	struct Null
	{
		// Draws exactly the column you give it. No extra colormapping
		static INLINE void Draw( colcontext_t* context )
		{
		}

		// Will take the source pixels and remap them with the context's colormap
		static INLINE void ColormapDraw( colcontext_t* context )
		{
		}
	};

	struct Bytewise
	{
		struct SamplerOriginal
		{
			// The 127 here makes it wrap, keeping max height to 128
			// Need to make that a bit nicer somehow
			struct Direct
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t frac, int32_t textureheight = 0 )
				{
					return context->source[ (frac >> RENDFRACBITS ) & 127 ];
				}
			};

			struct PaletteSwap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t frac, int32_t textureheight = 0 )
				{
					return context->translation[ Direct::Sample( context, frac ) ];
				}
			};

			struct Colormap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t frac, int32_t textureheight = 0 )
				{
					return context->colormap[ Direct::Sample( context, frac ) ];
				}
			};

			struct ColormapPaletteSwap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t frac, int32_t textureheight = 0 )
				{
					return context->colormap[ PaletteSwap::Sample( context, frac ) ];
				}
			};
		};

		struct SamplerLimitRemoving
		{
			// The 127 here makes it wrap, keeping max height to 128
			// Need to make that a bit nicer somehow
			struct Direct
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t frac, int32_t textureheight = 0 )
				{
					return context->source[ (frac >> RENDFRACBITS ) % textureheight ];
				}
			};

			struct PaletteSwap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t frac, int32_t textureheight = 0 )
				{
					return context->translation[ Direct::Sample( context, frac, textureheight ) ];
				}
			};

			struct Colormap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t frac, int32_t textureheight = 0 )
				{
					return context->colormap[ Direct::Sample( context, frac, textureheight ) ];
				}
			};

			struct ColormapPaletteSwap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t frac, int32_t textureheight = 0 )
				{
					return context->colormap[ PaletteSwap::Sample( context, frac, textureheight ) ];
				}
			};
		};

		template< typename Sampler >
		static INLINE void DrawWith( colcontext_t* context )
		{
			int			count		= context->yh - context->yl;

			// Framebuffer destination address.
			// Use ylookup LUT to avoid multiply with ScreenWidth.
			// Use columnofs LUT for subwindows? 
			pixel_t*	dest		= context->output.data + xlookup[ context->x ] + rowofs[ context->yl ];

			// Determine scaling, which is the only mapping to be done.
			rend_fixed_t		fracstep = context->iscale;
			rend_fixed_t		frac = context->texturemid +  ( context->yl - centery ) * fracstep;

			// Inner loop that does the actual texture mapping,
			//  e.g. a DDA-lile scaling.
			// This is as fast as it gets.
			do 
			{
				*dest = Sampler::Sample( context, frac, context->sourceheight );
		
				dest += 1; 
				frac += fracstep;
	
			} while (count--); 
		}

		static INLINE void Draw( colcontext_t* context )									{ DrawWith< SamplerOriginal::Direct >( context ); }
		static INLINE void PaletteSwapDraw( colcontext_t* context )							{ DrawWith< SamplerOriginal::PaletteSwap >( context ); }
		static INLINE void ColormapDraw( colcontext_t* context )							{ DrawWith< SamplerOriginal::Colormap >( context ); }
		static INLINE void ColormapPaletteSwapDraw( colcontext_t* context )					{ DrawWith< SamplerOriginal::ColormapPaletteSwap >( context ); }

		static INLINE void LimitRemovingDraw( colcontext_t* context )						{ DrawWith< SamplerLimitRemoving::Direct >( context ); }
		static INLINE void LimitRemovingPaletteSwapDraw( colcontext_t* context )			{ DrawWith< SamplerLimitRemoving::PaletteSwap >( context ); }
		static INLINE void LimitRemovingColormapDraw( colcontext_t* context )				{ DrawWith< SamplerLimitRemoving::Colormap >( context ); }
		static INLINE void LimitRemovingColormapPaletteSwapDraw( colcontext_t* context )	{ DrawWith< SamplerLimitRemoving::ColormapPaletteSwap >( context ); }
	};

#if R_DRAWCOLUMN_SIMDOPTIMISED
	struct SIMD
	{
		static INLINE void Draw( colcontext_t* context )
		{
		}
	};
#else // !R_DRAWCOLUMN_SIMDOPTIMISED
	typedef Bytewise SIMD;
#endif // R_DRAWCOLUMN_SIMDOPTIMISED
}

void R_DrawColumn( colcontext_t* context ) 
{ 
	DrawColumn::Bytewise::Draw( context );
} 

void R_DrawColumn_Untranslated( colcontext_t* context )
{
	DrawColumn::Bytewise::ColormapDraw( context );
}

void R_LimitRemovingDrawColumn( colcontext_t* context ) 
{ 
	DrawColumn::Bytewise::LimitRemovingDraw( context );
} 

void R_LimitRemovingDrawColumn_Untranslated( colcontext_t* context )
{
	DrawColumn::Bytewise::LimitRemovingColormapDraw( context );
}


void R_DrawColumnLow ( colcontext_t* context ) 
{ 
    int					count; 
    pixel_t*			dest;
    pixel_t*			dest2;
    rend_fixed_t		frac;
    rend_fixed_t		fracstep;
	byte				sample;

	count = context->yh - context->yl;

    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows? 
    dest = context->output.data + xlookup[ context->x * 2 ] + rowofs[context->yl];  
    dest2 = context->output.data + xlookup[ context->x * 2 + 1 ] + rowofs[context->yl];  

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = context->iscale; 
    frac = context->texturemid + (context->yl-centery)*fracstep; 

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do 
    {
		// Re-map color indices from wall texture column
		//  using a lighting/special effects LUT.
		sample = context->colormap[ context->source[ RendFixedToInt( frac ) & 127 ] ];
		*dest++ = sample; 
		*dest2++ = sample; 
		frac += fracstep;
	
    } while (count--); 
}



//
// Spectre/Invisibility.
//
// Hoo boy did I get this wrong originally. Vanilla offset by SCREENWIDTH, ie one column.
// Thus the correct value for vanilla compatibility is 1, not render_height.
#define FUZZOFF	( 1 )
#define FUZZPRIME ( 29 )

int32_t fuzzoffset[] =
{
	FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF 
};
#define FUZZTABLE ( sizeof( fuzzoffset ) / sizeof( *fuzzoffset ) )

// 1,000 fuzz offsets. Overkill. And loving it.
static const int32_t adjustedfuzzoffset[] =
{
	FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
	-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,
	FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
	-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
};

#define ADJUSTEDFUZZTABLE ( sizeof( adjustedfuzzoffset ) / sizeof( *adjustedfuzzoffset ) )

THREADLOCAL int32_t	fuzzpos = 0;
THREADLOCAL int32_t	cachedfuzzpos = 0;

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
	cachedfuzzpos = fuzzpos;
}

void R_DrawFuzzColumn ( colcontext_t* context ) 
{ 
    int					count; 
    pixel_t*			dest;
	pixel_t*			start;
    rend_fixed_t		frac;
	rend_fixed_t		lastfrac;
    rend_fixed_t		fracstep;
	int32_t				fuzzposbase;
	int32_t				fuzzposadd;

	fuzzposbase = fuzzpos;
	fuzzposadd = fuzzposbase;

	count = context->yh - context->yl; 

	start = dest = context->output.data + xlookup[context->x] + rowofs[context->yl];

    // Looks familiar.
    fracstep = context->iscale; 
    frac = context->texturemid + (context->yl-centery)*fracstep; 

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do 
    {
		// Lookup framebuffer, and retrieve
		//  a pixel that is either one column
		//  left or right of the current one.
		// Add index from colormap to index.
		*dest = colormaps[ 6*256 + dest[ fuzzoffset[ (uint32_t)fuzzposadd % FUZZTABLE ] ] ]; 

		++dest;

		lastfrac = frac;
		frac += fracstep; 

		++fuzzposadd;
	
	} while (count--);

	// Doing the fuzzprime add basically eliminates banding
	// But you kinda want it anyway to be able to see anything at high res...
	fuzzpos += ( fuzzposadd - fuzzposbase /*+ ( M_Random() % FUZZPRIME )*/ );
} 

void R_DrawAdjustedFuzzColumn( colcontext_t* context )
{
    int					count; 
    pixel_t*			dest;
    rend_fixed_t		frac;
    rend_fixed_t		fracstep;
	int32_t				fuzzposadd;
	int32_t				fuzzpossample;

	pixel_t*			src;
	pixel_t				srcbuffer[ MAXSCREENHEIGHT ];
	pixel_t*			basecolorpmap = colormaps + 6*256;
	pixel_t*			darkercolormap = colormaps + 12*256;
	pixel_t*			samplecolormap;

	rend_fixed_t		originalcolfrac = RendFixedDiv( IntToRendFixed( 200 ), IntToRendFixed( viewheight ) );
	rend_fixed_t		oldfrac = 0;
	rend_fixed_t		newfrac = 0;

	fuzzpos			= fuzzposadd = cachedfuzzpos;
	fuzzpossample	= adjustedfuzzoffset[ fuzzposadd % ADJUSTEDFUZZTABLE ];
	samplecolormap	= fuzzpossample < 0 ? darkercolormap : basecolorpmap;

	count = context->yh - context->yl; 

	dest = context->output.data + xlookup[context->x] + rowofs[context->yl];

	// count + 3 is correct. Since a 0 count will always sample one pixel. So make sure we copy one on each side of the sample.
	memcpy( srcbuffer, dest - 1, sizeof( pixel_t ) * ( count + 3 ) );
	src = srcbuffer + 1;

    // Looks familiar.
    fracstep = context->iscale; 
    frac = context->texturemid + (context->yl-centery)*fracstep; 

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do 
    {
		*dest = samplecolormap[ src[ fuzzpossample ] ]; 

		++dest;
		++src;

		frac += fracstep;

		oldfrac = newfrac;
		newfrac += originalcolfrac;

		if( RendFixedToInt( newfrac ) > RendFixedToInt( oldfrac ) )
		{
			// Clamp table lookup index.
			++fuzzposadd;
			fuzzpossample = adjustedfuzzoffset[ fuzzposadd % ADJUSTEDFUZZTABLE ];
			samplecolormap = fuzzpossample < 0 ? darkercolormap : basecolorpmap;

		//	src = srcbuffer + 1 + (dest - start) + fuzzoffset[ fuzzposadd % FUZZTABLE ] * adjustedscale;
		//	if( src > srcbuffer + 1 + (dest - start) - adjustedscale )
		//	{
		//		src = srcbuffer + 1 + (dest - start) - adjustedscale;
		//	}
		}
	} while (count--);

	fuzzpos = fuzzposadd + ( M_Random() % FUZZPRIME );
}

void R_DrawHeatwaveFuzzColumn( colcontext_t* context )
{
	R_DrawAdjustedFuzzColumn( context );
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
byte*	translationtables;

void R_DrawTranslatedColumn ( colcontext_t* context ) 
{ 
   DrawColumn::Bytewise::ColormapPaletteSwapDraw( context );
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
	
	translationtables = (byte*)Z_Malloc( 256*3, PU_STATIC, 0 );

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
void R_DrawSpan( spancontext_t* context ) 
{ 
	uint32_t	positionx;
	uint32_t	positiony;
	uint32_t	stepx;
	uint32_t	stepy;
	pixel_t		*dest;
	uint32_t	count;
	uint32_t	spot;

	positionx = context->xfrac;
	positiony = context->yfrac;
	stepx = context->xstep;
	stepy = context->ystep;

	dest = context->output.data + xlookup[context->x1] + rowofs[context->y];
	count = context->x2 - context->x1;

	do
	{
		// Avoids the 16 bit calculations in the original function
		// Multiplies y by 64 and leaves x as is, for direct flat texel lookup
		spot = ( (positiony & 0x3F0000 ) >> 10)
				| ( (positionx & 0x3F0000 ) >> 16);

		// Lookup pixel from flat texture tile,
		*dest = context->source[spot];
		dest += render_height;

		positionx += stepx;
		positiony += stepy;

	} while (count--);
}


//
// Again..
//
void R_DrawSpanLow ( spancontext_t* context )
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
	|| ds_x2>=render_width
	|| (unsigned)ds_y>render_height)
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
    int32_t	i; 

    // Handle resize,
    //  e.g. smaller view windows
    //  with border and/or status bar.
    viewwindowx = (render_width-width) >> 1; 
    // Samw with base row offset.
    if (width == render_width) 
		viewwindowy = 0;
    else 
		viewwindowy = (render_height-SBARHEIGHT-height) >> 1;

    // Preclaculate all column offsets.
    for (i=0 ; i<width ; i++) 
		xlookup[i] = (i+viewwindowx)*render_height; 

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
	int32_t numcols = FixedToInt( FixedMul( IntToFixed( virtualwidth ), V_WIDTHMULTIPLIER ) );
	int32_t numrows;

	byte* outputbase = dest_buffer->data	+ FixedToInt( FixedMul( IntToFixed( virtualx ), V_WIDTHMULTIPLIER ) ) * dest_buffer->pitch
											+ FixedToInt( FixedMul( IntToFixed( virtualy ), V_HEIGHTMULTIPLIER ) );
	byte* output;

	for( ; numcols > 0; --numcols )
	{
		output = outputbase;
		numrows = FixedToInt( FixedMul( IntToFixed( virtualheight ), V_HEIGHTMULTIPLIER ) );

		for( ; numrows > 0; --numrows )
		{
			*output = colormap[ *output ];
			++output;
		}
		outputbase += dest_buffer->pitch;
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

	int32_t viewx = FixedToInt( FixedDiv( IntToFixed( viewwindowx ),		V_WIDTHMULTIPLIER ) );
	int32_t viewy = FixedToInt( FixedDiv( IntToFixed( viewwindowy ),		V_HEIGHTMULTIPLIER ) );
	int32_t vieww = FixedToInt( FixedDiv( IntToFixed( scaledviewwidth ),	V_WIDTHMULTIPLIER ) );
	int32_t viewh = FixedToInt( FixedDiv( IntToFixed( viewheight ),			V_HEIGHTMULTIPLIER ) );

    int		x;
    int		y; 
    patch_t*	patch;

    // DOOM border patch.
    const char *name1 = DEH_String("FLOOR7_2");

    // DOOM II border patch.
    const char *name2 = DEH_String("GRNROCK");

    const char *name;

    // If we are running full screen, there is no need to do any of this,
    // and the background buffer can be freed if it was previously in use.

	if( scaledviewwidth == render_width
		|| background_data.width != render_width
		|| background_data.height != render_height )
	{
		if (background_data.data != NULL
			&& (render_width > background_data.width || render_height > background_data.height ) )
		{
			Z_Free( background_data.data );
			memset( &background_data, 0, sizeof( background_data ) );
		}

		if( scaledviewwidth == render_width )
		{
			return;
		}
	}

    // Allocate the background buffer if necessary
	
	if (background_data.data == NULL)
	{
		background_data.data = (pixel_t*)Z_Malloc( render_width * render_height * sizeof(*background_data.data), PU_STATIC, NULL );
		background_data.width = render_width;
		background_data.height = render_height;
		background_data.pitch = render_height;
		background_data.pixel_size_bytes = 1;
		background_data.magic_value = vbuffer_magic;
    }

	memset( background_data.data, 0, render_width * render_height * sizeof(*background_data.data) );

    if (gamemode == commercial)
	{
		name = name2;
	}
    else
	{
		name = name1;
	}

	V_UseBuffer( &background_data );

	switch( border_style )
	{
	case Border_Interpic:
		{
			const char* lookup = ( gamemode == retail || gamemode == commercial ) ? "INTERPIC" :"WIMAP0";
			patch_t* interpic = (patch_t*)W_CacheLumpName( DEH_String( lookup ), PU_LEVEL );
			V_DrawPatch( 0, 0, interpic );
		}
		break;

	case Border_Original:
	default:
		src.data = (pixel_t*)W_CacheLumpName( name, PU_LEVEL );
		src.width = src.height = src.pitch = 64;
		src.pixel_size_bytes = 1;
		src.magic_value = vbuffer_magic;

		V_TransposeBuffer( &src, &inflated, PU_CACHE );

		V_TileBuffer( &inflated, 0, 0, V_VIRTUALWIDTH, V_VIRTUALHEIGHT - ST_HEIGHT );
		//V_FillBorder( &inflated, 0, V_VIRTUALHEIGHT - ST_HEIGHT );

		break;
	}

	switch( border_bezel_style )
	{
	case Bezel_Dithered:
		// I cannot get this looking right at the moment. Le sigh.
		R_RemapBackBuffer( viewx - 8, viewy - 8, vieww + 16, viewh + 16, colormaps + 2 * 256 );
		R_RemapBackBuffer( viewx - 7, viewy - 7, vieww + 14, viewh + 14, colormaps + 4 * 256 );
		R_RemapBackBuffer( viewx - 6, viewy - 6, vieww + 12, viewh + 12, colormaps + 6 * 256 );
		R_RemapBackBuffer( viewx - 5, viewy - 5, vieww + 10, viewh + 10, colormaps + 7 * 256 );
		R_RemapBackBuffer( viewx - 4, viewy - 4, vieww + 8, viewh + 8, colormaps + 8 * 256 );
		R_RemapBackBuffer( viewx - 3, viewy - 3, vieww + 6, viewh + 6, colormaps + 9 * 256 );
		R_RemapBackBuffer( viewx - 2, viewy - 2, vieww + 4, viewh + 4, colormaps + 10 * 256 );
		R_RemapBackBuffer( viewx - 1, viewy - 1, vieww + 2, viewh + 2, colormaps + 11 * 256 );
		break;

	case Bezel_Original:
	default:
		// Draw screen and bezel; this is done to a separate screen buffer.
		patch = (patch_t*)W_CacheLumpName(DEH_String("brdr_t"),PU_CACHE);
		for (x=0 ; x < vieww; x+=8)
			V_DrawPatch( viewx + x, viewy - 8, patch);

		patch = (patch_t*)W_CacheLumpName(DEH_String("brdr_b"),PU_CACHE);
		for (x=0 ; x < vieww; x+=8)
			V_DrawPatch( viewx + x, viewy + viewh, patch);

		patch = (patch_t*)W_CacheLumpName(DEH_String("brdr_l"),PU_CACHE);
		for (y=0 ; y < viewh; y+=8)
			V_DrawPatchClipped( viewx - 8, viewy + y, patch, 0, 0, SHORT(patch->width), SHORT(patch->height));

		patch = (patch_t*)W_CacheLumpName(DEH_String("brdr_r"),PU_CACHE);
		for (y=0 ; y < viewh; y+=8)
			V_DrawPatchClipped( viewx + vieww, viewy + y, patch, 0, 0, SHORT(patch->width), SHORT(patch->height));

		// Draw beveled edge. 
		V_DrawPatch( viewx - 8,
					 viewy - 8,
					(patch_t*)W_CacheLumpName(DEH_String("brdr_tl"),PU_CACHE));
    
		V_DrawPatch( viewx + vieww,
					 viewy - 8,
					(patch_t*)W_CacheLumpName(DEH_String("brdr_tr"),PU_CACHE));
    
		V_DrawPatch( viewx - 8,
					 viewy + viewh,
					(patch_t*)W_CacheLumpName(DEH_String("brdr_bl"),PU_CACHE));
    
		V_DrawPatch( viewx + vieww,
					 viewy + viewh,
					(patch_t*)W_CacheLumpName(DEH_String("brdr_br"),PU_CACHE));
		break;

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
	int ofs = ( x * render_height ) + y;

	for( ; col < width; ++col )
	{
		R_VideoErase( ofs, height );
		ofs += render_height;
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
 
    if (scaledviewwidth == render_width) 
	return; 
  
    top = (render_height-SBARHEIGHT-viewheight)/2;
    side = (render_width-scaledviewwidth)/2;

	ofs = 0;
	for( i = 0; i < side; ++i )
	{
		R_VideoErase( ofs, render_height - SBARHEIGHT );
		ofs += render_height;
	}

	for( i = side; i < render_width - side; ++i )
	{
		R_VideoErase( ofs, top );
		R_VideoErase( ofs + render_height - SBARHEIGHT - top, top );
		ofs += render_height;
	}

	for( i = render_width - side; i < render_width; ++i )
	{
		R_VideoErase( ofs, render_height - SBARHEIGHT );
		ofs += render_height;
	}

    V_MarkRect (0,0,render_width, render_height-SBARHEIGHT); 
} 
 
 

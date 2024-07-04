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

#include "p_local.h"
#include "r_local.h"

#include "deh_main.h"

#include "i_system.h"
#include "i_thread.h"

#include "m_misc.h"

#include "r_state.h"

#include "v_video.h"

#include "w_wad.h"

#include "z_zone.h"

extern "C"
{

// Needs access to LFB (guess what).
#include "st_stuff.h"
#include "i_swap.h"

// State.
#include "doomstat.h"
#include "m_random.h"

extern int32_t border_style;
extern int32_t border_bezel_style;

extern vbuffer_t* dest_buffer;

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//  and we need only the base address,
//  and the total size == width*height*depth/8.,
//


// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//
byte			translations[3][256];	
 
// Backing buffer containing the bezel drawn around the screen and 
// surrounding background.

}

#include "m_profile.h"

// status bar height at bottom of screen
#define SBARHEIGHT( drs )				( ( (int64_t)( ST_HEIGHT << FRACBITS ) * (int64_t)V_HEIGHTMULTIPLIERVAL( drs->frame_height ) >> FRACBITS ) >> FRACBITS )


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

	basedest	= (size_t)context->output.data + context->x * context->output.pitch + context->yl;
	enddest		= basedest + ( context->yh - context->yl);
	overlap		= basedest & ( sizeof( simd_int8x16_t ) - 1 );

	// HACK FOR NOW
	simddest	= ( simd_int8x16_t* )( basedest - overlap );

	fracstep	= context->iscale << 4;
	frac		= context->texturemid + ( context->yl - drs_current->centery ) * context->iscale;

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

template< typename _ret, typename... _args >
constexpr size_t arg_count( _ret( * )( _args... ) )
{
	return sizeof...( _args );
};

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

	struct ViewportLookup
	{
		static INLINE size_t XOffset( colcontext_t*& context )
		{
			return context->x * context->output.pitch;
		}

		static INLINE size_t YOffset( colcontext_t*& context )
		{
			return context->yl;
		}

		static INLINE size_t StartPos( colcontext_t*& context )
		{
			return context->texturemid + ( context->yl - drs_current->centery ) * context->iscale;
		}
	};

	struct ViewportSpriteLookup
	{
		static INLINE size_t XOffset( colcontext_t*& context )
		{
			return context->x * context->output.pitch;
		}

		static INLINE size_t YOffset( colcontext_t*& context )
		{
			return context->yl;
		}

		static INLINE size_t StartPos( colcontext_t*& context )
		{
			return M_MAX( 0, context->texturemid + ( context->yl - drs_current->centery ) * context->iscale );
		}
	};

	struct BackbufferLookup
	{
		static INLINE size_t XOffset( colcontext_t*& context )
		{
			return context->x * context->output.pitch;
		}

		static INLINE size_t YOffset( colcontext_t*& context )
		{
			return context->yl;
		}

		static INLINE size_t StartPos( colcontext_t*& context )
		{
			return 0;
		}
	};

	struct Bytewise
	{
		template< size_t _texturewidth >
		requires( IsPowerOf2( _texturewidth ) )
		struct SamplerOriginal
		{
			struct Direct
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t& frac )
				{
					constexpr size_t Mask = _texturewidth - 1;
					return context->source[ (frac >> RENDFRACBITS ) & Mask ];
				}
			};

			struct PaletteSwap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t& frac )
				{
					return context->translation[ Direct::Sample( context, frac ) ];
				}
			};

			struct Colormap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t& frac )
				{
					return context->colormap[ Direct::Sample( context, frac ) ];
				}
			};

			struct ColormapPaletteSwap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t& frac )
				{
					return context->colormap[ PaletteSwap::Sample( context, frac ) ];
				}
			};
		};

		struct SamplerLimitRemoving
		{
			struct Direct
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t& frac, const rend_fixed_t& textureheight )
				{
					rend_fixed_t nextfrac = frac - textureheight;
					frac = ( frac >= textureheight ) ? nextfrac : frac;
					return context->source[ frac >> RENDFRACBITS ];
				}
			};

			struct PaletteSwap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t& frac, const rend_fixed_t& textureheight )
				{
					return context->translation[ Direct::Sample( context, frac, textureheight ) ];
				}
			};

			struct Colormap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t& frac, const rend_fixed_t& textureheight )
				{
					return context->colormap[ Direct::Sample( context, frac, textureheight ) ];
				}
			};

			struct ColormapPaletteSwap
			{
				static INLINE pixel_t Sample( colcontext_t* context, rend_fixed_t& frac, const rend_fixed_t& textureheight )
				{
					return context->colormap[ PaletteSwap::Sample( context, frac, textureheight ) ];
				}
			};
		};

		struct WriterDirect
		{
			static INLINE void Write( colcontext_t* context, pixel_t*& dest, pixel_t sample )
			{
				*dest++ = sample;
			}
		};

		struct WriterTransparent
		{
			static INLINE void Write( colcontext_t* context, pixel_t*& dest, pixel_t sample )
			{
				*dest++ = context->transparency[ (int32_t)sample + ((int32_t)*dest << 8 ) ];
			}
		};

		template< typename Sampler, typename Lookup, typename Writer >
		static INLINE void DrawWith( colcontext_t* context )
		{
			constexpr bool LimitRemoving = arg_count( &Sampler::Sample ) > 2;

			int32_t			count		= context->yh - context->yl;

			// Framebuffer destination address.
			// Use ylookup LUT to avoid multiply with ScreenWidth.
			// Use columnofs LUT for subwindows? 
			pixel_t*		dest		= context->output.data + Lookup::XOffset( context ) + Lookup::YOffset( context );

			// Determine scaling, which is the only mapping to be done.
			rend_fixed_t&	fracstep	= context->iscale;
			rend_fixed_t	frac		= Lookup::StartPos( context );
			if constexpr( LimitRemoving )
			{
				// We're not doing a mask operation, so we need to bring the frac value to within 0 -> sourceheight
				// range. We should also realisitcally be able to do this once elsewhere instead of all the time,
				// the frac calculation is only important when clipping to screenspace. A better way of calculating
				// it should be possible.
				frac %= context->sourceheight;
				rend_fixed_t nextfrac = frac + context->sourceheight;
				frac = ( frac < 0 ) ? nextfrac : frac;
			}

			// Inner loop that does the actual texture mapping,
			//  e.g. a DDA-lile scaling.
			// This is as fast as it gets.
			do 
			{
				if constexpr( !LimitRemoving )
				{
					Writer::Write( context, dest, Sampler::Sample( context, frac ) );
				}
				else
				{
					Writer::Write( context, dest, Sampler::Sample( context, frac, context->sourceheight ) );
				}
				frac += fracstep;
	
			} while (count--);
		}

		static INLINE void Draw( colcontext_t* context )									{ DrawWith< SamplerOriginal< 128 >::Direct, ViewportLookup, WriterDirect >( context ); }
		static INLINE void PaletteSwapDraw( colcontext_t* context )							{ DrawWith< SamplerOriginal< 128 >::PaletteSwap, ViewportLookup, WriterDirect >( context ); }
		static INLINE void ColormapDraw( colcontext_t* context )							{ DrawWith< SamplerOriginal< 128 >::Colormap, ViewportLookup, WriterDirect >( context ); }
		static INLINE void ColormapPaletteSwapDraw( colcontext_t* context )					{ DrawWith< SamplerOriginal< 128 >::ColormapPaletteSwap, ViewportLookup, WriterDirect >( context ); }

		template< size_t height >
		static INLINE void SizedDraw( colcontext_t* context )								{ DrawWith< typename SamplerOriginal< height >::Direct, ViewportLookup, WriterDirect >( context ); }
		template< size_t height >
		static INLINE void SizedPaletteSwapDraw( colcontext_t* context )					{ DrawWith< typename SamplerOriginal< height >::PaletteSwap, ViewportLookup, WriterDirect >( context ); }
		template< size_t height >
		static INLINE void SizedColormapDraw( colcontext_t* context )						{ DrawWith< typename SamplerOriginal< height >::Colormap, ViewportLookup, WriterDirect >( context ); }
		template< size_t height >
		static INLINE void SizedColormapPaletteSwapDraw( colcontext_t* context )			{ DrawWith< typename SamplerOriginal< height >::ColormapPaletteSwap, ViewportLookup, WriterDirect >( context ); }
		template< size_t height >
		static INLINE void SizedTransparentDraw( colcontext_t* context )					{ DrawWith< typename SamplerOriginal< height >::Colormap, ViewportLookup, WriterTransparent >( context ); }

		static INLINE void SpriteDraw( colcontext_t* context )								{ DrawWith< SamplerOriginal< 128 >::Direct, ViewportSpriteLookup, WriterDirect >( context ); }
		static INLINE void SpritePaletteSwapDraw( colcontext_t* context )					{ DrawWith< SamplerOriginal< 128 >::PaletteSwap, ViewportSpriteLookup, WriterDirect >( context ); }
		static INLINE void SpriteColormapDraw( colcontext_t* context )						{ DrawWith< SamplerOriginal< 128 >::Colormap, ViewportSpriteLookup, WriterDirect >( context ); }
		static INLINE void SpriteColormapPaletteSwapDraw( colcontext_t* context )			{ DrawWith< SamplerOriginal< 128 >::ColormapPaletteSwap, ViewportSpriteLookup, WriterDirect >( context ); }
		static INLINE void SpriteTransparentDraw( colcontext_t* context )					{ DrawWith< SamplerOriginal< 128 >::Colormap, ViewportSpriteLookup, WriterTransparent >( context ); }

		static INLINE void LimitRemovingDraw( colcontext_t* context )						{ DrawWith< SamplerLimitRemoving::Direct, ViewportLookup, WriterDirect >( context ); }
		static INLINE void LimitRemovingPaletteSwapDraw( colcontext_t* context )			{ DrawWith< SamplerLimitRemoving::PaletteSwap, ViewportLookup, WriterDirect >( context ); }
		static INLINE void LimitRemovingColormapDraw( colcontext_t* context )				{ DrawWith< SamplerLimitRemoving::Colormap, ViewportLookup, WriterDirect >( context ); }
		static INLINE void LimitRemovingColormapPaletteSwapDraw( colcontext_t* context )	{ DrawWith< SamplerLimitRemoving::ColormapPaletteSwap, ViewportLookup, WriterDirect >( context ); }
		static INLINE void LimitRemovingTransparentDraw( colcontext_t* context )			{ DrawWith< SamplerLimitRemoving::Colormap, ViewportLookup, WriterTransparent >( context ); }

		static INLINE void LimitRemovingSpriteColormapDraw( colcontext_t* context )			{ DrawWith< SamplerLimitRemoving::Colormap, ViewportSpriteLookup, WriterDirect >( context ); }
		static INLINE void LimitRemovingSpriteTransparentDraw( colcontext_t* context )		{ DrawWith< SamplerLimitRemoving::Colormap, ViewportSpriteLookup, WriterTransparent >( context ); }

		static INLINE void BackbufferDraw( colcontext_t* context )							{ DrawWith< SamplerLimitRemoving::Direct, BackbufferLookup, WriterDirect >( context ); }
		static INLINE void BackbufferPaletteSwapDraw( colcontext_t* context )				{ DrawWith< SamplerLimitRemoving::PaletteSwap, BackbufferLookup, WriterDirect >( context ); }
		static INLINE void BackbufferColormapDraw( colcontext_t* context )					{ DrawWith< SamplerLimitRemoving::Colormap, BackbufferLookup, WriterDirect >( context ); }
		static INLINE void BackbufferColormapPaletteSwapDraw( colcontext_t* context )		{ DrawWith< SamplerLimitRemoving::ColormapPaletteSwap, BackbufferLookup, WriterDirect >( context ); }
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

void R_DrawColumn_128( colcontext_t* context ) 
{ 
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedDraw< 128 >( context );
} 

void R_DrawColumn_Colormap_16( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedColormapDraw< 16 >( context );
}

void R_DrawColumn_Colormap_32( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedColormapDraw< 32 >( context );
}

void R_DrawColumn_Colormap_64( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedColormapDraw< 64 >( context );
}

void R_DrawColumn_Colormap_128( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedColormapDraw< 128 >( context );
}

void R_DrawColumn_Colormap_256( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedColormapDraw< 256 >( context );
}

void R_DrawColumn_Colormap_512( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedColormapDraw< 512 >( context );
}

void R_DrawColumn_Transparent_16( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedTransparentDraw< 16 >( context );
}

void R_DrawColumn_Transparent_32( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedTransparentDraw< 32 >( context );
}

void R_DrawColumn_Transparent_64( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedTransparentDraw< 64 >( context );
}

void R_DrawColumn_Transparent_128( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedTransparentDraw< 128 >( context );
}

void R_DrawColumn_Transparent_256( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedTransparentDraw< 256 >( context );
}

void R_DrawColumn_Transparent_512( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SizedTransparentDraw< 256 >( context );
}

void R_SpriteDrawColumn( colcontext_t* context ) 
{ 
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SpriteDraw( context );
} 

void R_SpriteDrawColumn_Colormap( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SpriteColormapDraw( context );
}

void R_SpriteDrawColumn_Transparent( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::SpriteTransparentDraw( context );
}

void R_LimitRemovingSpriteDrawColumn_Colormap( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::LimitRemovingSpriteColormapDraw( context );
}

void R_LimitRemovingSpriteDrawColumn_Transparent( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::LimitRemovingSpriteTransparentDraw( context );
}

void R_LimitRemovingDrawColumn( colcontext_t* context ) 
{ 
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::LimitRemovingDraw( context );
} 

void R_LimitRemovingDrawColumn_Colormap( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::LimitRemovingColormapDraw( context );
}

void R_LimitRemovingDrawColumn_Transparent( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::LimitRemovingTransparentDraw( context );
}

void R_BackbufferDrawColumn( colcontext_t* context ) 
{ 
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::BackbufferDraw( context );
} 

void R_BackbufferDrawColumn_Colormap( colcontext_t* context )
{
	M_PROFILE_FUNC();
	DrawColumn::Bytewise::BackbufferColormapDraw( context );
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

// 1,024 fuzz offsets. Overkill. And loving it.
#define ADJUSTEDFUZZTABLE 1024
#define ADJUSTEDFUZZTABLEMASK (ADJUSTEDFUZZTABLE-1)

static const int32_t adjustedfuzzoffset[ ADJUSTEDFUZZTABLE ] =
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
	-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
	-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,
	FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
};

thread_local int32_t	fuzzpos = 0;
thread_local int32_t	cachedfuzzpos = 0;
thread_local int32_t	framefuzzpos = 0;
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

void R_CacheFuzzColumnForFrame()
{
	framefuzzpos = cachedfuzzpos = fuzzpos;
}

void R_RestoreFuzzColumnForFrame()
{
	cachedfuzzpos = fuzzpos = framefuzzpos;
}

void R_DrawFuzzColumn ( colcontext_t* context ) 
{ 
    int					count; 
    pixel_t*			dest;
	int32_t				fuzzposbase;
	int32_t				fuzzposadd;

	fuzzposbase = fuzzpos;
	fuzzposadd = fuzzposbase;

	count = context->yh - context->yl;

	dest = context->output.data + context->x * context->output.pitch + context->yl;

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

		++fuzzposadd;
	
	} while (count--);

	// Doing the fuzzprime add basically eliminates banding
	// But you kinda want it anyway to be able to see anything at high res...
	fuzzpos += ( fuzzposadd - fuzzposbase /*+ ( M_Random() % FUZZPRIME )*/ );
} 

#define OLDFUZZ 1

#if OLDFUZZ
void R_DrawAdjustedFuzzColumn( colcontext_t* context )
{
	rend_fixed_t		originalcolfrac = RendFixedDiv( IntToRendFixed( 200 ), IntToRendFixed( drs_current->viewheight ) );
	rend_fixed_t		oldfrac = 0;
	rend_fixed_t		newfrac = 0;

	int32_t				fuzzposadd			= fuzzpos = cachedfuzzpos;
	int32_t				fuzzpossample		= adjustedfuzzoffset[ fuzzposadd & ADJUSTEDFUZZTABLEMASK ];
	pixel_t*			samplecolormap		= fuzzpossample < 0 ? context->fuzzdarkmap : context->fuzzlightmap;

	int32_t count = context->yh - context->yl; 

	pixel_t* dest = context->output.data + context->x * context->output.pitch + context->yl;

	// count + 3 is correct. Since a 0 count will always sample one pixel. So make sure we copy one on each side of the sample.
	memcpy( context->fuzzworkingbuffer, dest - 1, sizeof( pixel_t ) * ( count + 3 ) );
	pixel_t* src = context->fuzzworkingbuffer + 1;
	int32_t srcadd = 0;

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do 
    {
		*dest = samplecolormap[ src[ fuzzpossample ] ]; 

		++dest;
		++srcadd;

		oldfrac = newfrac;
		newfrac += originalcolfrac;

		if( RendFixedToInt( newfrac ) > RendFixedToInt( oldfrac ) )
		{
			// Clamp table lookup index.
			++fuzzposadd;
			src += srcadd;
			srcadd = 0;
			fuzzpossample = adjustedfuzzoffset[ fuzzposadd & ADJUSTEDFUZZTABLEMASK ];
			samplecolormap = fuzzpossample < 0 ? context->fuzzdarkmap : context->fuzzlightmap;
		}

	} while (count--);

	fuzzpos = fuzzposadd + ( M_Random() % FUZZPRIME );
}

#else // !OLDFUZZ

void R_DrawAdjustedFuzzColumn( colcontext_t* context )
{
	rend_fixed_t		originalcolfrac		= drs_current->skyiscaley;
	rend_fixed_t		oldfrac = 0;
	rend_fixed_t		newfrac = 0;

	constexpr rend_fixed_t offset = DoubleToRendFixed( 0.5 );

	int32_t				fuzzposbase			= RendFixedToInt( drs_current->skyscaley + offset );
	int32_t				fuzzposadd			= fuzzpos = cachedfuzzpos;
	int32_t				fuzzpossample		= adjustedfuzzoffset[ fuzzposadd & ADJUSTEDFUZZTABLEMASK ] * fuzzposbase;
	pixel_t*			samplecolormap		= context->fuzzlightmap;

	int32_t count = context->yh - context->yl; 

	pixel_t* dest = context->output.data + context->x * context->output.pitch + context->yl;
	pixel_t* src = context->output.data + context->x * context->output.pitch + M_MAX( 0, context->yl + fuzzpossample );
	pixel_t sampled = samplecolormap[ src[ fuzzpossample ] ]; 

	int32_t srcadd = 0;

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do 
    {
		*dest = sampled; 

		++dest;
		++srcadd;

		oldfrac = newfrac;
		newfrac += originalcolfrac;

		if( RendFixedToInt( newfrac ) > RendFixedToInt( oldfrac ) )
		{
			// Clamp table lookup index.
			++fuzzposadd;
			src += srcadd;
			srcadd = 0;
			fuzzpossample = adjustedfuzzoffset[ fuzzposadd & ADJUSTEDFUZZTABLEMASK ] * fuzzposbase;
			samplecolormap = context->fuzzlightmap;
			sampled = samplecolormap[ src[ fuzzpossample ] ]; 
		}

	} while (count--);

	fuzzpos = fuzzposadd + ( M_Random() % FUZZPRIME );
}

#endif // OLDFUZZ

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
// R_InitBuffer 
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//
void R_InitBuffer( drsdata_t* current, int width, int height )
{ 
    // Handle resize,
    //  e.g. smaller view windows
    //  with border and/or status bar.
    current->viewwindowx = (current->frame_width - width) >> 1; 
    // Samw with base row offset.
    if (width == current->frame_width) 
		current->viewwindowy = 0;
    else 
		current->viewwindowy = ( current->frame_height - SBARHEIGHT( current ) - height ) >> 1;

	//background_data.width = background_data.height = 0;
	//if (background_data.data != NULL)
	//{
	//	Z_Free(background_data.data);
	//}
	//
	//background_data.data = NULL;
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

	int32_t viewx = FixedToInt( FixedDiv( IntToFixed( drs_current->viewwindowx ),		V_WIDTHMULTIPLIER ) );
	int32_t viewy = FixedToInt( FixedDiv( IntToFixed( drs_current->viewwindowy ),		V_HEIGHTMULTIPLIER ) );
	int32_t vieww = FixedToInt( FixedDiv( IntToFixed( drs_current->scaledviewwidth ),	V_WIDTHMULTIPLIER ) );
	int32_t viewh = FixedToInt( FixedDiv( IntToFixed( drs_current->viewheight ),			V_HEIGHTMULTIPLIER ) );

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

	if( drs_current->scaledviewwidth == drs_current->frame_width
		|| background_data.width != render_width
		|| background_data.height != render_height )
	{
		if (background_data.data != NULL
			&& (render_width > background_data.width || render_height > background_data.height ) )
		{
			Z_Free( background_data.data );
			memset( &background_data, 0, sizeof( background_data ) );
		}

		if( drs_current->scaledviewwidth == drs_current->frame_width )
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
			// WIDESCREEN HACK
			int32_t xpos = -( ( interpic->width - V_VIRTUALWIDTH ) / 2 );
			V_DrawPatch( xpos, 0, interpic );
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
 
    if (drs_current->scaledviewwidth == drs_current->frame_width) 
	return; 
  
    top = (drs_current->frame_height - SBARHEIGHT( drs_current ) - drs_current->viewheight)/2;
    side = (drs_current->frame_width - drs_current->scaledviewwidth)/2;

	ofs = 0;
	for( i = 0; i < side; ++i )
	{
		R_VideoErase( ofs, drs_current->frame_height - SBARHEIGHT( drs_current ) );
		ofs += drs_current->frame_height;
	}

	for( i = side; i < render_width - side; ++i )
	{
		R_VideoErase( ofs, top );
		R_VideoErase( ofs + drs_current->frame_height - SBARHEIGHT( drs_current ) - top, top );
		ofs += drs_current->frame_height;
	}

	for( i = render_width - side; i < render_width; ++i )
	{
		R_VideoErase( ofs, drs_current->frame_height - SBARHEIGHT( drs_current ) );
		ofs += drs_current->frame_height;
	}

    V_MarkRect (0,0,drs_current->frame_width, drs_current->frame_height-SBARHEIGHT( drs_current )); 
} 
 
 

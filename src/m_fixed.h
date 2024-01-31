//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020-2022 Ethan Watson
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
//	Fixed point arithemtics, implementation.
//


#ifndef __M_FIXED__
#define __M_FIXED__


#include "doomtype.h"
#define FORCE_C_FIXED 1

// Once we're more in C++ land, this will become the default type
#if defined( __cplusplus ) && !defined( FORCE_C_FIXED )
#define USE_FIXED_T_TYPE 1
#else
#define USE_FIXED_T_TYPE 0
#endif

#if defined( __cplusplus )
template< typename _ty >
constexpr _ty RightmostBit( _ty val )
{
	return val & ( ~val + 1 );
}

template< typename _ty >
constexpr _ty FirstSetBitIndex( _ty val )
{
	if( !val ) return -1;

	_ty index = 0;
	while( !( val & ( (_ty)1 << index ) ) )
	{
		++index;
	}

	return index;
}

constexpr bool IsPowerOf2( size_t val )
{
	return val != 0 && ( val & ( val - 1 ) ) == 0;
}

constexpr bool IsValidBitCount( size_t val )
{
	return	val <= ( sizeof( size_t ) * 8 )
			&& IsPowerOf2( val );
}

template< size_t boundary, typename _val >
requires ( IsPowerOf2( boundary ) )
constexpr size_t AlignTo( _val val )
{
	constexpr _val clip = boundary - 1;
	constexpr _val mask = ~clip;
	return ( val + clip ) & mask;
}
#endif // defined( __cplusplus )

#if !USE_FIXED_T_TYPE
//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS						16
#define FRACUNIT						(1<<FRACBITS)
#define FRACMASK						( FRACUNIT - 1 )
#define FRACSIGNBIT						0x80000000
#define FRACFILL( x, o )				( ( x ) | ( ( o ) < 0 ? ( FRACMASK << ( 32 - FRACBITS ) ) : 0 ) )

// 64 bit as 44.20
#define RENDFRACBITS					20ll
#define RENDFRACUNIT					( 1ll << RENDFRACBITS )
#define RENDFRACMASK					( RENDFRACUNIT - 1ll )
#define RENDFRACSIGNBIT					0x8000000000000000ll
#define RENDFRACFILL( x, o )			( ( x ) | ( ( o ) < 0 ? ( RENDFRACMASK << ( 64 - RENDFRACBITS ) ) : 0 ) )
#define RENDFRACFILLFIXED( x, o )		( ( x ) | ( ( o ) < 0 ? ( RENDFRACMASK << ( 64 - ( RENDFRACBITS - FRACBITS ) ) ) : 0 ) )

#define RENDFRACTOFRACBITS				( RENDFRACBITS - FRACBITS )

typedef int32_t fixed_t;
typedef int64_t rend_fixed_t;

CONSTEXPR fixed_t FixedMul( fixed_t a, fixed_t b );
CONSTEXPR fixed_t FixedDiv( fixed_t a, fixed_t b );

CONSTEXPR fixed_t FixedRound( fixed_t a );

CONSTEXPR rend_fixed_t RendFixedMul( rend_fixed_t a, rend_fixed_t b );
CONSTEXPR rend_fixed_t RendFixedDiv( rend_fixed_t a, rend_fixed_t b );
CONSTEXPR rend_fixed_t RendFixedLerp( rend_fixed_t from, rend_fixed_t to, rend_fixed_t percent );

#define IntToFixed( x ) ( ( x ) << FRACBITS )
#define FixedToInt( x ) FRACFILL( ( x ) >> FRACBITS, ( x ) )

#define IntToRendFixed( x ) ( (rend_fixed_t)( x ) << RENDFRACBITS )
#define RendFixedToInt( x ) ( (int32_t)RENDFRACFILL( ( x ) >> RENDFRACBITS, ( x ) ) )

#define FixedToRendFixed( x ) ( (rend_fixed_t)( x ) << RENDFRACTOFRACBITS )
#define RendFixedToFixed( x ) ( (fixed_t)RENDFRACFILLFIXED( ( x ) >> RENDFRACTOFRACBITS, ( x ) ) )

#define DoubleToFixed( x ) ( (fixed_t)( ( x ) * FRACUNIT ) )
#define DoubleToRendFixed( x ) ( (rend_fixed_t)( ( x ) * RENDFRACUNIT ) )

#include "m_fixed.inl"

#else // USED_FIXED_T_TYPE

#include <type_traits>
#include <limits>

template< size_t Bits, bool Signed >
struct IntegralType
{
};

template< bool Signed >
struct IntegralType< 8, Signed >
{
	using type = std::conditional_t< Signed, int8_t, uint8_t >;
};

template< bool Signed >
struct IntegralType< 16, Signed >
{
	using type = std::conditional_t< Signed, int16_t, uint16_t >;
};

template< bool Signed >
struct IntegralType< 32, Signed >
{
	using type = std::conditional_t< Signed, int32_t, uint32_t >;
};

template< bool Signed >
struct IntegralType< 64, Signed >
{
	using type = std::conditional_t< Signed, int64_t, uint64_t >;
};

template< size_t IntegralBits, size_t FractionalBits >
requires IsValidBitCount( IntegralBits + FractionalBits )
class FixedPoint
{
public:
	static constexpr size_t integral_bits			= IntegralBits;
	static constexpr size_t fractional_bits			= FractionalBits;
	static constexpr size_t total_bits				= IntegralBits + FractionalBits;

	using value_type								= IntegralType< total_bits, true >::type;
	using working_type								= IntegralType< ( total_bits << 1 > 64 ? 64 : total_bits << 1 ), true >::type;

	static constexpr value_type one					= (value_type)1 << FractionalBits;
	static constexpr value_type fractional_mask		= one - 1;
	static constexpr value_type half				= one >> 1;

	FixedPoint() = default;
	FixedPoint( value_type val ) : storage( val ) { }

	INLINE FixedPoint operator*( const FixedPoint& rhs )
	{
		return { ( (working_type)storage * (working_type)rhs.storage ) >> fractional_bits };
	}

	INLINE FixedPoint operator/( const FixedPoint& rhs )
	{
		constexpr size_t Shift = integral_bits - 2;

		if( ( abs( storage ) >> Shift ) >= abs( rhs.storage ) )
		{
			return { ( storage ^ rhs.storage ) < 0 ? std::numeric_limits< value_type >::min() : std::numeric_limits< value_type >::max() };
		}

		return { ( (working_type)storage << fractional_bits ) / rhs.storage };
	}

	INLINE FixedPoint operator+( const FixedPoint& rhs )
	{
		return { storage + rhs.storage };
	}

	INLINE FixedPoint operator-( const FixedPoint& rhs )
	{
		return { storage - rhs.storage };
	}

	INLINE FixedPoint operator-()
	{
		return { -storage };
	}

	INLINE FixedPoint& operator*=( const FixedPoint& rhs )	{ *this = *this * rhs; return *this; }
	INLINE FixedPoint& operator/=( const FixedPoint& rhs )	{ *this = *this / rhs; return *this; }
	INLINE FixedPoint& operator+=( const FixedPoint& rhs )	{ *this = *this + rhs; return *this; }
	INLINE FixedPoint& operator-=( const FixedPoint& rhs )	{ *this = *this - rhs; return *this; }

	// We will allow divide and multiply by integer, since they don't need
	// to be translated in to a higher/lower bit space to actually work
	template< typename _ty, std::enable_if_t< std::is_integral_v< _ty >, _ty > = 0 >
	INLINE FixedPoint operator/( _ty rhs )
	{
		return { storage / rhs };
	}

	template< typename _ty, std::enable_if_t< std::is_integral_v< _ty >, _ty > = 0 >
	INLINE FixedPoint operator*( _ty rhs )
	{
		return { storage * rhs };
	}

	INLINE operator value_type() { return storage; }
private:
	value_type		storage;
};

using fixed_t = FixedPoint< 16, 16 >;
using rend_fixed_t = FixedPoint< 44, 20 >;

#endif // USE_FIXED_T_TYPE

#endif

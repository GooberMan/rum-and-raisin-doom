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
//	Fixed point arithemtics, implementation.
//


#ifndef __M_FIXED__
#define __M_FIXED__


#include "doomtype.h"

//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS		16
#define FRACUNIT		(1<<FRACBITS)

typedef int32_t fixed_t;

DOOM_C_API fixed_t FixedMul	(fixed_t a, fixed_t b);
DOOM_C_API fixed_t FixedDiv	(fixed_t a, fixed_t b);

// Once we're more in C++ land, this will become the default type
#define USE_FIXED_T_TYPE 0

#if USED_FIXED_T_TYPE
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
requires ( IntegralBits + FractionalBits <= _INTEGRAL_MAX_BITS )
class FixedPoint
{
public:
	static constexpr size_t integral_bits			= IntegralBits;
	static constexpr size_t fractional_bits			= FractionalBits;
	static constexpr size_t fractional_shift		= ( 1ull << FractionalBits );
	static constexpr size_t total_bits				= IntegralBits + FractionalBits;

	using value_type								= IntegralType< total_bits, true >::type;
	using working_type								= IntegralType< ( integral_bits << 1 > 64 ? 64 : integral_bits ), true >::type;

	static constexpr value_type one					= (value_type)1 << FractionalBits;
	static constexpr value_type fractional_mask		= one - 1;
	static constexpr value_type half				= one >> 1;

	FixedPoint() = default;
	FixedPoint( value_type val ) : storage( val ) { }

	INLINE FixedPoint& operator*( const FixedPoint& rhs )
	{
		storage = ( (working_type)storage * (working_type)rhs.storage ) >> fractional_bits;
		return *this;
	}

	INLINE FixedPoint& operator/( const FixedPoint& rhs )
	{
		constexpr size_t Shift = integral_bits - 2;

		if( ( abs( storage ) >> Shift ) >= abs( rhs.storage ) )
		{
			return ( storage ^ rhs.storage ) < 0 ? std::numeric_limits< value_type >::min() : std::numeric_limits< value_type >::max();
		}

		storage = ( (working_type)storage << fractional_bits ) / rhs.storage;
		return *this;
	}

	INLINE FixedPoint& operator+( const FixedPoint& rhs )
	{
		storage += rhs.storage;
		return *this;
	}

	INLINE FixedPoint& operator-( const FixedPoint& rhs )
	{
		storage -= rhs.storage;
		return *this;
	}

	INLINE FixedPoint& operator-()
	{
		storage = -storage;
		return *this;
	}

	INLINE operator value_type() { return storage; }
private:
	value_type		storage;
};

using fixed_t = FixedPoint< 16, 16 >;
using rend_fixed_t = FixedPoint< 40, 24 >;

#endif // USE_FIXED_T_TYPE

#endif

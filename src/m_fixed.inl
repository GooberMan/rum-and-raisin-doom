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

CONSTEXPR fixed_t FixedAbs( fixed_t val )
{
	fixed_t sign = val >> 31;
	return ( val ^ sign ) - sign;
}

CONSTEXPR fixed_t FixedMul( fixed_t a, fixed_t b )
{
	return ((int64_t) a * (int64_t) b) >> FRACBITS;
}

CONSTEXPR fixed_t FixedDiv(fixed_t a, fixed_t b)
{
	if ((FixedAbs(a) >> 14) >= FixedAbs(b))
	{
		return (a^b) < 0 ? INT_MIN : INT_MAX;
	}

	int64_t result = ((int64_t) a << FRACBITS) / b;

	return (fixed_t) result;
}

CONSTEXPR fixed_t FixedLerp( fixed_t from, fixed_t to, fixed_t percent )
{
	return from + FixedMul( ( to - from ), percent );
}

// Rounds to nearest whole number
CONSTEXPR fixed_t FixedRound( fixed_t a )
{
	return ( a & ~FRACMASK ) + ( ( a & ( 1 << ( FRACBITS - 1 ) ) ) << 1 );
}

CONSTEXPR rend_fixed_t RendFixedAbs( rend_fixed_t val )
{
	rend_fixed_t sign = val >> 63ll;
	return ( val ^ sign ) - sign;
}

CONSTEXPR rend_fixed_t RendFixedMul( rend_fixed_t a, rend_fixed_t b )
{
	rend_fixed_t result = ( a * b ) >> RENDFRACBITS;

	return RENDFRACFILL( result, result );
}

CONSTEXPR rend_fixed_t RendFixedDiv( rend_fixed_t a, rend_fixed_t b )
{
	if ( ( RendFixedAbs( a ) >> ( RENDFRACBITS - 2 ) ) >= RendFixedAbs( b ) )
	{
		return ( a ^ b ) < 0 ? LONG_MIN : LONG_MAX;
	}
	rend_fixed_t result = ( a << RENDFRACBITS ) / b;
	return (rend_fixed_t) result;
}

CONSTEXPR rend_fixed_t RendFixedLerp( rend_fixed_t from, rend_fixed_t to, rend_fixed_t percent )
{
	return from + RendFixedMul( ( to - from ), percent );
}

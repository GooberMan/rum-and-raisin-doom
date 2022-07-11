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


INLINE fixed_t FixedMul( fixed_t a, fixed_t b )
{
	return ((int64_t) a * (int64_t) b) >> FRACBITS;
}

INLINE fixed_t FixedDiv(fixed_t a, fixed_t b)
{
	if ((abs(a) >> 14) >= abs(b))
	{
		return (a^b) < 0 ? INT_MIN : INT_MAX;
	}

	int64_t result = ((int64_t) a << FRACBITS) / b;

	return (fixed_t) result;
}

INLINE rend_fixed_t RendFixedMul( rend_fixed_t a, rend_fixed_t b )
{
	rend_fixed_t result = ( a * b ) >> RENDFRACBITS;

	return RENDFRACFILL( result, result );
}

INLINE rend_fixed_t RendFixedDiv( rend_fixed_t a, rend_fixed_t b )
{
	if ( ( llabs( a ) >> ( RENDFRACBITS - 2 ) ) >= llabs( b ) )
	{
		return ( a ^ b ) < 0 ? LONG_MIN : LONG_MAX;
	}
	rend_fixed_t result = ( a << RENDFRACBITS ) / b;
	return (rend_fixed_t) result;
}

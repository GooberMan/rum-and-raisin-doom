//
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
//
// Definitions for use in the dehacked code
//

#ifndef DEH_DEFS_H
#define DEH_DEFS_H

#include "sha1.h"
#include "doomtype.h"

#if defined( __cplusplus )
#include <type_traits>

template< typename _ty >
struct is_some_string : std::false_type { };

template<>
struct is_some_string< char* > : std::true_type { };

template<>
struct is_some_string< const char* > : std::true_type { };

template< typename _ty >
constexpr bool is_some_string_v = is_some_string< _ty >::value;

constexpr uint64_t FNV1aBasis32 = 2166136261u;

constexpr uint32_t fnv1a32( uint32_t base, uint8_t byte )
{
	constexpr uint32_t Prime = 16777619u;

	base ^= (uint32_t)byte;
	base *= Prime;
	return base;
}

template< typename _ty
		, std::enable_if_t< std::is_integral_v< _ty > && !std::is_same_v< _ty, uint8_t >, bool > = true >
constexpr uint32_t fnv1a32( uint32_t base, const _ty& val )
{
	union
	{
		_ty		base;
		uint8_t	byte[ sizeof( _ty ) ];
	} mapped = { val };

	for( uint32_t index = 0; index < sizeof( _ty ); ++index )
	{
		base = fnv1a32( base, mapped.byte[ index ] );
	}

	return base;
}

constexpr uint32_t fnv1a32( uint32_t base, const char* string )
{
	while( string && *string )
	{
		base = fnv1a32( base, (uint8_t)*string );
		++string;
	}

	return base;
}

#endif // defined( __cplusplus )

DOOM_C_API typedef struct deh_context_s deh_context_t;
DOOM_C_API typedef struct deh_section_s deh_section_t;
DOOM_C_API typedef void (*deh_section_init_t)(void);
DOOM_C_API typedef void *(*deh_section_start_t)(deh_context_t *context, char *line);
DOOM_C_API typedef void (*deh_section_end_t)(deh_context_t *context, void *tag);
DOOM_C_API typedef void (*deh_line_parser_t)(deh_context_t *context, char *line, void *tag);
DOOM_C_API typedef void (*deh_sha1_hash_t)(sha1_context_t *context);
DOOM_C_API typedef uint32_t (*deh_fnv1a32_hash_t)(int32_t version, uint32_t base);

DOOM_C_API struct deh_section_s
{
    const char *name;

    // Called on startup to initialize code

    deh_section_init_t init;
    
    // This is called when a new section is started.  The pointer
    // returned is used as a tag for the following calls.

    deh_section_start_t start;

    // This is called for each line in the section

    deh_line_parser_t line_parser;

    // This is called at the end of the section for any cleanup

    deh_section_end_t end;

    // Called when generating an SHA1 sum of the dehacked state

    deh_sha1_hash_t sha1_hash;

	// Called after a dehacked patch has finished parsing to provide a hash value of all loaded objects
	deh_fnv1a32_hash_t fnv1_hash;
};

#endif /* #ifndef DEH_DEFS_H */



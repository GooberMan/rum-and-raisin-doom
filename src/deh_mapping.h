//
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
//
// Dehacked "mapping" code
// Allows the fields in structures to be mapped out and accessed by
// name
//

#ifndef DEH_MAPPING_H
#define DEH_MAPPING_H

#include "doomtype.h"
#include "deh_io.h"
#include "sha1.h"

#define DEH_BEGIN_MAPPING(mapping_name, structname)           \
    static structname deh_mapping_base;                       \
    static deh_mapping_t mapping_name =                       \
    {                                                         \
        &deh_mapping_base,                                    \
        {

#define DEH_MAPPING(deh_name, fieldname)                      \
             {deh_name, &deh_mapping_base.fieldname,          \
                 sizeof(deh_mapping_base.fieldname),          \
                 is_some_string_v< decltype( deh_mapping_base.fieldname ) >, exe_doom_1_2},

#define MBF21_MAPPING(deh_name, fieldname)                    \
             {deh_name, &deh_mapping_base.fieldname,          \
                 sizeof(deh_mapping_base.fieldname),          \
                 is_some_string_v< decltype( deh_mapping_base.fieldname ) >, exe_mbf21},

#define RNR_MAPPING(deh_name, fieldname)                      \
             {deh_name, &deh_mapping_base.fieldname,          \
                 sizeof(deh_mapping_base.fieldname),          \
                 is_some_string_v< decltype( deh_mapping_base.fieldname ) >, exe_rnr24},

#define DEH_UNSUPPORTED_MAPPING(deh_name)                     \
             {deh_name, NULL, -1, false, exe_invalid},

#define DEH_END_MAPPING                                       \
             {NULL, NULL, -1}                                 \
        }                                                     \
    };



#define MAX_MAPPING_ENTRIES 64

typedef struct deh_mapping_s deh_mapping_t;
typedef struct deh_mapping_entry_s deh_mapping_entry_t;

DOOM_C_API struct deh_mapping_entry_s 
{
    // field name
   
    const char *name;

    // location relative to the base in the deh_mapping_t struct
    // If this is NULL, it is an unsupported mapping

    void *location;

    // field size

    int size;

    // if true, this is a string value.

    doombool is_string;

	GameVersion_t minimum_version;
};

DOOM_C_API struct deh_mapping_s
{
    void *base;
    deh_mapping_entry_t entries[MAX_MAPPING_ENTRIES];
};

DOOM_C_API doombool DEH_SetMapping(deh_context_t *context, deh_mapping_t *mapping,
                       void *structptr, char *name, int value);
DOOM_C_API doombool DEH_SetStringMapping(deh_context_t *context, deh_mapping_t *mapping,
                             void *structptr, char *name, char *value);
DOOM_C_API void DEH_StructSHA1Sum(sha1_context_t *context, deh_mapping_t *mapping,
                       void *structptr);

#endif /* #ifndef DEH_MAPPING_H */


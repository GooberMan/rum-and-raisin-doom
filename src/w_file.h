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
//	WAD I/O functions.
//


#ifndef __W_FILE__
#define __W_FILE__

#include <stdio.h>
#include "doomtype.h"

DOOM_C_API typedef enum wadtype_e
{
	wt_none			= 0x00000000,
	wt_iwad			= 0x00000001,
	wt_pwad			= 0x00000002,
	wt_singlelump	= 0x00000004,
	wt_system		= 0x00000008,
	wt_widepix		= 0x00000010,
	wt_boomassets	= 0x00000020,
	wt_mbfassets	= 0x00000040,
} wadtype_t;

DOOM_C_API typedef struct _wad_file_s wad_file_t;

DOOM_C_API typedef struct
{
    // Open a file for reading.
    wad_file_t *(*OpenFile)(const char *path);

    // Close the specified file.
    void (*CloseFile)(wad_file_t *file);

    // Read data from the specified position in the file into the 
    // provided buffer.  Returns the number of bytes read.
    size_t (*Read)(wad_file_t *file, unsigned int offset,
                   void *buffer, size_t buffer_len);
} wad_file_class_t;

DOOM_C_API struct _wad_file_s
{
    // Class of this file.
    wad_file_class_t *file_class;

    // If this is NULL, the file cannot be mapped into memory.  If this
    // is non-NULL, it is a pointer to the mapped file.
    byte *mapped;

    // Length of the file, in bytes.
    unsigned int length;

	wadtype_t	type;

    // File's location on disk.
    const char *path;

	lumpindex_t		minlump;
	lumpindex_t		maxlump;
};

// Open the specified file. Returns a pointer to a new wad_file_t 
// handle for the WAD file, or NULL if it could not be opened.

DOOM_C_API wad_file_t *W_OpenFile(const char *path);

// Close the specified WAD file.

DOOM_C_API void W_CloseFile(wad_file_t *wad);

// Read data from the specified file into the provided buffer.  The
// data is read from the specified offset from the start of the file.
// Returns the number of bytes read.

DOOM_C_API size_t W_Read(wad_file_t *wad, unsigned int offset,
              void *buffer, size_t buffer_len);

#endif /* #ifndef __W_FILE__ */

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
//	Disk load indicator.
//

#include "doomtype.h"
#include "deh_str.h"
#include "i_swap.h"
#include "i_video.h"
#include "m_argv.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "v_diskicon.h"

// Only display the disk icon if more then this much bytes have been read
// during the previous tic.

static const size_t diskicon_threshold = 0;

// Two buffers: disk_data contains the data representing the disk icon
// (raw, not a patch_t) while saved_background is an equivalently-sized
// buffer where we save the background data while the disk is on screen.

static int32_t loading_disk_xoffs = 0;
static int32_t loading_disk_yoffs = 0;
static patch_t* disk_icon_patch = NULL;
static int32_t disk_icon_lumpnum = -1;
static size_t recent_bytes_read = 0;

void V_EnableLoadingDisk(const char *lump_name, int xoffs, int yoffs)
{
	loading_disk_xoffs = xoffs;
	loading_disk_yoffs = yoffs;

	if( disk_icon_patch != NULL )
	{
		W_ReleaseLumpNum( disk_icon_lumpnum );
		disk_icon_patch = NULL;
		disk_icon_lumpnum = -1;
	}

	if( lump_name != NULL )
	{
		disk_icon_lumpnum = W_GetNumForName( lump_name );
		disk_icon_patch = W_CacheLumpNum( disk_icon_lumpnum, PU_STATIC );
	}
}

void V_BeginRead(size_t nbytes)
{
	recent_bytes_read += nbytes;
}

void V_DrawDiskIcon(void)
{
	if (disk_icon_patch != NULL && recent_bytes_read > diskicon_threshold)
	{
		V_DrawPatch( loading_disk_xoffs, loading_disk_yoffs, disk_icon_patch, NULL, NULL );
	}

	recent_bytes_read = 0;
}

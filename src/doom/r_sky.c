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
//  Sky rendering. The DOOM sky is a texture map like any
//  wall, wrapping around. A 1024 columns equal 360 degrees.
//  The default sky map is 256 columns and repeats 4 times
//  on a 320 screen?
//  
//



// Needed for FRACUNIT.
#include "m_fixed.h"

// Needed for Flat retrieval.
#include "r_data.h"


#include "r_sky.h"

//
// sky mapping
//
int			skyflatnum;
int			skytexture;
int			skytexturemid;



//
// R_InitSkyMap
// Called whenever the view size changes.
//

// Used to do SCREENHEIGHT/2 to get the mid point for sky rendering
// Turns out 100 is the magic number for any screen resolution
// Makes sense really, it is meant to be sky texture relative not screen relative
// So let's leave the ratio here to support arbitrary sky heights later
#define SKYHEIGHT 128
#define SKYMID ( SKYHEIGHT * 781250 / 1000000 )
void R_InitSkyMap (void)
{
  // skyflatnum = R_FlatNumForName ( SKYFLATNAME );
    skytexturemid = IntToFixed( 100 );
}


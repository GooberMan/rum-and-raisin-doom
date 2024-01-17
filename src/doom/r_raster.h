//
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
// DESCRIPTION: Perspective correct rasteriser
//

#ifndef __R_RASTER__
#define __R_RASTER__

#ifdef __cplusplus
extern "C" {
#endif

#include "r_data.h"

#ifdef __cplusplus
}
#endif

DOOM_C_API void R_RasteriseRegion( viewpoint_t* viewpoint, planecontext_t* planecontext, rasterregion_t* firstregion, texturecomposite_t* texture );

#endif // __I_THREAD__

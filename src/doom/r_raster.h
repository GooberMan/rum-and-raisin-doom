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

#include "r_data.h"

#if defined(__cplusplus)

void R_RasteriseRegion16x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion16x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion16x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion16x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion16x256( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion16x512( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );

void R_RasteriseRegion32x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion32x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion32x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion32x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion32x256( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion32x512( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );

void R_RasteriseRegion64x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion64x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion64x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion64x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion64x256( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion64x512( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );

void R_RasteriseRegion128x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion128x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion128x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion128x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion128x256( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion128x512( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );

void R_RasteriseRegion256x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion256x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion256x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion256x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion256x256( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion256x512( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );

void R_RasteriseRegion512x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion512x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion512x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion512x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion512x256( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion512x512( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );

void R_RasteriseRegion1024x16( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion1024x32( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion1024x64( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion1024x128( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion1024x256( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );
void R_RasteriseRegion1024x512( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );

void R_RasteriseRegionArbitrary( rendercontext_t* rendercontext, rasterregion_t* firstregion, texturecomposite_t* texture );

#endif // defined(__cplusplus)

#endif // __I_THREAD__

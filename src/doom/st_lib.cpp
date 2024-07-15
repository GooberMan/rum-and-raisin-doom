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
//	The status bar widget code.
//


#include <stdio.h>
#include <ctype.h>

#include "deh_main.h"
#include "doomdef.h"

#include "p_local.h"

#include "z_zone.h"
#include "v_video.h"

#include "i_swap.h"
#include "i_system.h"

#include "w_wad.h"

#include "st_stuff.h"
#include "st_lib.h"
#include "r_local.h"


// in AM_map.c
extern doombool		automapactive; 




//
// Hack display negative frags.
//  Loads and store the stminus lump.
//
patch_t*		sttminus;

void STlib_init(void)
{
    if (W_CheckNumForName(DEH_String("STTMINUS")) >= 0)
        sttminus = (patch_t *) W_CacheLumpName(DEH_String("STTMINUS"), PU_STATIC);
    else
        sttminus = NULL;
}


// ?
void
STlib_initNum
( st_number_t*		n,
  int			x,
  int			y,
  patch_t**		pl,
  int*			num,
  doombool*		on,
  int			width )
{
    n->x	= x;
    n->y	= y;
    n->oldnum	= 0;
    n->width	= width;
    n->num	= num;
    n->on	= on;
    n->p	= pl;
}


// 
// A fairly efficient way to draw a number
//  based on differences from the old number.
// Note: worth the trouble?
//
void
STlib_drawNum
( st_number_t*	n,
  doombool	refresh )
{

    int		numdigits = n->width;
    int		num = *n->num;
    
    int		w = SHORT(n->p[0]->width);
    int		h = SHORT(n->p[0]->height);
    int		x = n->x;
    
    int		neg;

    n->oldnum = *n->num;

    neg = num < 0;

    if (neg)
    {
	if (numdigits == 2 && num < -9)
	    num = -9;
	else if (numdigits == 3 && num < -99)
	    num = -99;
	
	num = -num;
    }

    // clear the area
    x = n->x - numdigits*w;

    if (n->y - ST_Y < 0)
	I_Error("drawNum: n->y - ST_Y < 0");

    // if non-number, do not draw it
    if (num == 1994)
	return;

    x = n->x;

    // in the special case of 0, you draw 0
    if (!num)
	V_DrawPatch(x - w, n->y, n->p[ 0 ]);

    // draw the new number
    while (num && numdigits--)
    {
	x -= w;
	V_DrawPatch(x, n->y, n->p[ num % 10 ]);
	num /= 10;
    }

    // draw a minus sign if necessary
    if (neg && sttminus)
	V_DrawPatch(x - 8, n->y, sttminus);
}


//
void
STlib_updateNum
( st_number_t*		n,
  doombool		refresh )
{
    if (*n->on) STlib_drawNum(n, refresh);
}


//
void
STlib_initPercent
( st_percent_t*		p,
  int			x,
  int			y,
  patch_t**		pl,
  int*			num,
  doombool*		on,
  patch_t*		percent )
{
    STlib_initNum(&p->n, x, y, pl, num, on, 3);
    p->p = percent;
}




void
STlib_updatePercent
( st_percent_t*		per,
  int			refresh )
{
    if (refresh && *per->n.on)
	V_DrawPatch(per->n.x, per->n.y, per->p);
    
    STlib_updateNum(&per->n, refresh);
}



void
STlib_initMultIcon
( st_multicon_t*	i,
  int			x,
  int			y,
  patch_t**		il,
  int*			inum,
  doombool*		on )
{
    i->x	= x;
    i->y	= y;
    i->oldinum 	= -1;
    i->inum	= inum;
    i->on	= on;
    i->p	= il;
}



void
STlib_updateMultIcon
( st_multicon_t*	mi,
  doombool		refresh )
{
	if (*mi->on
		&& (mi->oldinum != *mi->inum || refresh)
		&& (*mi->inum!=-1))
	{
		V_DrawPatch(mi->x, mi->y, mi->p[*mi->inum]);
		mi->oldinum = *mi->inum;
	}
}



void
STlib_initBinIcon
( st_binicon_t*		b,
  int			x,
  int			y,
  patch_t*		i,
  doombool*		val,
  doombool*		on )
{
    b->x	= x;
    b->y	= y;
    b->oldval	= false;
    b->val	= val;
    b->on	= on;
    b->p	= i;
}



void
STlib_updateBinIcon
( st_binicon_t*		bi,
  doombool		refresh )
{
    int			x;
    int			y;
    int			w;
    int			h;

    if (*bi->on
     && (bi->oldval != *bi->val || refresh))
    {
		x = bi->x - SHORT(bi->p->leftoffset);
		y = bi->y - SHORT(bi->p->topoffset);
		w = SHORT(bi->p->width);
		h = SHORT(bi->p->height);

		if (y - ST_Y < 0)
			I_Error("updateBinIcon: y - ST_Y < 0");

		if (*bi->val)
			V_DrawPatch(bi->x, bi->y, bi->p);

		bi->oldval = *bi->val;
    }

}

#if ALLOW_SBARDEF
#include "m_jsonlump.h"

jsonlumpresult_t STlib_ParseNumberFont( const JSONElement& elem, sbarnumfont_t& output )
{
	const JSONElement& name		= elem[ "name" ];
	const JSONElement& type		= elem[ "type" ];
	const JSONElement& stem		= elem[ "stem" ];

	if( !name.IsString()
		|| !type.IsNumber()
		|| !stem.IsString()
		)
	{
		return jl_parseerror;
	}

	output.name = M_DuplicateStringToZone( to< std::string >( name ).c_str(), PU_STATIC, nullptr );
	output.type = to< sbarnumfonttype_t >( type );

	char basebuffer[16] = {};
	char buffer[16] = {};
	std::string stemstr = to< std::string >( stem );
	lumpindex_t found = -1;

	int32_t maxwidth = 0;

	for( int32_t num : iota( 0, 10 ) )
	{
		snprintf( basebuffer, 16, "%sNUM%%d", stemstr.c_str() );
		DEH_snprintf( buffer, 16, basebuffer, num );
		found = W_CheckNumForName( buffer );
		if( found < 0 ) return jl_parseerror;
		output.numbers[ num ] = (patch_t*)W_CacheLumpNum( found, PU_STATIC );
		maxwidth = M_MAX( maxwidth, output.numbers[ num ]->width );
	}

	snprintf( buffer, 16, "%sMINUS", stemstr.c_str() );
	found = W_CheckNumForName( DEH_String( buffer ) );
	if( found >= 0 )
	{
		output.minus = (patch_t*)W_CacheLumpNum( found, PU_STATIC );
		maxwidth = M_MAX( maxwidth, output.minus->width );
	}
	
	snprintf( buffer, 16, "%sPRCNT", stemstr.c_str() );
	found = W_CheckNumForName( DEH_String( buffer ) );
	if( found >= 0 )
	{
		output.percent = (patch_t*)W_CacheLumpNum( found, PU_STATIC );
		maxwidth = M_MAX( maxwidth, output.percent->width );
	}

	switch( output.type )
	{
	case sbf_mono0:
		output.monowidth = output.numbers[ 0 ]->width;
		break;

	case sbf_monomax:
		output.monowidth = maxwidth;
		break;

	default:
		break;
	}
	
	return jl_success;
}

jsonlumpresult_t STlib_ParseNumberFonts( const JSONElement& elem, statusbars_t& output )
{
	if( elem.Children().size() == 0 )
	{
		return jl_parseerror;
	}

	output.fonts = (sbarnumfont_t*)Z_MallocZero( sizeof( sbarnumfont_t ) * elem.Children().size(), PU_STATIC, nullptr );
	output.numfonts = elem.Children().size();

	for( int32_t index : iota( 0, (int32_t)output.numfonts ) )
	{
		jsonlumpresult_t result = STlib_ParseNumberFont( elem.Children()[ index ], output.fonts[ index ] );
		if( result != jl_success ) return result;
	}

	return jl_success;
}

static const std::map< std::string, sbarelementtype_t > elementtypes =
{
	{ "canvas",			sbe_canvas },
	{ "graphic",		sbe_graphic },
	{ "animation",		sbe_animation },
	{ "face",			sbe_face },
	{ "facebackground",	sbe_facebackground },
	{ "number",			sbe_number },
	{ "percent",		sbe_percentage },
};

jsonlumpresult_t STlib_ParseStatusBarFrame( const JSONElement& elem, sbaranimframe_t& output )
{
	const JSONElement& lump			= elem[ "lump" ];
	const JSONElement& duration		= elem[ "duration" ];

	if( !lump.IsString()
		|| !duration.IsNumber() )
	{
		return jl_parseerror;
	}

	output.lump = M_DuplicateStringToZone( ToUpper( to< std::string >( lump ) ).c_str(), PU_STATIC, nullptr );
	output.tics = (int32_t)( to< double_t >( duration ) * TICRATE );

	return jl_success;
}

jsonlumpresult_t STlib_ParseStatusBarCondition( const JSONElement& elem, sbarcondition_t& output )
{
	const JSONElement& condition	= elem[ "condition" ];
	const JSONElement& param		= elem[ "param" ];

	if( !condition.IsNumber()
		|| !param.IsNumber() )
	{
		return jl_parseerror;
	}

	output.condition = to< sbarconditiontype_t >( condition );
	output.param = to< int32_t >( param );

	return jl_success;
}

jsonlumpresult_t STlib_ParseStatusBarElement( const JSONElement& rootelem, sbarelement_t& output )
{
	if( !rootelem.IsElement()
		|| rootelem.Children().size() > 1
		|| !rootelem.Children()[ 0 ].IsElement() )
	{
		return jl_parseerror;
	}

	const JSONElement& elem = rootelem.Children()[ 0 ];
	const std::string& type = elem.Key();
	auto foundtype = elementtypes.find( type );
	if( foundtype == elementtypes.end() )
	{
		return jl_parseerror;
	}

	output.type = foundtype->second;

	const JSONElement& xpos				= elem[ "x" ];
	const JSONElement& ypos				= elem[ "y" ];
	const JSONElement& alignment		= elem[ "alignment" ];
	const JSONElement& conditions		= elem[ "conditions" ];
	const JSONElement& children			= elem[ "children" ];
	const JSONElement& patch			= elem[ "patch" ];
	const JSONElement& tranmap			= elem[ "tranmap" ];
	const JSONElement& translation		= elem[ "translation" ];
	const JSONElement& frames			= elem[ "frames" ];
	const JSONElement& numfont			= elem[ "font" ];
	const JSONElement& numtype			= elem[ "type" ];
	const JSONElement& numparam			= elem[ "param" ];
	const JSONElement& numlength		= elem[ "maxlength" ];

	if( !xpos.IsNumber()
		|| !ypos.IsNumber()
		|| !alignment.IsNumber()
		|| !( tranmap.IsNull() || tranmap.IsString() )
		|| !( translation.IsNull() || translation.IsString() )
		|| !( conditions.IsNull() || conditions.IsArray() )
		|| !( children.IsNull() || children.IsArray() )
		)
	{
		return jl_parseerror;
	}

	if( output.type == sbe_graphic
		&& !patch.IsString() )
	{
		return jl_parseerror;
	}

	if( output.type == sbe_animation
		&& !( frames.IsArray()
			|| !frames.Children().empty() )
		)
	{
		return jl_parseerror;
	}

	if( ( output.type == sbe_number || output.type == sbe_percentage )
		&& ( !numfont.IsString()
			|| !numtype.IsNumber()
			|| !numparam.IsNumber()
			|| !numlength.IsNumber() )
		)
	{
		return jl_parseerror;
	}

	output.xpos = to< int32_t >( xpos );
	output.ypos = to< int32_t >( ypos );
	output.alignment = to< sbaralignment_t >( alignment );

	switch( output.type )
	{
	case sbe_graphic:
		output.patch = M_DuplicateStringToZone( DEH_String( ToUpper( to< std::string >( patch ) ).c_str() ), PU_STATIC, nullptr );
		output.tranmap = tranmap.IsString()		? M_DuplicateStringToZone( ToUpper( to< std::string >( tranmap ) ).c_str(), PU_STATIC, nullptr )
												: nullptr;
		output.translation = translation.IsString()	? M_DuplicateStringToZone( ToUpper( to< std::string >( translation ) ).c_str(), PU_STATIC, nullptr )
												: nullptr;
		break;

	case sbe_animation:
		output.numframes = (int32_t)frames.Children().size();
		output.frames = (sbaranimframe_t*)Z_MallocZero( sizeof( sbaranimframe_t ) * output.numframes, PU_STATIC, nullptr );
		for( int32_t animindex : iota( 0, (int32_t)output.numframes ) )
		{
			jsonlumpresult_t result = STlib_ParseStatusBarFrame( elem.Children()[ animindex ], output.frames[ animindex ] );
			if( result != jl_success ) return result;
		}
		break;

	case sbe_number:
	case sbe_percentage:
		output.numfont = M_DuplicateStringToZone( ToUpper( to< std::string >( numfont ) ).c_str(), PU_STATIC, nullptr );
		output.numtype = to< sbarnumbertype_t >( numtype );
		output.numparam = to< int32_t >( numparam );
		output.numlength = to< int32_t >( numlength );
		break;
	}

	if( conditions.IsArray() )
	{
		output.conditions = (sbarcondition_t*)Z_MallocZero( sizeof( sbarcondition_t ) * conditions.Children().size(), PU_STATIC, nullptr );
		output.numconditions = conditions.Children().size();

		for( int32_t index : iota( 0, (int32_t)output.numconditions ) )
		{
			jsonlumpresult_t result = STlib_ParseStatusBarCondition( conditions.Children()[ index ], output.conditions[ index ] );
			if( result != jl_success ) return result;
		}
	}

	if( children.IsArray() )
	{
		output.children = (sbarelement_t*)Z_MallocZero( sizeof( sbarelement_t ) * children.Children().size(), PU_STATIC, nullptr );
		output.numchildren = children.Children().size();

		for( int32_t index : iota( 0, (int32_t)output.numchildren ) )
		{
			jsonlumpresult_t result = STlib_ParseStatusBarElement( children.Children()[ index ], output.children[ index ] );
			if( result != jl_success ) return result;
		}
	}

	return jl_success;
}


jsonlumpresult_t STlib_ParseStatusBar( const JSONElement& elem, statusbarbase_t& output )
{
	const JSONElement& height				= elem[ "height" ];
	const JSONElement& fullscreenrender		= elem[ "fullscreenrender" ];
	const JSONElement& fillflat				= elem[ "fillflat" ];
	const JSONElement& children				= elem[ "children" ];

	if( !height.IsNumber()
		|| !fullscreenrender.IsBoolean()
		|| !( fillflat.IsString() || fillflat.IsNull() )
		|| !( children.IsArray() || children.IsNull() ) )
	{
		return jl_parseerror;
	}

	output.height = to< int32_t >( height );
	output.fullscreen = to< bool >( fullscreenrender );
	output.fillflat = fillflat.IsString()	? M_DuplicateStringToZone( ToUpper( to< std::string >( fillflat ) ).c_str(), PU_STATIC, nullptr )
											: nullptr;

	output.children = children.IsArray()	? (sbarelement_t*)Z_MallocZero( sizeof( sbarelement_t ) * children.Children().size(), PU_STATIC, nullptr )
											: nullptr;
	output.numchildren = children.IsArray()	? children.Children().size()
											: 0;

	for( int32_t index : iota( 0, (int32_t)output.numchildren ) )
	{
		jsonlumpresult_t result = STlib_ParseStatusBarElement( children.Children()[ index ], output.children[ index ] );
		if( result != jl_success ) return result;
	}

	return jl_success;
}

jsonlumpresult_t STlib_ParseStatusbars( const JSONElement& elem, statusbars_t& output )
{
	if( elem.Children().size() == 0 )
	{
		return jl_parseerror;
	}

	output.statusbars = (statusbarbase_t*)Z_MallocZero( sizeof( statusbarbase_t ) * elem.Children().size(), PU_STATIC, nullptr );
	output.numstatusbars = elem.Children().size();

	for( int32_t index : iota( 0, (int32_t)output.numstatusbars ) )
	{
		jsonlumpresult_t result = STlib_ParseStatusBar( elem.Children()[ index ], output.statusbars[ index ] );
		if( result != jl_success ) return result;
	}

	return jl_success;
}

statusbars_t* STlib_LoadSBARDEF()
{
	statusbars_t storage = {};

	auto ParseSBARDEF = [&storage]( const JSONElement& elem, const JSONLumpVersion& version ) -> jsonlumpresult_t
	{
		const JSONElement& numberfonts	= elem[ "numberfonts" ];
		const JSONElement& statusbars	= elem[ "statusbars" ];

		if( !numberfonts.IsArray()
			|| !statusbars.IsArray() )
		{
			return jl_parseerror;
		}

		jsonlumpresult_t result = STlib_ParseNumberFonts( numberfonts, storage );
		if( result != jl_success ) return result;

		result = STlib_ParseStatusbars( statusbars, storage );
		return result;
	};

	if( M_ParseJSONLump( "SBARDEF", "statusbar", { 1, 0, 0 }, ParseSBARDEF ) == jl_success )
	{
		statusbars_t* output = Z_MallocAs( statusbars_t, PU_STATIC, nullptr );
		*output = storage;
		return output;
	}

	return nullptr;
}

#endif // ALLOW_SBARDEF


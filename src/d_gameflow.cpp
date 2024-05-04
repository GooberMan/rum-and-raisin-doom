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
// DESCRIPTION: Manages the current game

#include "d_gameflow.h"

#include "i_terminal.h"

#include "m_container.h"
#include "m_jsonlump.h"

#include "w_wad.h"

gameflow_t*		current_game		= nullptr;
episodeinfo_t*	current_episode		= nullptr;
mapinfo_t*		current_map			= nullptr;

static std::unordered_map< std::string, mapinfo_t* > allmaps;
static std::unordered_map< std::string, interlevel_t* > interlevelstorage;

void D_GameflowParseUMAPINFO( int32_t lumpnum );
void D_GameflowParseDMAPINFO( int32_t lumpnum );

episodeinfo_t* D_GameflowGetEpisode( int32_t episodenum )
{
	for( auto& episode : D_GetEpisodes() )
	{
		if( episode->episode_num == episodenum )
		{
			return episode;
		}
	}

	return nullptr;
}

mapinfo_t* D_GameflowGetMap( episodeinfo_t* episode, int32_t mapnum )
{
	if( episode != nullptr )
	{
		for( auto& map : D_GetMapsFor( episode ) )
		{
			if( map->map_num == mapnum )
			{
				return map;
			}
		}
	}

	return nullptr;
}

void D_GameflowSetCurrentEpisode( episodeinfo_t* episode )
{
	current_episode = episode;
	current_map = episode->first_map;
}

void D_GameflowSetCurrentMap( mapinfo_t* map )
{
	current_episode = map->episode;
	current_map = map;
}

constexpr auto PlainFlowString( const char* name )
{
	flowstring_t lump = { name, FlowString_None };
	return lump;
}

INLINE auto CopyToPlainFlowString( const char* name )
{
	size_t len = strlen( name ) + 1;
	char* copy = (char*)Z_MallocZero( len, PU_STATIC, nullptr );
	M_StringCopy( copy, name, len );

	return PlainFlowString( copy );
}

INLINE auto CopyToPlainFlowString( const std::string& name )
{
	size_t len = name.size() + 1;
	char* copy = (char*)Z_MallocZero( len, PU_STATIC, nullptr );
	M_StringCopy( copy, name.c_str(), len );

	return PlainFlowString( copy );
}

static void Sanitize( std::string& currline )
{
	std::replace( currline.begin(), currline.end(), '\t', ' ' );
	currline.erase( std::remove( currline.begin(), currline.end(), '\r' ), currline.end() );
	size_t startpos = currline.find_first_not_of( " " );
	if( startpos != std::string::npos && startpos > 0 )
	{
		currline = currline.substr( startpos );
	}
}

template< typename _ty >
jsonlumpresult_t D_GameflowParseInterlevelArray( const JSONElement& array, _ty*& output, int32_t& outputcount
												, std::function< jsonlumpresult_t( const JSONElement&, _ty& ) >&& parse )
{
	if( !array.IsArray() )
	{
		return jl_parseerror;
	}

	outputcount = (int32_t)array.Children().size();

	output = (_ty*)Z_MallocZero( sizeof( _ty ) * outputcount, PU_STATIC, nullptr );

	for( int32_t curr : iota( 0, outputcount ) )
	{
		jsonlumpresult_t res = parse( array.Children()[ curr ], output[ curr ] );
		if( res != jl_success ) return res;
	}

	return jl_success;
}

jsonlumpresult_t D_GameflowParseInterlevelFrame( const JSONElement& frame, interlevelframe_t& output )
{
	const JSONElement& image = frame[ "image" ];
	const JSONElement& type = frame[ "type" ];
	const JSONElement& duration = frame[ "duration" ];
	const JSONElement& maxduration = frame[ "maxduration" ];

	if( !image.IsString()
		|| !type.IsNumber()
		|| !duration.IsNumber()
		|| !maxduration.IsNumber() )
	{
		return jl_parseerror;
	}

	output.image_lump = CopyToPlainFlowString( to< std::string >( image ) );
	output.type = to< frametype_t >( type );
	output.duration = to< int32_t >( duration );
	output.maxduration = to< int32_t >( maxduration );
	output.lumpname_animindex = 0;
	output.lumpname_animframe = 0;
}

jsonlumpresult_t D_GameflowParseInterlevelCondition( const JSONElement& condition, interlevelcond_t& output )
{
	const JSONElement& animcondition = condition[ "condition" ];
	const JSONElement& param = condition[ "param" ];

	if( !animcondition.IsNumber()
		|| !param.IsNumber() )
	{
		return jl_parseerror;
	}

	output.condition = to< animcondition_t >( animcondition );
	output.param = to< int32_t >( param );
	return jl_success;
}

jsonlumpresult_t D_GameflowParseInterlevelAnim( const JSONElement& anim, interlevelanim_t& output )
{
	const JSONElement& xpos = anim[ "x" ];
	const JSONElement& ypos = anim[ "y" ];
	const JSONElement& frames = anim[ "frames" ];
	const JSONElement& conditions = anim[ "conditions" ];

	if( !xpos.IsNumber()
		|| !ypos.IsNumber()
		|| !( conditions.IsArray() || conditions.IsNull() )
		|| !frames.IsArray() )
	{
		return jl_parseerror;
	}

	output.x_pos = to< int32_t >( xpos );
	output.y_pos = to< int32_t >( ypos );
	jsonlumpresult_t res = D_GameflowParseInterlevelArray< interlevelframe_t >( frames, output.frames, output.num_frames, D_GameflowParseInterlevelFrame );
	if( res != jl_success ) return res;
	if( conditions.IsNull() )
	{
		output.conditions = nullptr;
		output.num_conditions = 0;
	}
	else
	{
		res = D_GameflowParseInterlevelArray< interlevelcond_t >( conditions, output.conditions, output.num_conditions, D_GameflowParseInterlevelCondition );
		if( res != jl_success ) return res;
	}

	return jl_success;
}

interlevel_t* D_GameflowGetInterlevel( const char* lumpname )
{
	auto found = interlevelstorage.find( lumpname );
	if( found != interlevelstorage.end() )
	{
		return found->second;
	}

	interlevel_t* output = nullptr;
	auto ParseInterlevel = [&output]( const JSONElement& elem, const JSONLumpVersion& version ) -> jsonlumpresult_t
	{
		const JSONElement& type = elem[ "type" ];
		const JSONElement& music = elem[ "music" ];
		const JSONElement& backgroundimage = elem[ "backgroundimage" ];
		const JSONElement& backgroundanims = elem[ "backgroundanims" ];
		const JSONElement& foregroundanims = elem[ "foregroundanims" ];

		if( !type.IsNumber()
			|| !music.IsString()
			|| !backgroundimage.IsString() )
		{
			return jl_parseerror;
		}

		output = (interlevel_t*)Z_MallocZero( sizeof( interlevel_t ), PU_STATIC, nullptr );
		output->type = to< interleveltype_t >( type );
		output->music_lump = CopyToPlainFlowString( to< std::string >( music ) );
		output->background_lump = CopyToPlainFlowString( to< std::string >( backgroundimage ) );
		jsonlumpresult_t res = jl_parseerror;
		if( !backgroundanims.IsNull() )
		{
			res = D_GameflowParseInterlevelArray< interlevelanim_t >( backgroundanims, output->background_anims, output->num_background_anims, D_GameflowParseInterlevelAnim );
			if( res != jl_success ) return res;
		}
		if( !foregroundanims.IsNull() )
		{
			res = D_GameflowParseInterlevelArray< interlevelanim_t >( foregroundanims, output->foreground_anims, output->num_foreground_anims, D_GameflowParseInterlevelAnim );
			if( res != jl_success ) return res;
		}

		return jl_success;
	};

	if( M_ParseJSONLump( lumpname, "interlevel", { 1, 0, 0 }, ParseInterlevel ) == jl_success )
	{
		interlevelstorage[ lumpname ] = output;
		return output;
	}

	return nullptr;
}

void D_GameflowParseMUSINFO( lumpindex_t lumpnum )
{
	std::string lumptext( (size_t)W_LumpLength( lumpnum ) + 1, 0 );
	W_ReadLump( lumpnum, (void*)lumptext.c_str() );

	std::stringstream lumpstream( lumptext );
	std::string currline;

	mapinfo_t* currmap = nullptr;

	while( std::getline( lumpstream, currline ) )
	{
		Sanitize( currline );
		auto found = allmaps.find( currline );
		if( found != allmaps.end() )
		{
			currmap = found->second;
		}
		else if( currmap )
		{
			int32_t index = -1;
			char lumpname[ 16 ] = {};
			if( sscanf( currline.c_str(), "%d %8s", &index, lumpname ) == 2
				&& index > 0
				&& index <= 64 )
			{
				currmap->music_lump[ index ] = CopyToPlainFlowString( lumpname );
			}
		}
	}
}

void D_GameflowCheckAndParseMapinfos( void )
{
	lumpindex_t lumpnum = -1;
	if( ( lumpnum = W_CheckNumForName( "UMAPINFO" ) ) >= 0 )
	{
		D_GameflowParseUMAPINFO( lumpnum );
		I_TerminalPrintf( Log_Startup, " UMAPINFO gameflow defined\n" );
	}
	else if( ( lumpnum = W_CheckNumForName( "DMAPINFO" ) ) >= 0 )
	{
		D_GameflowParseDMAPINFO( lumpnum );
		I_TerminalPrintf( Log_Startup, " DMAPINFO gameflow defined\n" );
	}
	else if( ( lumpnum = W_CheckNumForName( "ZMAPINFO" ) ) >= 0 )
	{
		//D_GameflowParseZMAPINFO( lumpnum );
		I_TerminalPrintf( Log_Error, " ZMAPINFO gameflow unimplemented, retaining vanilla gameflow\n" );
	}
	else if( ( lumpnum = W_CheckNumForName( "MAPINFO" ) ) >= 0 )
	{
		//D_GameflowParseMAPINFO( lumpnum );
		I_TerminalPrintf( Log_Error, " MAPINFO gameflow unimplemented, retaining vanilla gameflow\n" );
	}

	for( episodeinfo_t* episode : D_GetEpisodes() )
	{
		for( mapinfo_t* map : D_GetMapsFor( episode ) )
		{
			std::string maplump = AsStdString( map->data_lump, map->episode->episode_num, map->map_num );
			allmaps[ maplump ] = map;
		}
	}

	if( ( lumpnum = W_CheckNumForName( "MUSINFO" ) ) >= 0 )
	{
		D_GameflowParseMUSINFO( lumpnum );
	}
}

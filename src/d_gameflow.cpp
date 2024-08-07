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
#include "i_timer.h"

#include "m_container.h"
#include "m_jsonlump.h"

#include "w_wad.h"

gameflow_t*		current_game		= nullptr;
episodeinfo_t*	current_episode		= nullptr;
mapinfo_t*		current_map			= nullptr;

static std::unordered_map< std::string, mapinfo_t* >		allmaps;
static std::unordered_map< std::string, interlevel_t* >		interlevelstorage;
static std::unordered_map< std::string, endgame_t* >		endgamestorage;

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
	return PlainFlowString( M_DuplicateStringToZone( name, PU_STATIC, nullptr ) );
}

INLINE auto CopyToPlainFlowString( const std::string& name )
{
	return PlainFlowString( M_DuplicateStringToZone( name.c_str(), PU_STATIC, nullptr ) );
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

enum endgameloadedtype_t : int32_t
{
	eg_invalid = -1,
	eg_art,
	eg_bunny,
	eg_castrollcall
};

constexpr endgametype_t GetEndgameType( endgameloadedtype_t type )
{
	switch( type )
	{
	case eg_art:
		return (endgametype_t)( EndGame_Pic | EndGame_LoopingMusic );
	case eg_bunny:
		return EndGame_Bunny;
	case eg_castrollcall:
		return (endgametype_t)( EndGame_Cast | EndGame_LoopingMusic );

	default:
		break;
	}

	return EndGame_None;
}

jsonlumpresult_t D_GameflowParseCastFrames( const JSONElement& elem, endgame_castframe_t*& frames, int32_t& numframes )
{
	frames = (endgame_castframe_t*)Z_MallocZero( sizeof( endgame_castframe_t ) * elem.Children().size(), PU_STATIC, nullptr );
	numframes = (int32_t)elem.Children().size();

	endgame_castframe_t* outframe = frames;
	for( const JSONElement& inframe : elem.Children() )
	{
		if( !inframe.IsElement() )
		{
			return jl_parseerror;
		}

		const JSONElement& lump			= inframe[ "lump" ];
		const JSONElement& tranmap		= inframe[ "tranmap" ];
		const JSONElement& translation	= inframe[ "translation" ];
		const JSONElement& flipped		= inframe[ "flipped" ];
		const JSONElement& duration		= inframe[ "duration" ];
		const JSONElement& sound		= inframe[ "sound" ];

		if( !lump.IsString()
			|| !( tranmap.IsNull() || tranmap.IsString() )
			|| !( translation.IsNull() || translation.IsString() )
			|| !flipped.IsBoolean()
			|| !duration.IsNumber()
			|| !sound.IsNumber()
			)
		{
			return jl_parseerror;
		}

		constexpr flowstring_t emptystring = {};

		outframe->lump = CopyToPlainFlowString( to< std::string >( lump ) );
		outframe->tranmap = tranmap.IsNull() ? emptystring : CopyToPlainFlowString( to< std::string >( tranmap ) );
		outframe->translation = translation.IsNull() ? emptystring : CopyToPlainFlowString( to< std::string >( translation ) );
		outframe->flipped = to< bool >( flipped );
		outframe->duration = M_MAX( 1, (int32_t)( to< double_t >( duration ) * TICRATE ) );
		outframe->sound = to< int32_t >( sound );

		++outframe;
	}

	return jl_success;
}

// We have to assume that anyone getting this finale can and might
// set an intermission on it. So we always return a copy.
endgame_t* D_GameflowGetFinaleCopy( const char* lumpname )
{
	auto found = endgamestorage.find( lumpname );
	if( found == endgamestorage.end() )
	{
		endgame_t output = {};
		auto ParseFinale = [ &output ]( const JSONElement& elem, const JSONLumpVersion& version ) -> jsonlumpresult_t
		{
			const JSONElement& type			= elem[ "type" ];
			const JSONElement& music		= elem[ "music" ];
			const JSONElement& musicloops	= elem[ "musicloops" ];
			const JSONElement& background	= elem[ "background" ];
			const JSONElement& donextmap	= elem[ "donextmap" ];
			const JSONElement& bunny		= elem[ "bunny" ];
			const JSONElement& castrollcall	= elem[ "castrollcall" ];

			if( !type.IsNumber()
				|| !music.IsString()
				|| !musicloops.IsBoolean()
				|| !background.IsString()
				|| !donextmap.IsBoolean()
				)
			{
				return jl_parseerror;
			}

			endgametype_t endgametype = GetEndgameType( to< endgameloadedtype_t >( type ) );

			if( endgametype == EndGame_None
				|| ( ( endgametype & EndGame_Pic ) == EndGame_Pic		&& !( bunny.IsNull() && castrollcall.IsNull() ) )
				|| ( ( endgametype & EndGame_Bunny ) == EndGame_Bunny	&& !( bunny.IsElement() && castrollcall.IsNull() ) )
				|| ( ( endgametype & EndGame_Cast ) == EndGame_Cast		&& !( bunny.IsNull() && castrollcall.IsElement() ) )
				)
			{
				return jl_parseerror;
			}

			if( to< bool >( musicloops ) ) endgametype = (endgametype_t)( endgametype | EndGame_LoopingMusic );
			if( to< bool >( donextmap ) ) endgametype = (endgametype_t)( endgametype | EndGame_DoNextMap );

			output.type = endgametype;
			output.intermission = nullptr;
			output.music_lump = CopyToPlainFlowString( to< std::string >( music ) );
			output.primary_image_lump = CopyToPlainFlowString( to< std::string >( background ) );
			output.cast_members = nullptr;
			output.num_cast_members = 0;

			if( ( endgametype & EndGame_Bunny ) == EndGame_Bunny )
			{
				const JSONElement& stitchimage		= bunny[ "stitchimage" ];
				const JSONElement& overlay			= bunny[ "overlay" ];
				const JSONElement& overlaycount		= bunny[ "overlaycount" ];
				const JSONElement& overlaysound		= bunny[ "overlaysound" ];
				const JSONElement& overlayx			= bunny[ "overlayx" ];
				const JSONElement& overlayy			= bunny[ "overlayy" ];
				if( !stitchimage.IsString()
					|| !( overlay.IsNull() || overlay.IsString() )
					|| !overlaycount.IsNumber()
					|| !overlaysound.IsNumber()
					|| !overlayx.IsNumber()
					|| !overlayy.IsNumber() )
				{
					return jl_parseerror;
				}

				output.secondary_image_lump = CopyToPlainFlowString( to< std::string >( stitchimage ) );
				if( !overlay.IsNull() ) output.bunny_end_overlay = CopyToPlainFlowString( to< std::string >( overlay ) );
				output.bunny_end_count = to< int32_t >( overlaycount );
				output.bunny_end_sound = to< int32_t >( overlaysound );
				output.bunny_end_x = to< int32_t >( overlayx );
				output.bunny_end_y = to< int32_t >( overlayy );
			}
			else if(  ( endgametype & EndGame_Cast ) == EndGame_Cast )
			{
				const JSONElement& castmembers = castrollcall[ "castanims" ];
				if( !castmembers.IsArray() )
				{
					return jl_parseerror;
				}

				output.cast_members = (endgame_castmember_t*)Z_MallocZero( sizeof( endgame_castmember_t ) * castmembers.Children().size(), PU_STATIC, nullptr );
				output.num_cast_members = (int32_t)castmembers.Children().size();

				endgame_castmember_t* currmember = output.cast_members;
				for( const JSONElement& member : castmembers.Children() )
				{
					if( !member.IsElement() )
					{
						return jl_parseerror;
					}

					const JSONElement& name			= member[ "name" ];
					const JSONElement& aliveframes	= member[ "aliveframes" ];
					const JSONElement& deathframes	= member[ "deathframes" ];

					if( !name.IsString()
						|| !aliveframes.IsArray()
						|| aliveframes.Children().size() == 0
						|| !deathframes.IsArray()
						|| deathframes.Children().size() == 0 )
					{
						return jl_parseerror;
					}

					currmember->name_mnemonic = CopyToPlainFlowString( to< std::string >( name ) );

					jsonlumpresult_t result = D_GameflowParseCastFrames( aliveframes, currmember->alive_frames, currmember->num_alive_frames );
					if( result != jl_success ) return result;
					result = D_GameflowParseCastFrames( deathframes, currmember->death_frames, currmember->num_death_frames );
					if( result != jl_success ) return result;

					++currmember;
				}
			}

			return jl_success;
		};

		jsonlumpresult_t result = M_ParseJSONLump( lumpname, "finale", { 1, 0, 0 }, ParseFinale );
		if( result == jl_success )
		{
			endgame_t* newendgame = (endgame_t*)Z_Malloc( sizeof( endgame_t ), PU_STATIC, nullptr );
			*newendgame = output;
			endgamestorage[ lumpname ] = newendgame;
			found = endgamestorage.find( lumpname );
		}
	}

	if( found != endgamestorage.end() )
	{
		endgame_t* output = (endgame_t*)Z_Malloc( sizeof( endgame_t ), PU_STATIC, nullptr );
		*output = *found->second;
		return output;
	}

	return nullptr;
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
	output.duration = (int32_t)( to< double_t >( duration ) * TICRATE );
	output.maxduration = (int32_t)( to< double_t >( maxduration ) * TICRATE );
	output.lumpname_animindex = 0;
	output.lumpname_animframe = 0;

	if( output.type != 0 && ( output.type & ~Frame_Valid ) != 0 )
	{
		return jl_parseerror;
	}

	return jl_success;
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

	if( output.condition < AnimCondition_None || output.condition >= AnimCondition_Max )
	{
		return jl_parseerror;
	}

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

jsonlumpresult_t D_GameflowParseInterlevelLayer( const JSONElement& anim, interlevellayer_t& output )
{
	const JSONElement& anims = anim[ "anims" ];
	const JSONElement& conditions = anim[ "conditions" ];

	jsonlumpresult_t res = D_GameflowParseInterlevelArray< interlevelanim_t >( anims, output.anims, output.num_anims, D_GameflowParseInterlevelAnim );
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
		const JSONElement& music = elem[ "music" ];
		const JSONElement& backgroundimage = elem[ "backgroundimage" ];
		const JSONElement& layers = elem[ "layers" ];

		if( !music.IsString()
			|| !backgroundimage.IsString() )
		{
			return jl_parseerror;
		}

		output = (interlevel_t*)Z_MallocZero( sizeof( interlevel_t ), PU_STATIC, nullptr );
		output->music_lump = CopyToPlainFlowString( to< std::string >( music ) );
		output->background_lump = CopyToPlainFlowString( to< std::string >( backgroundimage ) );
		jsonlumpresult_t res = jl_success;
		if( !layers.IsNull() )
		{
			res = D_GameflowParseInterlevelArray< interlevellayer_t >( layers, output->anim_layers, output->num_anim_layers, D_GameflowParseInterlevelLayer );
		}

		return res;
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

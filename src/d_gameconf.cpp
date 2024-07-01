//
// Copyright(C) 2020-2024 Ethan Watson
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

#include "d_gameconf.h"

#include "d_iwad.h"

#include "i_error.h"

#include "m_argv.h"
#include "m_container.h"
#include "m_jsonlump.h"
#include "m_misc.h"

#include "w_wad.h"

extern "C"
{
	gameconf_t*		gameconf;
}

static std::map< std::string, GameVersion_t > executableversions =
{
	{ "doom1.9",		exe_doom_1_9				},
	{ "limitremoving",	exe_limit_removing			},
	{ "bugfixed",		exe_limit_removing_fixed	},
	{ "boom2.02",		exe_boom_2_02				},
	{ "complevel9",		exe_complevel9				},
	{ "mbf",			exe_mbf						},
	{ "mbfextra",		exe_mbf_dehextra			},
	{ "mbf21",			exe_mbf21					},
	{ "mbf21ex",		exe_mbf21_extended			},
	{ "rnr24",			exe_rnr24					},
};

static const std::map< std::string, GameMode_t > modeversions =
{
	{ "registered",		registered						},
	{ "retail",			retail							},
	{ "commercial",		commercial						},
};

DOOM_C_API gameconf_t* D_BuildGameConf()
{
	struct info_t
	{;
		std::string title;
		std::string author;
		std::string description;
		std::string version;
		std::string iwadfile;
		std::vector< std::string > pwadfiles;
		std::vector< std::string > dehfiles;
		std::string options;
		GameVersion_t executable;
		GameMode_t mode;
		GameMission_t mission;
	} info;

	auto ValidateFile = []( const char* file ) -> std::string
	{
		const char* foundfile = D_TryFindWADByName( file );
		if( !foundfile )
		{
			I_Error( "D_BuildGameConf: %s not found.", file );
		}
		std::string output = foundfile;
		free( (void*)foundfile );
		return output;
	};

	auto CheckGameconf = [ &info, &ValidateFile ]( const JSONElement& elem, const JSONLumpVersion& version ) -> jsonlumpresult_t
	{
		const JSONElement& title = elem[ "title" ];
		const JSONElement& author = elem[ "author" ];
		const JSONElement& description = elem[ "description" ];
		const JSONElement& wadversion = elem[ "version" ];
		const JSONElement& iwadfile = elem[ "iwad" ];
		const JSONElement& pwadfiles = elem[ "pwads" ];
		const JSONElement& executable = elem[ "executable" ];
		const JSONElement& mode = elem[ "mode" ];
		const JSONElement& options = elem[ "options" ];

		if( title.IsString() )
		{
			info.title = to< std::string >( title );
		}

		if( author.IsString() )
		{
			info.author = to< std::string >( author );
		}

		if( description.IsString() )
		{
			info.description = to< std::string >( description );
		}

		if( wadversion.IsString() )
		{
			info.version = to< std::string >( wadversion );
		}

		if( iwadfile.IsString() )
		{
			info.iwadfile = D_FindWADByName( to< std::string >( iwadfile ).c_str() );
			info.mission = D_IdentifyIWADByName( info.iwadfile.c_str(), IWAD_MASK_DOOM );
		}

		if( pwadfiles.Valid() )
		{
			if( pwadfiles.IsArray() )
			{
				for( const JSONElement& file : pwadfiles.Children() )
				{
					info.pwadfiles.push_back( ValidateFile( to< std::string >( file ).c_str() ) );
				}
			}
			else if( !pwadfiles.IsNull() )
			{
				I_Error( "D_BuildGameConf: pwadfiles is not an array." );
				return jl_parseerror;
			}
		}

		if( executable.IsString() )
		{
			auto found = executableversions.find( to< std::string >( executable ) );
			if( found != executableversions.end() )
			{
				info.executable = M_MAX( info.executable, found->second );
			}
			else
			{
				I_Error( "D_BuildGameConf: executable '%s' is not valid.", executable );
				return jl_parseerror;
			}
		}
		else if( executable.IsNull() )
		{
			info.executable = M_MAX( info.executable, exe_invalid );
		}
		else
		{
			I_Error( "D_BuildGameConf: executable field is not valid.", executable );
		}

		if( mode.IsString() )
		{
			auto found = modeversions.find( to< std::string >( mode ) );
			if( found != modeversions.end() )
			{
				info.mode = M_MAX( info.mode, found->second );
			}
			else
			{
				I_Error( "D_BuildGameConf: mode '%s' is not valid.", to< std::string >( mode ).c_str() );
				return jl_parseerror;
			}
		}

		if( options.IsString() )
		{
			if( !info.options.empty() ) info.options += "\n";
			info.options += to< std::string >( options );
		}


		return jl_success;
	};

	auto CheckWad = [ &CheckGameconf ]( const std::string& wad ) -> bool
	{
		wad_file_t* wadfile = W_AddFile( wad.c_str() );
		jsonlumpresult_t result = M_ParseJSONLump( "GAMECONF", "gameconf", { 1, 0, 0 }, CheckGameconf );
		W_RemoveFile( wadfile );
		return result == jl_success || result == jl_notfound;
	};

	int32_t dehparm = M_CheckParmWithArgs( "-deh", 1 );
	if( dehparm > 0 )
	{
		while ( ++dehparm != myargc && myargv[ dehparm ][ 0 ] != '-' )
		{
			std::string dehpath = ValidateFile( myargv[ dehparm ] );
			info.dehfiles.push_back( dehpath );
		}
	}

	const char* foundiwad = D_FindIWAD( IWAD_MASK_DOOM, &info.mission );
	if( foundiwad != nullptr )
	{
		info.iwadfile = foundiwad;
		if( !CheckWad( info.iwadfile ) )
		{
			return nullptr;
		}
	}

	int32_t pwadparm = M_CheckParmWithArgs( "-file", 1 );
	if( pwadparm > 0 )
	{
		while ( ++pwadparm != myargc && myargv[ pwadparm ][ 0 ] != '-' )
		{
			std::string pwadpath = ValidateFile( myargv[ pwadparm ] );
			info.pwadfiles.push_back( pwadpath );

			if( !CheckWad( pwadpath ) )
			{
				return nullptr;
			}
		}
	}

	auto CopyString = []( const std::string& str ) -> const char*
	{
		if( !str.empty() )
		{
			size_t size = str.size() + 1;
			char* output = (char*)Z_MallocZero( size, PU_STATIC, nullptr );
			strcpy_s( output, size, str.c_str() );
			return output;
		}
		return nullptr;
	};

	auto ReplaceNewlines = []( const std::string& str ) -> std::string
	{
		std::string output = str;
		size_t foundpos = 0;
		while( ( foundpos = output.find( "\\n", foundpos ) ) != std::string::npos )
		{
			output.replace( foundpos, 2, "\n" );
			foundpos += 1;
		}

		return output;
	};

	gameconf_t* output = (gameconf_t*)Z_MallocZero( sizeof( gameconf_t ), PU_STATIC, nullptr );
	output->title = CopyString( info.title );
	output->description = CopyString( info.description );
	output->version = CopyString( info.version );
	output->iwad = CopyString( info.iwadfile );
	output->pwads = !info.pwadfiles.empty()		? (const char**)Z_MallocZero( sizeof( const char* ) * info.pwadfiles.size(), PU_STATIC, nullptr )
												: nullptr;
	output->pwadscount = (int32_t)info.pwadfiles.size();
	for( int32_t pwad : iota( 0, info.pwadfiles.size() ) )
	{
		output->pwads[ pwad ] = CopyString( info.pwadfiles[ pwad ] );
	}

	output->dehfiles = !info.dehfiles.empty()	? (const char**)Z_MallocZero( sizeof( const char* ) * info.dehfiles.size(), PU_STATIC, nullptr )
												: nullptr;
	output->dehfilescount = (int32_t)info.dehfiles.size();
	for( int32_t deh : iota( 0, info.dehfiles.size() ) )
	{
		output->dehfiles[ deh ] = CopyString( info.dehfiles[ deh ] );
	}
	output->executable = info.executable;
	output->mission = info.mission;
	output->options = CopyString( ReplaceNewlines( info.options ) );

	return output;
}

void D_DestroyGameConf( gameconf_t* conf )
{
	if( conf )
	{
		if( conf->options ) Z_Free( (void*)conf->options );

		if( conf->dehfiles )
		{
			for( int32_t deh : iota( 0, conf->dehfilescount ) )
			{
				Z_Free( (void*)conf->dehfiles[ deh ] );
			}
			Z_Free( (void*)conf->dehfiles );
		}

		if( conf->pwads )
		{
			for( int32_t pwad : iota( 0, conf->pwadscount ) )
			{
				Z_Free( (void*)conf->pwads[ pwad ] );
			}
			Z_Free( (void*)conf->pwads );
		}

		if( conf->iwad ) Z_Free( (void*)conf->iwad );
		if( conf->version ) Z_Free( (void*)conf->version );
		if( conf->description ) Z_Free( (void*)conf->description );
		if( conf->title ) Z_Free( (void*)conf->title );

		Z_Free( (void*)conf );
	}
}

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

#include "m_zipfile.h"

#include "doomtype.h"

#include <minizip/unzip.h>

#include <filesystem>
#include <vector>
#include <regex>
#include <cstdio>
#include <cstdint>

#ifdef _WIN32

#define DIR_SEPARATOR '\\'

#else

#define DIR_SEPARATOR '/'

#endif

bool M_ZipExtractAllFromFile( const char* inputfile, const char* outputfolder, zipprogress_t& progressfunc )
{
	std::filesystem::path outputpath = outputfolder;
	std::filesystem::create_directories( outputpath );

	unzFile input = unzOpen64( inputfile );

	if( !input )
	{
		return false;
	}

	unz_global_info64 info;
	unzGetGlobalInfo64( input, &info );

	ptrdiff_t totalsize = 0;
	ptrdiff_t written = 0;

	if( unzGoToFirstFile( input ) == UNZ_OK )
	{
		do
		{
			unz_file_info64 fileinfo;
			unzGetCurrentFileInfo64( input, &fileinfo, nullptr, 0, nullptr, 0, nullptr, 0 );
			totalsize += fileinfo.uncompressed_size;
		} while( unzGoToNextFile( input ) == UNZ_OK );

		unzGoToFirstFile( input );
		do
		{
			char filename[ 1024 ];
			unz_file_info64 fileinfo;
			unzGetCurrentFileInfo64( input, &fileinfo, filename, 1024, nullptr, 0, nullptr, 0 );

			if( unzOpenCurrentFile( input ) == UNZ_OK )
			{
				unsigned char readbuffer[ 4096 ];
				std::string writetostr = outputpath.string() + filename;
				std::replace( writetostr.begin(), writetostr.end(), '/', DIR_SEPARATOR );
				std::filesystem::path writeto = writetostr;
				if( writeto.has_parent_path() )
				{
					std::filesystem::create_directories( writeto.parent_path() );
				}

				if( writeto.has_filename() )
				{
					FILE* output = fopen( writeto.string().c_str(), "wb" );

					int32_t readamount = 0;
					do
					{
						readamount = unzReadCurrentFile( input, readbuffer, 4096 );
						if( readamount > 0 )
						{
							fwrite( readbuffer, sizeof( unsigned char ), readamount, output );
							written += readamount;
							progressfunc( written, totalsize );
						}
					} while( readamount > 0 );

					fclose( output );
				}
				unzCloseCurrentFile( input );
			}
		} while( unzGoToNextFile( input ) == UNZ_OK );
	}

	unzClose( input );
	return true;
}

bool M_ZipExtractFromICE( const char* inputfolder, const char* outputfolder, zipprogress_t& progressfunc )
{
	std::filesystem::path inputpath = inputfolder;
	if( !std::filesystem::exists( inputpath ) && !std::filesystem::is_directory( inputfolder ) )
	{
		return false;
	}

	constexpr const char* DATRegexString = ".+\\.dat$";
	static const std::regex DATRegex = std::regex( DATRegexString, std::regex_constants::icase );

	std::filesystem::path datpath;

	for( auto& file : std::filesystem::directory_iterator( inputpath ) )
	{
		if( std::regex_match( file.path().string(), DATRegex ) )
		{
			datpath = file.path();
			break;
		}
	}

	if( datpath.empty() )
	{
		return false;
	}

	std::filesystem::path icestem = datpath.stem();
	std::filesystem::path firstfile = inputfolder + icestem.string() + ".1";

	if( !std::filesystem::exists( firstfile ) )
	{
		return false;
	}

	// Reference documentation: https://fileformats.archiveteam.org/wiki/MS-DOS_EXE
	PACKED_STRUCT( DOSExecutableHeader
	{
		uint8_t		e_magic[ 2 ];
		uint16_t	e_cblp;
		uint16_t	e_cp;
		uint16_t	e_crlc;
		uint16_t	e_cparhdr;
		uint16_t	e_minalloc;
		uint16_t	e_maxalloc;
		int16_t		e_ss;
		uint16_t	e_sp;
		uint16_t	e_csum;
		uint16_t	e_ip;
		int16_t		e_cs;
		uint16_t	e_lfarlc;
		uint16_t	e_ovno;
	} );

	DOSExecutableHeader header;

	FILE* input = fopen( firstfile.string().c_str(), "rb" );
	fread( &header, sizeof( DOSExecutableHeader ), 1, input );
	if( header.e_magic[ 0 ] != 'M' || header.e_magic[ 1 ] != 'Z' )
	{
		fclose( input );
		return false;
	}

	size_t zipstart;
	size_t seekto;
	
	zipstart = seekto = header.e_cblp == 0 ? header.e_cp * 512 : ( header.e_cp - 1 ) * 512 + header.e_cblp;

	std::filesystem::path outputpath = outputfolder;
	std::filesystem::create_directories( outputpath );
	
	std::filesystem::path outputfile = outputpath.string() + icestem.string() + ".zip";
	
	FILE* output = fopen( outputfile.string().c_str(), "wb" );
	
	uint8_t buffer[ 4096 ];
	std::vector< uint8_t > filecontents;

	int32_t nextfileindex = 2;
	while( input && output )
	{
		fseek( input, seekto, SEEK_SET );

		size_t read = 0;
		do
		{
			read = fread( buffer, 1, 4096, input );
			if( read > 0 )
			{
				filecontents.insert( filecontents.end(), buffer, buffer + read );
			}
		} while( read > 0 );
		fclose( input );
		input = nullptr;

		std::filesystem::path nextfile = inputfolder + icestem.string() + "." + std::to_string( nextfileindex++ );
		if( std::filesystem::exists( nextfile ) )
		{
			input = fopen( nextfile.string().c_str(), "rb" );
			seekto = 0;
		}
		else
		{
			size_t length = ftell( output );

			// We need to patch up the central directory to account for
			// stripping of the executable segment. This includes every
			// file entry.
			PACKED_STRUCT( CentralDirectory
			{
				uint8_t		signature[ 4 ];
				uint16_t	disk_number;
				uint16_t	disk_with_central_directory;
				uint16_t	disk_entries;
				uint16_t	toal_entries;
				uint32_t	central_directory_size;
				uint32_t	central_directory_offset;
				uint16_t	comment_length;
			} );

			PACKED_STRUCT( CentralFileHeader
			{
				uint8_t		signature[ 4 ];
				uint16_t	version;
				uint16_t	version_needed;
				uint16_t	flags;
				uint16_t	compression;
				uint16_t	mod_time;
				uint16_t	mod_date;
				uint32_t	crc32;
				uint32_t	compressed_size;
				uint32_t	uncompressed_size;
				uint16_t	filename_len;
				uint16_t	extrafield_len;
				uint16_t	comment_len;
				uint16_t	disk_number_start;
				uint16_t	internal_attributes;
				uint32_t	external_attributes;
				uint32_t	local_header_offset;
			} );

			uint8_t* fileend = filecontents.data() + filecontents.size();
			CentralDirectory* directory = (CentralDirectory*)( fileend - sizeof( CentralDirectory ) );
			
			directory->central_directory_offset -= zipstart;

			uint8_t* currheaderpos = fileend - sizeof( CentralDirectory ) - directory->central_directory_size;
			uint8_t* currheaderend = fileend - sizeof( CentralDirectory );
			while( currheaderpos < currheaderend )
			{
				CentralFileHeader* header = (CentralFileHeader*)currheaderpos;
				header->local_header_offset -= zipstart;
				currheaderpos += sizeof( CentralFileHeader ) + header->filename_len  + header->extrafield_len + header->comment_len;
			}

			fwrite( (void*)filecontents.data(), 1, filecontents.size(), output );
			fclose( output );
		}
	}

	if( input )
	{
		fclose( input );
	}

	std::filesystem::path deflatepath = outputpath.string() + icestem.string() + DIR_SEPARATOR;

	return M_ZipExtractAllFromFile( outputfile.string().c_str(), deflatepath.string().c_str(), progressfunc );

}

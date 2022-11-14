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

#include <minizip/unzip.h>

#include <filesystem>
#include <vector>
#include <cstdio>

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

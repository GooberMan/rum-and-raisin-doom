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

#if !defined( __M_ZIPFILE_H__ )
#define __M_ZIPFILE_H__

#if defined( __cplusplus )

#include <functional>
#include <cstddef>

using zipprogress_t = std::function< bool( ptrdiff_t, ptrdiff_t ) >;

bool M_ZipExtractAllFromFile( const char* intputfile, const char* outputfolder, zipprogress_t& progressfunc );
bool M_ZipExtractFromICE( const char* inputfolder, const char* outputfolder, zipprogress_t& progressfunc );

#endif // defined( __cplusplus )

#endif //!defined( __M_ZIPFILE_H__ )

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

#include "m_url.h"

#include <curl/curl.h>

thread_local CURL* curl = nullptr;

void M_URLInit( void )
{
	curl_global_init( CURL_GLOBAL_ALL );
	curl = curl_easy_init();
}

void M_URLDeinit( void )
{
	curl_easy_cleanup( curl );
	curl = nullptr;
}

size_t CURL_StringWrite( char *ptr, size_t size, size_t count, void *str )
{
	std::string& output = *(std::string*)str;
	output.append( ptr, count );

	return size * count;
};

bool M_URLGetString( std::string& output, const char* url, const char* params )
{
	output.clear();

	curl_easy_setopt( curl, CURLOPT_URL, url );
	curl_easy_setopt( curl, CURLOPT_POSTFIELDS, params );
	curl_easy_setopt( curl, CURLOPT_WRITEDATA, &output );
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, &CURL_StringWrite );
	CURLcode errorcode = curl_easy_perform( curl );
	if( errorcode != CURLE_OK )
	{
		output = curl_easy_strerror( errorcode );
	}
	return errorcode == CURLE_OK;
}

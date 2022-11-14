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

size_t CURL_StringWrite( char *ptr, size_t size, size_t count, void *str )
{
	std::string& output = *(std::string*)str;
	output.append( ptr, count );

	return size * count;
};

size_t CURL_BytesWrite( unsigned char *ptr, size_t size, size_t count, void *bytes )
{
	std::vector< unsigned char >& output = *(std::vector< byte >*)bytes;
	output.insert( output.end(), ptr, ptr + count );

	return size * count;
};

int32_t CURL_Progress( void* user, curl_off_t down_total, curl_off_t down_now, curl_off_t up_total, curl_off_t up_now )
{
	if( user )
	{
		urlprogress_t& func = *(urlprogress_t*)user;
		bool continuedownload = func( down_now, down_total );
		return continuedownload ? 0 : 1;
	}

	return 0;
}

void M_URLInit( void )
{
	curl_global_init( CURL_GLOBAL_ALL );
	curl = curl_easy_init();
	curl_easy_setopt( curl, CURLOPT_XFERINFOFUNCTION, &CURL_Progress );
	curl_easy_setopt( curl, CURLOPT_NOPROGRESS, 0 );
}

void M_URLDeinit( void )
{
	curl_easy_cleanup( curl );
	curl = nullptr;
}

bool M_URLGetString( std::string& output, const char* url, const char* params, urlprogress_t* func )
{
	output.clear();

	curl_easy_setopt( curl, CURLOPT_URL, url );
	curl_easy_setopt( curl, CURLOPT_POSTFIELDS, params );
	curl_easy_setopt( curl, CURLOPT_WRITEDATA, &output );
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, &CURL_StringWrite );
	curl_easy_setopt( curl, CURLOPT_XFERINFODATA, func );
	CURLcode errorcode = curl_easy_perform( curl );
	if( errorcode != CURLE_OK )
	{
		output = curl_easy_strerror( errorcode );
	}
	return errorcode == CURLE_OK;
}

bool M_URLGetBytes( std::vector< unsigned char >& output, int64_t& outfiletime, std::string& outerror, const char* url, const char* params, urlprogress_t* func )
{
	output.clear();
	outerror.clear();

	curl_easy_setopt( curl, CURLOPT_URL, url );
	curl_easy_setopt( curl, CURLOPT_POSTFIELDS, params );
	curl_easy_setopt( curl, CURLOPT_WRITEDATA, &output );
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, &CURL_BytesWrite );
	curl_easy_setopt( curl, CURLOPT_XFERINFODATA, func );
	curl_easy_setopt( curl, CURLOPT_FILETIME, true );
	CURLcode errorcode = curl_easy_perform( curl );
	if( errorcode != CURLE_OK )
	{
		outerror = curl_easy_strerror( errorcode );
	}
	else
	{
		curl_easy_getinfo( curl, CURLINFO_FILETIME_T, &outfiletime );
	}
	return errorcode == CURLE_OK;
}

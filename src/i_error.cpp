//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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

#include "i_error.h"

#include "i_log.h"
#include "i_system.h"
#include "i_video.h"

#include "m_argv.h"
#include "m_container.h"
#include "m_misc.h"

#include <SDL2/SDL.h>

#include <exception>
#include <atomic>
#include <semaphore>
#include <thread>

using semaphore = std::counting_semaphore< 1 >;

static std::thread*								error_thread;
static semaphore*								error_posted;
static semaphore*								error_wait;
static AtomicCircularQueue< const char* >*		error_queue;

#if defined( WIN32 )
	#include <excpt.h>
	// Until I fix up the boolean redefinition mess, we'll forward declare the function we want from WinAPI
	DOOM_C_API __declspec( dllimport ) int __stdcall IsDebuggerPresent( void );
	DOOM_C_API __declspec( dllimport ) int __stdcall SetUnhandledExceptionFilter( void* filter );
	#define IsDebuggerAttached() IsDebuggerPresent()
#else
	#include <execinfo.h>
	// Needs a large chunk of code on Linux to work, but the intrinsic we pick should do things just fine
	#define SetUnhandledExceptionFilter( filter )
	#define IsDebuggerAttached() true
#endif

#ifdef __has_builtin
	#if __has_builtin( __builtin_debugtrap )
		#define DoDebugBreak() __builtin_debugtrap()
	#endif
#elif defined( _MSC_VER )
	#include <intrin.h>
	#define DoDebugBreak() __debugbreak()
#endif

#if !defined( DoDebugBreak )
	#include "signal.h"
	#if defined( SIGTRAP )
		#define DoDebugBreak() raise( SIGTRAP )
	#else
		#define DoDebugBreak() raise( SIGABRT )
	#endif
#endif 

void I_ErrorThread()
{
	error_posted->acquire();

	const char* firsterror = error_queue->access();

	I_LogDebug( firsterror );

	if( IsDebuggerAttached() )
	{
		DoDebugBreak();
	}

	if( !M_ParmExists( "-nogui" ) )
	{
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, PACKAGE_STRING, firsterror, nullptr );
	}

	exit(-1);
}

void I_PostError( const char* error )
{
	error_queue->push( error );
	error_posted->release();
	error_wait->acquire();
}

void I_UnhandledUserException()
{
	try
	{
		std::rethrow_exception( std::current_exception() );
	}
	catch( const std::exception& e )
	{
		I_PostError( e.what() );
	}
	catch( ... )
	{
		I_PostError( "Unknown user exception" );
	}
}

int32_t I_UnhandledStructuredException()
{
	I_PostError( "Unknown structed exception" );
	return EXCEPTION_EXECUTE_HANDLER;
}

void I_InitError( void )
{
	std::set_terminate( &I_UnhandledUserException );
	SetUnhandledExceptionFilter( &I_UnhandledStructuredException );

	error_queue = new AtomicCircularQueue< const char* >( 512 );
	error_posted = new semaphore( 0 );
	error_wait = new semaphore( 0 );

	error_thread = new std::thread( [] { I_ErrorThread(); } );
}

thread_local char error_message_buffer[ 1024 ];

void I_Error( const char *error, ... )
{
	va_list argptr;

	// Write a copy of the message into buffer.
	va_start( argptr, error );
	memset( error_message_buffer, 0, sizeof( error_message_buffer ) );
	M_vsnprintf( error_message_buffer, sizeof( error_message_buffer ), error, argptr );
	va_end( argptr );

	I_PostError( error_message_buffer );
}

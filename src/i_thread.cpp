//
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
// DESCRIPTION: Threading library, C ABI functionality that wraps
// C++11 threaded primitives
//

#include "i_thread.h"

#include "z_zone.h"

#include <atomic>
#include <semaphore>
#include <thread>

#if OS_CHECK( WINDOWS )
#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
	#define NOMINMAX
#endif
#include <windows.h>
#include <processthreadsapi.h>
#include <stringapiset.h>
#endif // OS_CHECK

std::atomic< size_t > JobThread::num_threads_created = 0;

threadhandle_t I_ThreadCreate( threadfunc_t runfunc, void* userdata )
{
	void* data = Z_Malloc( sizeof( std::thread ), PU_STATIC, NULL );
	new( data ) std::thread( runfunc, userdata );

	return data;
}

void I_ThreadDestroy( threadhandle_t thread )
{
	std::thread* thisthread = (std::thread*)thread;
	thisthread->~thread();
	Z_Free( thread );
}

void I_ThreadJoin( threadhandle_t thread )
{
	std::thread* thisthread = (std::thread*)thread;
	thisthread->join();
}

size_t I_ThreadGetHardwareCount( void )
{
	return std::thread::hardware_concurrency();
}

void I_Yield( void )
{
	std::this_thread::yield();
}

using semaphore = std::counting_semaphore< 1 >;

semaphore_t I_SemaphoreCreate( int32_t initialcount )
{
	void* data = Z_Malloc( sizeof( semaphore ), PU_STATIC, NULL );
	new( data ) semaphore( initialcount );
	return data;
}

void I_SemaphoreAcquire( semaphore_t sem )
{
	((semaphore*)sem)->acquire();
}

void I_SemaphoreRelease( semaphore_t sem )
{
	((semaphore*)sem)->release();
}


atomicval_t I_AtomicLoad( atomicptr_t atomic )
{
	return ( (std::atomic< atomicval_t >*)atomic )->load();
}

atomicval_t I_AtomicExchange( atomicptr_t atomic, atomicval_t val )
{
	return ( (std::atomic< atomicval_t >*)atomic )->exchange( val );
}

atomicval_t I_AtomicIncrement( atomicptr_t atomic, atomicval_t val )
{
	return ( (std::atomic< atomicval_t >*)atomic )->fetch_add( val );
}

atomicval_t I_AtomicDecrement( atomicptr_t atomic, atomicval_t val )
{
	return ( (std::atomic< atomicval_t >*)atomic )->fetch_sub( val );
}

void JobThread::SetupThreadName()
{
	constexpr size_t BufferSize = 256;
	char threadname[ BufferSize ] = { 0 };
	sprintf( threadname, "Job thread %d", (int32_t)thread_index );

#if OS_CHECK( WINDOWS )
	wchar_t wthreadname[ BufferSize ] = { 0 };
	MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, threadname, -1, wthreadname, BufferSize );
	SetThreadDescription( worker_thread.native_handle(), wthreadname );
#endif // OS_CHECK
	M_ProfileThreadInit( threadname );
}

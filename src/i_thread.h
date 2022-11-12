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


#ifndef __I_THREAD__
#define __I_THREAD__

#include "doomtype.h"

DOOM_C_API typedef int32_t (*threadfunc_t)( void* );
DOOM_C_API typedef void* threadhandle_t;
DOOM_C_API typedef ptrdiff_t atomicval_t;
DOOM_C_API typedef atomicval_t* atomicptr_t;

DOOM_C_API typedef void* semaphore_t;

#ifdef _MSC_VER
#define THREADLOCAL DOOM_C_API static __declspec(thread)
#else
#define THREADLOCAL DOOM_C_API static __thread
#endif // _MSC_VER

DOOM_C_API threadhandle_t I_ThreadCreate( threadfunc_t runfunc, void* userdata );
DOOM_C_API void I_ThreadDestroy( threadhandle_t thread );
DOOM_C_API void I_ThreadJoin( threadhandle_t thread );

DOOM_C_API size_t I_ThreadGetHardwareCount( void );

DOOM_C_API void I_Yield( void );

DOOM_C_API semaphore_t I_SemaphoreCreate( int32_t initialcount );
DOOM_C_API void I_SemaphoreAcquire( semaphore_t sem );
DOOM_C_API void I_SemaphoreRelease( semaphore_t sem );

// Do we need to go this cray cray?
//atomicptr_t I_AtomicCreate( atomicval_t init );
//void I_AtomicDestroy( void );

DOOM_C_API atomicval_t I_AtomicLoad( atomicptr_t atomic );
DOOM_C_API atomicval_t I_AtomicExchange( atomicptr_t atomic, atomicval_t val );
DOOM_C_API atomicval_t I_AtomicIncrement( atomicptr_t atomic, atomicval_t val );
DOOM_C_API atomicval_t I_AtomicDecrement( atomicptr_t atomic, atomicval_t val );

#if defined( __cplusplus )
#include "m_container.h"
#include <functional>
#include <atomic>
#include <thread>

class JobThread
{
public:
	enum : size_t { queue_size = 128 };
	using func_type = std::function< void() >;

	JobThread()
		: jobs( queue_size )
		, terminate( false )
		, worker_thread( std::thread( [ this ]() { this->Run(); } ) )
 	{
	}

	~JobThread()
	{
		if( worker_thread.joinable() )
		{
			terminate = true;
			worker_thread.join();
		}
	}

	void AddJob( func_type&& func )
	{
		jobs.push( func );
	}

private:
	void Run()
	{
		while( !terminate.load() )
		{
			if( jobs.valid() )
			{
				func_type& func = jobs.access();
				func();
				jobs.pop();
			}
			else
			{
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
			}
		}
	}

	AtomicCircularQueue< func_type >	jobs;
	std::atomic< bool >					terminate;
	std::thread							worker_thread;
};

#endif //defined( __cplusplus )

#endif // __I_THREAD__

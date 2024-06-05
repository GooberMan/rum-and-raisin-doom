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
#include "m_misc.h"

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
#include "m_profile.h"
#include <functional>
#include <atomic>
#include <thread>

class Spinlock
{
public:
	INLINE Spinlock()
		: locked( false )
	{
	}

	INLINE bool Acquire()
	{
		uint64_t waitcount = 0;
		bool expected = false;
		while( !locked.compare_exchange_weak(expected, true) )
		{
			if( ( waitcount & 1023 ) == 1023 )
			{
				std::this_thread::sleep_for( std::chrono::milliseconds( 0 ) );
			}
			else if( ( waitcount & 127 ) == 127 )
			{
				std::this_thread::yield();
			}
			else if( ( waitcount & 15 ) == 15 )
			{
				if constexpr( ARCH_CHECK( x86 ) || ARCH_CHECK( x64 ) )
				{
					_mm_pause();
				}
			}

			expected = false;
			++waitcount;
		}
	}

	INLINE bool Release()
	{
		locked.store( false );
	}

private:
	std::atomic<bool>		locked;
};

class JobThread
{
public:
	enum : size_t { queue_size = 16384 };
	using func_type = std::function< void() >;

	JobThread()
		: jobs( queue_size )
		, thread_index( num_threads_created.fetch_add( 1 ) )
		, terminate( false )
		, worker_thread( std::thread( [ this ](){ this->Run(); } ) )
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

	void AddJob( const func_type& func )
	{
		jobs.push( func );
	}

	void Flush()
	{
		uint64_t waitcount = 0;
		while( !jobs.empty() )
		{
			if( ( waitcount & 1023 ) == 1023 )
			{
				std::this_thread::sleep_for( std::chrono::milliseconds( 0 ) );
			}
			else if( ( waitcount & 127 ) == 127 )
			{
				std::this_thread::yield();
			}
			else if( ( waitcount & 15 ) == 15 )
			{
				if constexpr( ARCH_CHECK( x86 ) || ARCH_CHECK( x64 ) )
				{
					_mm_pause();
				}
			}

			++waitcount;
		}
	}

private:
	void SetupThreadName();

	void Run()
	{
		SetupThreadName();

		uint64_t waitcount = 0;
		while( !terminate.load() )
		{
			if( jobs.valid() )
			{
				func_type& func = jobs.access();
				func();
				jobs.pop();
				waitcount = 0;
			}
			else
			{
				if( ( waitcount & 1023 ) == 1023 )
				{
					std::this_thread::sleep_for( std::chrono::milliseconds( 0 ) );
				}
				else if( ( waitcount & 127 ) == 127 )
				{
					std::this_thread::yield();
				}
				else if( ( waitcount & 15 ) == 15 )
				{
#if ( ARCH_CHECK( x86 ) || ARCH_CHECK( x64 ) )
					_mm_pause();
#elif ( ARCH_CHECK( ARM32 ) || ARCH_CHECK( ARM64 ) )
					__yield();
#endif // ARCH_CHECK
				}

				++waitcount;
			}
		}
	}

	AtomicCircularQueue< func_type >	jobs;
	size_t								thread_index;
	std::atomic< bool >					terminate;
	std::thread							worker_thread;

	static std::atomic< size_t >		num_threads_created;
};

class JobSystem
{
public:
	static constexpr int32_t MaxJobs = JOBSYSTEM_MAXJOBS;

	using func_type = JobThread::func_type;

	JobSystem( size_t maxnumjobs )
		: jobpool( maxnumjobs )
		, maxjobs( maxnumjobs )
	{
		nextjobthread = jobpool.begin();
		endjobthread = nextjobthread + maxnumjobs;
	}

	void AddJob( const func_type& func )
	{
		if( endjobthread == jobpool.begin() )
		{
			func();
		}
		else
		{
			nextjobthread->AddJob( func );
			if( ++nextjobthread == endjobthread )
			{
				nextjobthread = jobpool.begin();
			}
		}
	}

	void SetMaxJobs( size_t maxnumjobs )
	{
		maxnumjobs = M_MIN( maxnumjobs, maxjobs );
		endjobthread = jobpool.begin() + maxnumjobs;
		if( nextjobthread - endjobthread >= 0 )
		{
			nextjobthread = jobpool.begin();
		}
	}

	void Flush()
	{
		for( JobThread& currthread : jobpool )
		{
			currthread.Flush();
		}
	}

	void NewProfileFrame()
	{
		for( JobThread& currthread : jobpool )
		{
			currthread.AddJob( []()
			{
				M_ProfileNewFrame();
			} );
			currthread.Flush();
		}
	}

private:
	std::vector< JobThread >			jobpool;
	std::vector< JobThread >::iterator	nextjobthread;
	std::vector< JobThread >::iterator	endjobthread;
	size_t								maxjobs;
};

#endif //defined( __cplusplus )

#endif // __I_THREAD__

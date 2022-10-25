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

#ifdef __cplusplus
extern "C" {
#endif

#include "doomtype.h"

typedef int32_t (*threadfunc_t)( void* );
typedef void* threadhandle_t;
typedef ptrdiff_t atomicval_t;
typedef atomicval_t* atomicptr_t;

typedef void* semaphore_t;

#ifdef _MSC_VER
#define THREADLOCAL static __declspec(thread)
#else
#define THREADLOCAL static __thread
#endif // _MSC_VER

threadhandle_t I_ThreadCreate( threadfunc_t runfunc, void* userdata );
void I_ThreadDestroy( threadhandle_t thread );
void I_ThreadJoin( threadhandle_t thread );

size_t I_ThreadGetHardwareCount( void );

void I_Yield( void );

semaphore_t I_SemaphoreCreate( int32_t initialcount );
void I_SemaphoreAcquire( semaphore_t sem );
void I_SemaphoreRelease( semaphore_t sem );

// Do we need to go this cray cray?
//atomicptr_t I_AtomicCreate( atomicval_t init );
//void I_AtomicDestroy( void );


atomicval_t I_AtomicLoad( atomicptr_t atomic );
atomicval_t I_AtomicExchange( atomicptr_t atomic, atomicval_t val );
atomicval_t I_AtomicIncrement( atomicptr_t atomic, atomicval_t val );
atomicval_t I_AtomicDecrement( atomicptr_t atomic, atomicval_t val );

#ifdef __cplusplus
}
#endif

#endif // __I_THREAD__

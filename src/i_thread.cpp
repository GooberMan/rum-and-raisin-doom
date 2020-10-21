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

extern "C"
{
	#include "i_thread.h"
	#include "z_zone.h"
}

#include <atomic>
#include <thread>

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

void I_Yield( void )
{
	std::this_thread::yield();
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


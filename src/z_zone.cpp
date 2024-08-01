//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//	Zone Memory Allocation. Neat.
//

#include <string.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"

#include "z_zone.h"

//
// ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks,
//  and there will never be two contiguous free memblocks.
// The rover can be left pointing at a non-empty block.
//
// It is of no value to free a cachable block,
//  because it will get overwritten automatically if needed.
// 
 
// Need things to be 16 by default for SIMD purposes...
#define MEM_ALIGN 16ull
#define ZONEID	0x1d4a11
#define MAGICMARKER 0xDECEA5EDu

typedef struct memblock_s
{
	uint32_t			magic;
	uint32_t			size;	// including the header and possibly tiny fragments
	void**				user;
	uint32_t			datasize;
	int32_t				tag;
	int32_t				id;		// should be ZONEID
	uint32_t			line;
	const char*			file;
	memdestruct_t		destructor;
	struct memblock_s*	next;
	struct memblock_s*	prev;
} memblock_t;


typedef struct
{
	// total bytes malloced, including header
	size_t		size;
	memblock_t*	head;
} memzone_t;



static memzone_t mainzone = {};

//
// Z_Init
//
void Z_Init (void)
{
    mainzone = {};
}

//
// Z_Free
//
void Z_Free (void* ptr)
{
#if PARANOIA
	Z_CheckHeap();
#endif

    memblock_t*		block;
    memblock_t*		other;

    block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));

    if (block->id != ZONEID)
	{
		I_Error ("Z_Free: freed a pointer without ZONEID");
	}

	if(block->magic != MAGICMARKER)
	{
		I_Error ("Z_Free: something has overwritten this memory's data block!");
	}

    if( block->user != nullptr )
    {
    	// clear the user's mark
	    *block->user = 0;
    }

	if( block->destructor )
	{
		block->destructor( ptr, block->datasize );
	}

	if( mainzone.head == block ) mainzone.head = block->next;
	if( block->prev ) block->prev->next = block->next;
	if( block->next ) block->next->prev = block->prev;

	_mm_free( block );
}



//
// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//

#define PARANOIA 0

void* Z_MallocTracked( const char* file, size_t line, size_t size, int32_t tag, void **ptr, memdestruct_t destructor )
{
#if PARANOIA
	Z_CheckHeap();
#endif

	if( ptr == nullptr && tag >= PU_PURGELEVEL )
	{
		I_Error ("Z_Malloc: an owner is required for purgable blocks");
	}

	uint32_t datasize = size;

    size = (size + MEM_ALIGN - 1ull) & ~(MEM_ALIGN - 1ull);
    
    // scan through the block list,
    // looking for the first free block
    // of sufficient size,
    // throwing out any purgable blocks along the way.

    // account for size of block header
    size += sizeof(memblock_t);

	// there will be a free fragment after the allocated block
	memblock_t* newblock = (memblock_t *)_mm_malloc( size, MEM_ALIGN );
	newblock->magic			= MAGICMARKER;
	newblock->size			= size;
	newblock->user			= ptr;
	newblock->datasize		= datasize;
	newblock->tag			= tag;
	newblock->id			= ZONEID;
	newblock->line			= line;
	newblock->file			= file;
	newblock->destructor	= destructor;
	
	void* result  = (void *)( newblock + 1 );
	if( newblock->user )
	{
		*newblock->user = result;
	}

	newblock->prev = nullptr;
	newblock->next = mainzone.head;

	if( mainzone.head ) mainzone.head->prev = newblock;
	mainzone.head = newblock;

	return result;
}



//
// Z_FreeTags
//
void
Z_FreeTags
( int		lowtag,
  int		hightag )
{
#if PARANOIA
	Z_CheckHeap();
#endif

    memblock_t*	block;
    memblock_t*	next;
	memblock_t* prev;
	
	for (block = mainzone.head ;
		block != nullptr ;
		block = next)
	{
		// get link before freeing
		prev = block;
		next = block->next;

		if (block->tag >= lowtag && block->tag <= hightag)
		{
			Z_Free ( (byte *)block+sizeof(memblock_t));
		}
	}
}



//
// Z_DumpHeap
// Note: TFileDumpHeap( stdout ) ?
//
void
Z_DumpHeap
( int		lowtag,
  int		hightag )
{
    memblock_t*	block;
	
    printf ("zone size: %zd  location: %p\n",
	    mainzone.size,mainzone);
    
    printf ("tag range: %i to %i\n",
	    lowtag, hightag);
	
    for (block = mainzone.head ; ; block = block->next)
    {
	if (block->tag >= lowtag && block->tag <= hightag)
	    printf ("block:%p    size:%7zd    user:%p    tag:%3i\n",
		    block, block->size, block->user, block->tag);
		
	if (block->next == nullptr)
	{
	    // all blocks have been hit
	    break;
	}
	
	if ( block->next->prev != block)
	    printf ("ERROR: next block doesn't have proper back link\n");

    }
}


//
// Z_FileDumpHeap
//
void Z_FileDumpHeap (FILE* f)
{
    memblock_t*	block;
	
    fprintf (f,"zone size: %zd  location: %p\n",mainzone.size,mainzone);
	
    for (block = mainzone.head ; ; block = block->next)
    {
	fprintf (f,"block:%p    size:%7zd    user:%p    tag:%3i\n",
		 block, block->size, block->user, block->tag);
		
	if (block->next == nullptr)
	{
	    // all blocks have been hit
	    break;
	}
	
	if ( block->next->prev != block)
	    fprintf (f,"ERROR: next block doesn't have proper back link\n");

    }
}



//
// Z_CheckHeap
//
void Z_CheckHeap (void)
{
	memblock_t* prev = NULL;
    for (memblock_t* block = mainzone.head; ; prev = block, block = block->next)
    {
		if (block->next == nullptr)
		{
			// all blocks have been hit
			break;
		}

		if( block->magic != MAGICMARKER )
		{
			I_Error ("Z_CheckHeap: something has overwritten this memory's data block!");
		}
	
		if ( block->next->prev != block)
			I_Error ("Z_CheckHeap: next block doesn't have proper back link\n");
    }
}




//
// Z_ChangeTag
//
doombool Z_ChangeTag2(void *ptr, int tag, const char *file, int line)
{
    memblock_t*	block;
	doombool changed;
	
    block = (memblock_t *) ((byte *)ptr - sizeof(memblock_t));

    if (block->id != ZONEID)
        I_Error("%s:%i: Z_ChangeTag: block without a ZONEID!",
                file, line);

    if (tag >= PU_PURGELEVEL && block->user == NULL)
        I_Error("%s:%i: Z_ChangeTag: an owner is required "
                "for purgable blocks", file, line);

	changed = block->tag != tag;
    block->tag = tag;

	return changed;
}

doombool Z_LowerTagTracked( void* ptr, int32_t tag, const char* file, size_t line )
{
	memblock_t*	block = (memblock_t *) ((byte *)ptr - sizeof(memblock_t));

	if (block->id != ZONEID)
	{
		I_Error( "%s:%i: Z_LowerTag: block without a ZONEID!", file, line);
	}

	int32_t oldtag = block->tag;
	block->tag = M_MIN( block->tag, tag );

	if( oldtag != block->tag )
	{
		if( tag >= PU_PURGELEVEL && block->user == nullptr )
		{
			I_Error( "%s:%i: Z_LowerTag: an owner is required for purgable blocks", file, line );
		}
		return true;
	}

	return false;
}

void Z_ChangeUser(void *ptr, void **user)
{
    memblock_t*	block;

    block = (memblock_t *) ((byte *)ptr - sizeof(memblock_t));

    if (block->id != ZONEID)
    {
        I_Error("Z_ChangeUser: Tried to change user for invalid block!");
    }

    block->user = user;
    *user = ptr;
}


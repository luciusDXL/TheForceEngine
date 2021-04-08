#pragma once
//////////////////////////////////////////////////////////////////////
// Allocator API as found in the Jedi Engine
// So far this is used by the INF related systems but may be moved in
// the future if it is also used by Logics or other systems.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <vector>
#include <string>

struct Allocator;

namespace InfAllocator
{
	// Create and free an allocator.
	Allocator* allocator_create(s32 allocSize);
	void allocator_free(Allocator* alloc);

	// Allocate and free individual items.
	void* allocator_newItem(Allocator* alloc);
	void  allocator_deleteItem(Allocator* alloc, void* item);

	// Random access.
	void* allocator_getByIndex(Allocator* alloc, s32 index);

	// Iteration
	void* allocator_getHead(Allocator* alloc);
	void* allocator_getTail(Allocator* alloc);
	void* allocator_getTail_noIterUpdate(Allocator* alloc);
	void* allocator_getNext(Allocator* alloc);
	void* allocator_getPrev(Allocator* alloc);

	// Ref counting.
	void allocator_addRef(Allocator* alloc);
	void allocator_release(Allocator* alloc);
	s32  allocator_getRefCount(Allocator* alloc);
}
#include "allocator.h"
#include <TFE_System/system.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_Game/igame.h>
#include <assert.h>

struct AllocHeader
{
	AllocHeader* prev;
	AllocHeader* next;
};

struct Allocator
{
	Allocator*   self;
	AllocHeader* head;
	AllocHeader* tail;
	AllocHeader* iterPrev;
	AllocHeader* iter;
	MemoryRegion* region;
	s32 size;
	s32 refCount;

	// TFE
	AllocHeader* iterSave;
	AllocHeader* iterPrevSave;
};

namespace TFE_Jedi
{
	// Note that the invalid pointer value is chosen such that
	// InvalidPointer + sizeof(AllocHeader) = 0 = null
	// This is hardcoded in DF, here I calculate it based off of size_t so that it works for both 32-bit and 64-bit code.
	static const size_t c_invalidPtr = (~size_t(0)) - sizeof(AllocHeader) + 1;
	#define ALLOC_INVALID_PTR ((AllocHeader*)c_invalidPtr)
	#define MAX_ALLOC_SIZE (8*1024*1024)  // 8MB

	// Create and free an allocator.
	Allocator* allocator_create(s32 allocSize, MemoryRegion* region)
	{
		if (allocSize > MAX_ALLOC_SIZE || allocSize <= 0)
		{
			TFE_System::logWrite(LOG_ERROR, "Allocator", "Invalid allocator size: %d", allocSize);
			assert(0);
			return nullptr;
		}
		region = region ? region : s_levelRegion;	// If a null region is passed in, assume we want the level region.
		Allocator* res = (Allocator*)TFE_Memory::region_alloc(region, sizeof(Allocator));
		if (!res)
		{
			TFE_System::logWrite(LOG_ERROR, "Allocator", "Could not allocate Allocator.");
			assert(0);
			return nullptr;
		}
		res->self = res;
		res->region = region;

		// the original code used special bit patterns to represent invalid pointers.
		res->head = ALLOC_INVALID_PTR;
		res->tail = ALLOC_INVALID_PTR;
		res->iterPrev = ALLOC_INVALID_PTR;
		res->iter = ALLOC_INVALID_PTR;
		res->size = allocSize + sizeof(AllocHeader);
		res->refCount = 0;

		return res;
	}

	void allocator_free(Allocator* alloc)
	{
		if (!alloc) { return; }

		void* item = allocator_getHead(alloc);
		while (item)
		{
			allocator_deleteItem(alloc, item);
			item = allocator_getNext(alloc);
		}

		alloc->self = (Allocator*)ALLOC_INVALID_PTR;
		TFE_Memory::region_free(alloc->region, alloc);
	}

	bool allocator_validate(Allocator* alloc)
	{
		return alloc->self == alloc;
	}

	// Allocate and free individual items.
	void* allocator_newItem(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }

		AllocHeader* header = (AllocHeader*)TFE_Memory::region_alloc(alloc->region, alloc->size);
		if (!header)
		{
			TFE_System::logWrite(LOG_ERROR, "Allocator", "allocator_newItem - cannot allocate header of size %d", alloc->size);
			assert(0);
			return nullptr;
		}

		header->next = ALLOC_INVALID_PTR;
		header->prev = alloc->tail;

		if (alloc->tail != ALLOC_INVALID_PTR)
		{
			alloc->tail->next = header;
		}
		alloc->tail = header;

		if (alloc->head == ALLOC_INVALID_PTR)
		{
			alloc->head = header;
		}

		return ((u8*)header + sizeof(AllocHeader));
	}

	void allocator_deleteItem(Allocator* alloc, void* item)
	{
		if (!alloc) { return; }

		AllocHeader* header = (AllocHeader*)((u8*)item - sizeof(AllocHeader));
		AllocHeader* prev = header->prev;
		AllocHeader* next = header->next;

		if (prev == ALLOC_INVALID_PTR) { alloc->head = next; }
		else { prev->next = next; }

		if (next == ALLOC_INVALID_PTR) { alloc->tail = prev; }
		else { next->prev = prev; }

		if (alloc->iter == header)
		{
			alloc->iter = header->prev;
		}
		if (alloc->iterPrev == header)
		{
			alloc->iterPrev = header->next;
		}

		TFE_Memory::region_free(alloc->region, header);
	}

	// Random access.
	s32 allocator_getCount(Allocator* alloc)
	{
		if (!alloc) { return 0; }

		s32 count = 0;
		AllocHeader* header = alloc->head;
		while (header != ALLOC_INVALID_PTR)
		{
			count++;
			header = header->next;
		}
		return count;
	}
		
	s32 allocator_getCurPos(Allocator* alloc)
	{
		if (!alloc) { return -1; }

		s32 index = 0;
		AllocHeader* iter = alloc->iter;
		AllocHeader* header = alloc->head;
		while (header != ALLOC_INVALID_PTR)
		{
			if (header == iter)
			{
				return index;
			}
			index++;
			header = header->next;
		}
		return -1;
	}

	void allocator_setPos(Allocator* alloc, s32 pos)
	{
		s32 index = 0;
		AllocHeader* header = alloc->head;
		while (header != ALLOC_INVALID_PTR)
		{
			if (index == pos)
			{
				alloc->iter = header;
				break;
			}
			index++;
			header = header->next;
		}
	}
		
	s32 allocator_getPrevPos(Allocator* alloc)
	{
		if (!alloc) { return -1; }

		s32 index = 0;
		AllocHeader* iterPrev = alloc->iterPrev;
		AllocHeader* header = alloc->head;
		while (header != ALLOC_INVALID_PTR)
		{
			if (header == iterPrev)
			{
				return index;
			}
			index++;
			header = header->next;
		}
		return -1;
	}

	void allocator_setPrevPos(Allocator* alloc, s32 pos)
	{
		s32 index = 0;
		AllocHeader* header = alloc->head;
		while (header != ALLOC_INVALID_PTR)
		{
			if (index == pos)
			{
				alloc->iterPrev = header;
				break;
			}
			index++;
			header = header->next;
		}
	}

	s32 allocator_getIndex(Allocator* alloc, void* item)
	{
		if (!item) { return -1; }

		AllocHeader* header = alloc->head;
		s32 index = 0;
		while (header != ALLOC_INVALID_PTR)
		{
			if ((u8*)item == (u8*)header + sizeof(AllocHeader))
			{
				return index;
			}
			index++;
			header = header->next;
		}
		return -1;
	}

	void* allocator_getByIndex(Allocator* alloc, s32 index)
	{
		if (!alloc) { return nullptr; }

		AllocHeader* header = alloc->head;
		while (index > 0 && header != ALLOC_INVALID_PTR)
		{
			index--;
			header = header->next;
		}

		alloc->iterPrev = header;
		alloc->iter = header;
		return (u8*)header + sizeof(AllocHeader);
	}

	// Iteration
	void allocator_saveIter(Allocator* alloc)
	{
		alloc->iterPrevSave = alloc->iterPrev;
		alloc->iterSave = alloc->iter;
	}

	void allocator_restoreIter(Allocator* alloc)
	{
		alloc->iterPrev = alloc->iterPrevSave;
		alloc->iter = alloc->iterSave;
	}

	void* allocator_getHead(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }

		alloc->iterPrev = alloc->head;
		alloc->iter = alloc->head;
		return (u8*)alloc->iter + sizeof(AllocHeader);
	}

	void* allocator_getHead_noIterUpdate(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }
		return (u8*)alloc->head + sizeof(AllocHeader);
	}

	void* allocator_getTail(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }

		alloc->iterPrev = alloc->tail;
		alloc->iter = alloc->tail;

		return (u8*)alloc->tail + sizeof(AllocHeader);
	}

	void* allocator_getTail_noIterUpdate(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }
		return (u8*)alloc->tail + sizeof(AllocHeader);
	}

	void* allocator_getNext(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }

		AllocHeader* iter = alloc->iter;
		if (iter != ALLOC_INVALID_PTR)
		{
			alloc->iter = iter->next;
			alloc->iterPrev = iter->next;
			return (u8*)iter->next + sizeof(AllocHeader);
		}

		iter = alloc->head;
		alloc->iter = iter;
		alloc->iterPrev = iter;
		return (u8*)iter + sizeof(AllocHeader);
	}

	void* allocator_getPrev(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }

		AllocHeader* iterPrev = alloc->iterPrev;
		if (iterPrev != ALLOC_INVALID_PTR)
		{
			AllocHeader* prev = iterPrev->prev;
			alloc->iter = iterPrev->prev;
			alloc->iterPrev = iterPrev->prev;
			return (u8*)iterPrev->prev + sizeof(AllocHeader);
		}

		alloc->iter = alloc->tail;
		alloc->iterPrev = alloc->tail;
		return (u8*)alloc->tail + sizeof(AllocHeader);
	}

	// Ref counting.
	void allocator_addRef(Allocator* alloc)
	{
		if (!alloc) { return; }
		alloc->refCount++;
	}

	void allocator_release(Allocator* alloc)
	{
		if (!alloc) { return; }
		alloc->refCount--;
	}

	s32 allocator_getRefCount(Allocator* alloc)
	{
		return alloc ? alloc->refCount : 0;
	}
}
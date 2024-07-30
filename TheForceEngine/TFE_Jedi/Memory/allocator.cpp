#include "allocator.h"
#include <TFE_System/system.h>
#include <TFE_Game/igame.h>
#include <cstring>

struct AllocHeader
{
	AllocHeader* prev;
	AllocHeader* next;
	char data[];		// actual data storage area.
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

// given an "item" (=allocheader->data), get the "AllocHeader" it belongs to.
template<class P, class C>C* _AllocHeader_of(P ptr)
{
	const size_t off = (size_t)&((reinterpret_cast<C*>(0))->data);
	return (C*)((char*)ptr - off);
}
#define AllocHeader_of(item) _AllocHeader_of<void*, AllocHeader>(item)

// safely get the "AllocHeader->data".
#define GET_DATA(x) ((x) ? &((x)->data) : nullptr)


namespace TFE_Jedi
{
	#define MAX_ALLOC_SIZE (8*1024*1024)  // 8MB

	// Create and free an allocator.
	Allocator* allocator_create(s32 allocSize, MemoryRegion* region)
	{
		if (allocSize > MAX_ALLOC_SIZE || allocSize <= 0)
		{
			TFE_System::logWrite(LOG_ERROR, "Allocator", "Invalid allocator size: %d", allocSize);
			return nullptr;
		}
		region = region ? region : s_levelRegion;       // If a null region is passed in, assume we want the level region.
		Allocator* res = (Allocator*)TFE_Memory::region_alloc(region, sizeof(Allocator));
		if (!res)
		{
			TFE_System::logWrite(LOG_ERROR, "Allocator", "Could not allocate Allocator.");
			return nullptr;
		}
		memset(res, 0, sizeof(Allocator));
		res->self = res;
		res->region = region;
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

		alloc->self = nullptr;
		TFE_Memory::region_free(alloc->region, alloc);
	}

	bool allocator_validate(Allocator* alloc)
	{
		return alloc ? alloc->self == alloc : false;
	}

	// Allocate and free individual items.
	void* allocator_newItem(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }

		AllocHeader* header = (AllocHeader*)TFE_Memory::region_alloc(alloc->region, alloc->size);
		if (!header)
		{
			TFE_System::logWrite(LOG_ERROR, "Allocator", "allocator_newItem - cannot allocate header of size %d", alloc->size);
			return nullptr;
		}
		memset(header, 0, alloc->size);

		header->next = nullptr;
		header->prev = alloc->tail;

		if (alloc->tail)
		{
			alloc->tail->next = header;
		}
		alloc->tail = header;

		if (alloc->head == nullptr)
		{
			alloc->head = header;
		}

		return GET_DATA(header);
	}

	void allocator_deleteItem(Allocator* alloc, void* item)
	{
		if (!alloc || !item) { return; }

		AllocHeader* header = AllocHeader_of(item);
		if (header == nullptr) { return; }

		AllocHeader* prev = header->prev;
		AllocHeader* next = header->next;

		if (prev == nullptr) { alloc->head = next; }
		else { prev->next = next; }

		if (next == nullptr) { alloc->tail = prev; }
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
		while (header)
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
		while (header)
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
		if (!alloc) { return; }

		s32 index = 0;
		AllocHeader* header = alloc->head;
		alloc->iter = nullptr;
		while (header)
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
		while (header)
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
		if (!alloc) { return; }

		s32 index = 0;
		AllocHeader* header = alloc->head;
		while (header)
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
		if (!item || !alloc) { return -1; }

		AllocHeader* header = alloc->head;
		s32 index = 0;
		while (header)
		{
			if (item == &(header->data))
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
		while (index > 0 && header)
		{
			index--;
			header = header->next;
		}

		alloc->iterPrev = header;
		alloc->iter = header;
		return GET_DATA(header);
	}

	// Iteration
	void allocator_saveIter(Allocator* alloc)
	{
		if (!alloc) { return; }
		alloc->iterPrevSave = alloc->iterPrev;
		alloc->iterSave = alloc->iter;
	}

	void allocator_restoreIter(Allocator* alloc)
	{
		if (!alloc) { return; }
		alloc->iterPrev = alloc->iterPrevSave;
		alloc->iter = alloc->iterSave;
	}

	void* allocator_getIter(Allocator* alloc)
	{
		return alloc ? GET_DATA(alloc->iter) : nullptr;
	}

	void allocator_setIter(Allocator* alloc, void* iter)
	{
		if (alloc && iter)
			alloc->iter = AllocHeader_of(iter);
	}

	void* allocator_getHead(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }

		alloc->iterPrev = alloc->head;
		alloc->iter = alloc->head;
		return GET_DATA(alloc->iter);
	}

	void* allocator_getHead_noIterUpdate(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }
		return GET_DATA(alloc->head);
	}

	void* allocator_getTail(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }

		alloc->iterPrev = alloc->tail;
		alloc->iter = alloc->tail;

		return GET_DATA(alloc->tail);
	}

	void* allocator_getTail_noIterUpdate(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }
		return GET_DATA(alloc->tail);
	}

	void* allocator_getNext(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }

		AllocHeader* iter = alloc->iter;
		if (iter)
		{
			alloc->iter = iter->next;
			alloc->iterPrev = iter->next;
			return GET_DATA(iter->next);
		}

		iter = alloc->head;
		if (iter == nullptr) { return nullptr; }

		alloc->iter = iter;
		alloc->iterPrev = iter;
		return GET_DATA(iter);
	}

	void* allocator_getPrev(Allocator* alloc)
	{
		if (!alloc) { return nullptr; }

		AllocHeader* iterPrev = alloc->iterPrev;
		if (iterPrev)
		{
			AllocHeader* prev = iterPrev->prev;
			alloc->iter = iterPrev->prev;
			alloc->iterPrev = iterPrev->prev;
			return GET_DATA(iterPrev->prev);
		}

		alloc->iter = alloc->tail;
		alloc->iterPrev = alloc->tail;
		return GET_DATA(alloc->tail);
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

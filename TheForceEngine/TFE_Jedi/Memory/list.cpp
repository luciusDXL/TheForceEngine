#include <cstring>

#include "list.h"
#include <TFE_Game/igame.h>

namespace TFE_Jedi
{
	void initializeList(List* list);

	u8* list_getNext(List* list)
	{
		u8* iter = list->iter;
		u8* end  = list->end;
		s32 step = list->step;

		// Search for the next item where the first byte is non-zero.
		while (iter < end)
		{
			iter += step;
			if (*iter)
			{
				list->iter = iter;
				return iter + 1;
			}
		}

		return nullptr;
	}

	u8* list_getHead(List* list)
	{
		u8* head = list->head;
		list->iter = list->head;
		// If the current entry is not filled, get the next one.
		if (!(head[0] & 1))
		{
			return list_getNext(list);
		}
		return head + 1;
	}

	void list_removeItem(List* list, void* item)
	{
		if (!list || !item) { return; }

		u8* data = (u8*)item - 1;
		// Already freed.
		if (!data[0]) { return; }

		// Free the item and reduce the count.
		data[0] = 0;
		list->count--;

		// The original method of finding the next free won't really work today, so...
		u8* nextFree = list->head;
		for (s32 i = 0; i < list->capacity; i++, nextFree += list->step)
		{
			if (!nextFree[0])
			{
				list->nextFree = nextFree;
				break;
			}
		}
	}

	u8* list_addItem(List* list)
	{
		u8* nextFree = list->nextFree;
		if (nextFree)
		{
			u8* newItem = nextFree;
			// By marking the first byte with 1, we signal that it is now used.
			newItem[0] = 1;

			list->count++;
			if (list->count >= list->capacity)
			{
				// This is the last available free item, so mark the nextFree as null.
				nextFree = nullptr;
			}
			else
			{
				// Keep stepping forward until another free slot is found.
				while (*nextFree)
				{
					nextFree += list->step;
				}
			}

			list->nextFree = nextFree;
			return newItem + 1;
		}

		// TODO(Core Game Loop Release)
		// Handle Error
		return nullptr;
	}

	List* list_allocate(s32 elemSize, s32 capacity)
	{
		elemSize++;
		s32 size = elemSize * capacity + sizeof(List);
		List* list = (List*)game_alloc(size);
		u8* end = (u8*)list + size;
		list->end = end - elemSize;
		list->self = list;

		u8* start = (u8*)list + sizeof(List);
		list->head = start;
		list->step = elemSize;
		list->capacity = capacity;

		initializeList(list);
		return list;
	}

	void list_clear(List* list)
	{
		s32 size = list->step * list->capacity + sizeof(List);
		u8* end = (u8*)list + size;
		list->end = end - list->step;
		list->self = list;

		u8* start = (u8*)list + sizeof(List);
		list->head = start;

		initializeList(list);
	}

	void initializeList(List* list)
	{
		list->iter = list->head;
		list->nextFree = list->head;
		list->count = 0;

		s32 size = s32(list->end - list->head + list->step);
		memset(list->head, 0, size);
	}

}
#pragma once
//////////////////////////////////////////////////////////////////////
// List API as found in the Jedi Engine
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

struct List
{
	List* self;

	u8* head;
	u8* end;
	u8* iter;
	s32 step;
	u8* nextFree;
	s32 count;
	s32 capacity;
};

namespace TFE_Jedi
{
	u8* list_getNext(List* list);
	u8* list_getHead(List* list);
	u8* list_addItem(List* list);
	void list_removeItem(List* list, void* item);
	List* list_allocate(s32 elemSize, s32 capacity);
}
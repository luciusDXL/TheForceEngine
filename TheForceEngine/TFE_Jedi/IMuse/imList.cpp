#include "imlist.h"
#include "imuse.h"
#include <TFE_System/system.h>

namespace TFE_Jedi
{
	s32 ImListAdd(ImList** list, ImList* item)
	{
		if (!item || item->next || item->prev)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "List arg err when adding");
			return imArgErr;
		}

		ImList* next = *list;
		item->next = next;
		if (next)
		{
			next->prev = item;
		}
		item->prev = nullptr;

		*list = item;
		return imSuccess;
	}

	s32 ImListRemove(ImList** list, ImList* item)
	{
		ImList* curItem = *list;
		if (!item || !curItem)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "List arg err when removing.");
			return imArgErr;
		}
		while (curItem && item != curItem)
		{
			curItem = curItem->next;
		}
		if (!curItem)
		{
			TFE_System::logWrite(LOG_ERROR, "iMuse", "Item not on list.");
			return imNotFound;
		}

		if (item->next)
		{
			item->next->prev = item->prev;
		}
		if (item->prev)
		{
			item->prev->next = item->next;
		}
		else
		{
			*list = item->next;
		}
		item->next = nullptr;
		item->prev = nullptr;

		return imSuccess;
	}
}  // namespace TFE_Jedi

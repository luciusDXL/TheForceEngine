#include "logic.h"
#include <TFE_Memory/allocator.h>

using namespace TFE_Memory;

namespace TFE_DarkForces
{
	void obj_addLogic(SecObject* obj, Logic* logic, Task* logicTask, LogicFunc func)
	{
		if (!obj->logic)
		{
			obj->logic = allocator_create(sizeof(Logic**));
		}

		Logic** logicItem = (Logic**)allocator_newItem((Allocator*)obj->logic);
		*logicItem = logic;
		logic->obj = obj;
		logic->parent = logicItem;
		logic->task = logicTask;
		logic->func = func;
	}
}  // TFE_DarkForces
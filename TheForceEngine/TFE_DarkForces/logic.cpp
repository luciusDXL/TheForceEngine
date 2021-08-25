#include "logic.h"
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Memory/allocator.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	void obj_addLogic(SecObject* obj, Logic* logic, Task* task, LogicCleanupFunc cleanupFunc)
	{
		if (!obj->logic)
		{
			obj->logic = allocator_create(sizeof(Logic**));
		}

		Logic** logicItem = (Logic**)allocator_newItem((Allocator*)obj->logic);
		*logicItem = logic;
		logic->obj = obj;
		logic->parent = logicItem;
		logic->task = task;
		logic->cleanupFunc = cleanupFunc;
	}

	void deleteLogicAndObject(Logic* logic)
	{
		if (s_freeObjLock) { return; }

		SecObject* obj = logic->obj;
		Logic** parent = logic->parent;
		Logic** logics = (Logic**)allocator_getHead_noIterUpdate((Allocator*)obj->logic);

		allocator_deleteItem((Allocator*)obj->logic, parent);
		if (parent == logics)
		{
			freeObject(obj);
		}
	}
}  // TFE_DarkForces
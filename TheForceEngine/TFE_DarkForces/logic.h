#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Logic - 
// The base Logic structure and functionality.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Level/rsector.h>
#include <TFE_Level/robject.h>

typedef void(*LogicFunc)();

namespace TFE_DarkForces
{
	struct Task;

	struct Logic
	{
		RSector* sector;
		s32 u04;
		SecObject* obj;
		Logic** parent;
		Task* task;
		LogicFunc func;
	};

	void obj_addLogic(SecObject* obj, Logic* logic, Task* logicTask, LogicFunc func);
}  // namespace TFE_DarkForces
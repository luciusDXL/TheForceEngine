#include "logic.h"
#include "pickup.h"
#include "player.h"
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Memory/allocator.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	typedef u32(*LogicSetupFunc)(Logic*, KEYWORD);

	static char s_objSeqArg0[256];
	static char s_objSeqArg1[256];
	static char s_objSeqArg2[256];
	static char s_objSeqArg3[256];
	static char s_objSeqArg4[256];
	static char s_objSeqArg5[256];
	static s32  s_objSeqArgCount;

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

	JBool logic_defaultSetupFunc(SecObject* obj, KEYWORD key)
	{
		char* endPtr;
		JBool retValue = JTRUE;
		if (key == KW_EYE)
		{
			player_setupEyeObject(obj);
			//setCameraOffset(0, 0, 0);
			//setCameraAngleOffset(0, 0, 0);
		}
		else if (key == KW_BOSS)
		{
			obj->flags |= OBJ_FLAG_BOSS;
		}
		else if (key == KW_HEIGHT)
		{
			obj->worldHeight = floatToFixed16(strtof(s_objSeqArg1, &endPtr));
		}
		else if (key == KW_RADIUS)
		{
			obj->worldWidth = floatToFixed16(strtof(s_objSeqArg1, &endPtr));
		}
		else  // Invalid key.
		{
			retValue = JFALSE;
		}
		return retValue;
	}

	JBool object_parseSeq(SecObject* obj, TFE_Parser* parser, size_t* bufferPos)
	{
		LogicSetupFunc setupFunc = nullptr;

		const char* line = parser->readLine(*bufferPos);
		if (!line || !strstr(line, "SEQ"))
		{
			return JFALSE;
		}

		Logic* newLogic = nullptr;
		while (1)
		{
			line = parser->readLine(*bufferPos);
			s_objSeqArgCount = sscanf(line, " %s %s %s %s %s %s", s_objSeqArg0, s_objSeqArg1, s_objSeqArg2, s_objSeqArg3, s_objSeqArg4, s_objSeqArg5);
			KEYWORD key = getKeywordIndex(s_objSeqArg0);
			if (key == KW_TYPE || key == KW_LOGIC)
			{
				KEYWORD logicId = getKeywordIndex(s_objSeqArg1);
				if (logicId == KW_PLAYER)
				{
					player_setupObject(obj);
					setupFunc = nullptr;
				}
				else if (logicId == KW_ANIM)
				{
				}
				else if (logicId == KW_UPDATE)
				{
				}
				else if (logicId >= KW_TROOP && logicId <= KW_BARREL)
				{
				}
				else if (logicId == KW_LAND_MINE)
				{
				}
				else if (logicId == KW_KEY)
				{
				}
				else if (logicId == KW_GENERATOR)
				{
				}
				else if (logicId == KW_DISPATCH)
				{
				}
				else if ((logicId >= KW_BATTERY && logicId <= KW_AUTOGUN) || logicId == KW_ITEM)
				{
					if (logicId >= KW_BATTERY && logicId <= KW_AUTOGUN)
					{
						strcpy(s_objSeqArg2, s_objSeqArg1);
					}
					ItemId itemId = getPickupItemId(s_objSeqArg2);
					obj_createPickup(obj, itemId);
					setupFunc = nullptr;
				}
			}
			else if (key == KW_SEQEND)
			{
				return JTRUE;
			}
			else
			{
				if (setupFunc && setupFunc(newLogic, key))
				{
					continue;
				}
				logic_defaultSetupFunc(obj, key);
			}
		}

		return JTRUE;
	}
}  // TFE_DarkForces
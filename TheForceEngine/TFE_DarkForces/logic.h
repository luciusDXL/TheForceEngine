#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Logic - 
// The base Logic structure and functionality.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_System/parser.h>

struct Logic;
typedef void(*LogicCleanupFunc)(Logic*);
typedef JBool(*LogicSetupFunc)(Logic*, KEYWORD);

struct Logic
{
	RSector* sector;
	s32 u04;
	SecObject* obj;
	Logic** parent;
	Task* task;
	LogicCleanupFunc cleanupFunc;
};

namespace TFE_DarkForces
{		
	void obj_addLogic(SecObject* obj, Logic* logic, Task* task, LogicCleanupFunc cleanupFunc);
	void deleteLogicAndObject(Logic* logic);
	JBool object_parseSeq(SecObject* obj, TFE_Parser* parser, size_t* bufferPos);
	Logic* obj_setEnemyLogic(SecObject* obj, KEYWORD logicId, LogicSetupFunc* setupFunc);
	SecObject* logic_spawnEnemy(const char* waxName, const char* typeName);

	// Shared variables used for loading.
	extern char s_objSeqArg0[];
	extern char s_objSeqArg1[];
	extern char s_objSeqArg2[];
	extern char s_objSeqArg3[];
	extern char s_objSeqArg4[];
	extern char s_objSeqArg5[];
	extern s32  s_objSeqArgCount;
}  // namespace TFE_DarkForces
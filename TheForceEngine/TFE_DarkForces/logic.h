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
#include <TFE_ExternalData/dfLogics.h>
#include <TFE_ForceScript/forceScript.h>

struct Logic;
typedef void(*LogicCleanupFunc)(Logic*);
typedef JBool(*LogicSetupFunc)(Logic*, KEYWORD);

// Added for TFE to help with serialization.
enum LogicType : u32
{
	// AI
	LOGIC_DISPATCH = 0,
	LOGIC_BOBA_FETT,
	LOGIC_DRAGON,
	LOGIC_MOUSEBOT,
	LOGIC_PHASE_ONE,
	LOGIC_PHASE_TWO,
	LOGIC_PHASE_THREE,
	LOGIC_TURRET,
	LOGIC_WELDER,
	// General
	LOGIC_ANIM,
	LOGIC_UPDATE,
	LOGIC_GENERATOR,
	LOGIC_PICKUP,
	LOGIC_PLAYER,
	LOGIC_PROJECTILE,
	LOGIC_VUE,
	
	LOGIC_UNKNOWN,
};

struct Logic
{
	LogicType type = LOGIC_UNKNOWN;			// changed this slot for TFE, for serialization.
	SecObject* obj = nullptr;
	Logic** parent = nullptr;
	Task* task = nullptr;
	LogicCleanupFunc cleanupFunc = nullptr;
};

// TFE: ScriptCall from a logic.
struct LogicScriptCall
{
	void* funcPtr = nullptr;
	s32 argCount;
	TFE_ForceScript::ScriptArg args[TFE_ForceScript::MAX_SCRIPT_CALL_ARG];     // up to 4 arguments passed from the O file, plus an ObjectId in some cases
	char funcName[TFE_ForceScript::MAX_SCRIPT_CALL_NAME_LEN];	// Needed for serialization.
};

enum LogicScriptCallType
{
	SCRIPTCALL_NONE = 0,
	SCRIPTCALL_DEATH,
	SCRIPTCALL_ALERT,
	SCRIPTCALL_PAIN,
	SCRIPTCALL_PICKUP,
};

namespace TFE_DarkForces
{		
	void obj_addLogic(SecObject* obj, Logic* logic, LogicType type, Task* task, LogicCleanupFunc cleanupFunc);
	void deleteLogicAndObject(Logic* logic);
	JBool object_parseSeq(SecObject* obj, TFE_Parser* parser, size_t* bufferPos);
	Logic* obj_setEnemyLogic(SecObject* obj, KEYWORD logicId);
	SecObject* logic_spawnEnemy(const char* waxName, const char* typeName);

	void logic_serialize(Logic*& logic, SecObject* obj, Stream* stream);

	Logic* obj_setCustomActorLogic(SecObject* obj, TFE_ExternalData::CustomActorLogic* customLogic);
	TFE_ExternalData::CustomActorLogic* tryFindCustomActorLogic(const char* logicName);

	LogicScriptCall* logic_getScriptCall(s32 index);
	void logic_clearScriptCalls();
	void logic_serializeScriptCalls(Stream* stream);
	void logic_fixupScriptCalls();

	// Shared variables used for loading.
	extern char s_objSeqArg0[];
	extern char s_objSeqArg1[];
	extern char s_objSeqArg2[];
	extern char s_objSeqArg3[];
	extern char s_objSeqArg4[];
	extern char s_objSeqArg5[];
	extern s32  s_objSeqArgCount;
}  // namespace TFE_DarkForces
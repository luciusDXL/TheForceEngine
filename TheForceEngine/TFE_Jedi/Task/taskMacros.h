#pragma once

#ifndef __FUNCTION__
	#ifndef _WIN32   //*NIX
		#define __FUNCTION__   __func__ 
	#endif
#endif

#define task_begin	\
	ctxBegin();	\
	switch (ctxGetIP()) \
	{	\
	case 0:

#define task_begin_ctx	\
	ctxBegin();	\
	ctxAllocate(sizeof(LocalContext));	\
	switch (ctxGetIP()) \
	{	\
	case 0:

#define task_end \
	} \
	ctxReturn();

#define task_yield(delay) \
	do { itask_yield(delay, __LINE__);	\
	return;	\
	case __LINE__:; \
	} while (0)

#define task_runAndReturn(task, msg) \
	itask_run(task, msg)
	
#define task_callTaskFunc(func)	\
	do { if (ctxCall(func, msg, __LINE__, __FUNCTION__)) { return; } \
	case __LINE__:; \
	} while (0)

#define task_callTaskFuncWithId(func, newId)	\
	do { if (ctxCall(func, newId, __LINE__, __FUNCTION__)) { return; } \
	case __LINE__:; \
	} while (0)

#define taskCtx ((LocalContext*)ctxGet())

#define task_waitWhileIdNotZero(ticks) \
	do \
	{ \
		task_yield(ticks); \
	} while (msg != MSG_RUN_TASK)

#define task_localBlockBegin {
#define task_localBlockEnd }

namespace TFE_Jedi
{
	typedef void(*TaskFunc)(MessageType msg);

	//////////////////////////////////////////
	// Internal functions used by macros.
	//////////////////////////////////////////
	s32 ctxGetIP();
	void ctxAllocate(u32 size);
	void* ctxGet();
	void ctxBegin();
	bool ctxCall(TaskFunc func, MessageType msg, s32 ip, const char* funcName);
	void ctxReturn();
	void itask_yield(Tick delay, s32 ip);
	void itask_run(Task* task, MessageType msg);
}
#pragma once

#define task_begin	\
	ctxBegin();	\
	switch (ctxGetState()) \
	{	\
	case 0:

#define task_begin_ctx	\
	ctxBegin();	\
	ctxAllocate(sizeof(LocalContext));	\
	switch (ctxGetState()) \
	{	\
	case 0:

#define task_end \
	} \
	ctxReturn();

#define task_yield(delay) \
	do { \
	itask_yield(delay, __LINE__);	\
	return;	\
	case __LINE__:; \
	} while (0)

#define task_callTaskFunc(func)	\
	ctxCall(func, id)

#define taskCtx ((LocalContext*)ctxGet())

#define task_waitWhileIdNotZero(ticks) \
	do \
	{ \
		task_yield(ticks); \
	} while (id != 0)

namespace TFE_Jedi
{
	typedef void(*TaskFunc)(s32 id);

	//////////////////////////////////////////
	// Internal functions used by macros.
	//////////////////////////////////////////
	s32 ctxGetState();
	void ctxAllocate(u32 size);
	void* ctxGet();
	void ctxBegin();
	void ctxCall(TaskFunc func, s32 id);
	void ctxReturn();
	void itask_yield(Tick delay, s32 state);
}
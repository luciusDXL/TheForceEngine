#pragma once

#define task_begin	\
	switch (ctxGetState()) \
	{	\
	case 0:

#define task_begin_ctx	\
	ctxAllocate(sizeof(LocalContext));	\
	switch (ctxGetState()) \
	{	\
	case 0:

#define task_loop \
	} \
	itask_loop(id);

#define task_end \
	} \
	itask_end(id);

#define task_yield(delay) \
	do { \
	itask_yield(delay, __LINE__, id);	\
	return;	\
	case __LINE__:; \
	} while (0)

#define taskCtx ((LocalContext*)ctxGet())

namespace TFE_Task
{
	//////////////////////////////////////////
	// Internal functions used by macros.
	//////////////////////////////////////////
	s32 ctxGetState();
	void ctxAllocate(u32 size);
	void* ctxGet();
	void itask_yield(TickSigned delay, s32 state, s32 id);
	void itask_loop(s32 id);
	void itask_end(s32 id);
}
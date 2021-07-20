#include "task.h"
#include <TFE_Memory/chunkedArray.h>
#include <TFE_DarkForces/time.h>
#include <stdarg.h>
#include <tuple>
#include <vector>

using namespace TFE_DarkForces;
using namespace TFE_Memory;

struct TaskContext
{
	s32 state;
	TaskFunc func;
	u8* ctx;
	u32 ctxSize;
};

struct Task
{
	Task* prevMain;
	Task* nextMain;
	Task* prevSec;
	Task* nextSec;
	Task* retTask;			// Task to return to once this one is completed or paused.
	
	// Used in place of stack memory.
	TaskContext context;

	// Timing.
	Tick nextTick;
	s32 activeIndex;
};

namespace TFE_Task
{
	enum
	{
		MAX_ACTIVE_TASKS = 1024,
		TASK_CHUNK_SIZE = 256,
		TASK_PREALLOCATED_CHUNKS = 1,
	};

	ChunkedArray* s_tasks = nullptr;
	Task* s_activeTasks[MAX_ACTIVE_TASKS];
	Task* s_curTask = nullptr;
	TaskContext* s_curContext = nullptr;
	s32 s_activeCount = 0;

	Task* s_frameTasks[MAX_ACTIVE_TASKS];
	s32 s_frameTaskCount = 0;
	s32 s_frameTaskIndex = 0;

	Task  s_rootTask;
	Task* s_taskIter;

	void setNextTask(Task* task, s32 id);

	void createRootTask()
	{
		s_tasks = createChunkedArray(sizeof(Task), TASK_CHUNK_SIZE, TASK_PREALLOCATED_CHUNKS);

		s_rootTask = { 0 };
		s_rootTask.prevMain = &s_rootTask;
		s_rootTask.nextMain = &s_rootTask;
		s_rootTask.nextTick = TASK_SLEEP;

		s_taskIter = &s_rootTask;
		s_curTask  = &s_rootTask;
	}
	
	Task* createTask(TaskFunc func)
	{
		Task* newTask = (Task*)allocFromChunkedArray(s_tasks);
		assert(newTask);

		newTask->nextMain = s_curTask->prevSec;
		newTask->prevMain = nullptr;
		newTask->nextSec  = s_curTask;
		newTask->prevSec  = nullptr;
		newTask->retTask  = nullptr;
		if (s_curTask->prevSec)
		{
			s_curTask->prevSec->prevMain = newTask;
		}
		s_curTask->prevSec = newTask;

		newTask->nextTick = 0;
		
		newTask->context = { 0 };
		newTask->context.func = func;
		return newTask;
	}

	Task* pushTask(TaskFunc func)
	{
		Task* newTask = (Task*)allocFromChunkedArray(s_tasks);
		assert(newTask);

		// Insert the task after 's_taskIter'
		newTask->nextMain = s_taskIter->nextMain;
		newTask->prevMain = s_taskIter;
		s_taskIter->nextMain = newTask;

		newTask->prevSec = nullptr;
		newTask->nextSec = nullptr;
		newTask->retTask = nullptr;

		newTask->context = { 0 };
		newTask->context.func = func;
		
		return newTask;
	}

	void freeTask(Task* task)
	{
		// TODO

		// Free any memory allocated for the local context.
		free(task->context.ctx);
		// Finally free the task itself from the chunked array.
		freeToChunkedArray(s_tasks, task);
	}

	void freeAllTasks()
	{
		const u32 taskCount = chunkedArraySize(s_tasks);
		for (u32 i = 0; i < taskCount; i++)
		{
			Task* task = (Task*)chunkedArrayGet(s_tasks, i);
			free(task->context.ctx);
		}

		freeChunkedArray(s_tasks);

		s_curTask = nullptr;
		s_tasks = nullptr;
		s_curContext = nullptr;
		s_activeCount = 0;
	}

	// Run a task and then return to the curren task.
	void runTask(Task* task, s32 id)
	{
		if (!task) { return; }
		
		task->retTask = s_curTask;
		setNextTask(task, id);
	}

	void setNextTask(Task* task, s32 id)
	{
		// Assign the task + id as the next to run.
		// TODO
	}
		
	void addTaskToActive(Task* task)
	{
		if (task->activeIndex < 0)
		{
			task->activeIndex = s_activeCount;
			s_activeTasks[s_activeCount++] = task;
		}
	}

	void task_makeActive(Task* task)
	{
		task->nextTick = 0;
	}

	void itask_end(s32 id)
	{
		freeTask(s_curTask);
		// What to do here?
	}

	void itask_yield(Tick delay, s32 state, s32 id)
	{
		// Copy the state so we know where to return.
		s_curContext->state = state;

		// If there is a return task, then take it next.
		if (s_curTask->retTask)
		{
			// Clear out the return task once it is executed.
			Task* retTask = s_curTask->retTask;
			s_curTask->retTask = nullptr;

			// Add the task to be run next.
			setNextTask(retTask, 0);
			return;
		}

		// Update the current tick based on the delay.
		s_curTask->nextTick = (delay < TASK_SLEEP) ? s_curTick + delay : delay;
		
		// Find the next task to run.
		Task* task = s_curTask;
		while (1)
		{
			if (task->nextMain)
			{
				task = task->nextMain;
				while (task->prevSec)
				{
					task = task->prevSec;
				}
				if (task->nextTick <= s_curTick)
				{
					setNextTask(task, 0);
					return;
				}
			}
			else if (task->nextSec)
			{
				task = task->nextSec;
				if (task->nextTick <= s_curTick)
				{
					setNextTask(task, 0);
					return;
				}
			}
			else
			{
				break;
			}
		}
	}
		
	// Called once per frame to run all of the tasks.
	void runTasks()
	{
		// TODO
	}

	s32 ctxGetState()
	{
		return s_curContext->state;
	}

	void ctxAllocate(u32 size)
	{
		if (!size) { return; }
		if (s_curContext->ctxSize)
		{
			assert(s_curContext->ctx);
			return;
		}

		s_curContext->ctxSize = size;
		s_curContext->ctx = (u8*)malloc(size);
		memset(s_curContext->ctx, 0, size);
	}

	void* ctxGet()
	{
		return s_curContext->ctx;
	}

	////////////////////////////////////////////////////////////
	// Test adding the ability to call sub-functions with
	// the ability to yield
	////////////////////////////////////////////////////////////
	enum { TASK_MAX_CALLSTACK = 4, MAX_TASK_ARG = 16 };
	typedef union
	{
		s32 iValue;
		f32 fValue;
	} TaskArg;
	typedef void(*TaskCall)(u32, TaskArg*);
	struct Call
	{
		u32 state;
		u32 argCount;
		TaskArg args[MAX_TASK_ARG];
		TaskCall call;
	};
	struct Context2
	{
		u32 state;
		u32 callstackCount;
		Call callstack[TASK_MAX_CALLSTACK];
	};
	static Context2* s_context2;

	void itask_call(TaskCall func, u32 state, s32 argCount, ...)
	{
		s32 csCount = s_context2->callstackCount;
		s_context2->callstackCount++;

		s_context2->callstack[csCount].call = func;
		s_context2->callstack[csCount].argCount = 0;
		s_context2->callstack[csCount].state = 0;
		if (csCount)
		{
			s32 prevIndex = csCount - 1;
			s_context2->callstack[prevIndex].state = state;
		}
		else
		{
			s_context2->state = state;
		}

		va_list valist;
		va_start(valist, argCount);
		s_context2->callstack[csCount].argCount = argCount;
		for (s32 i = 0; i < argCount; i++)
		{
			va_arg(valist, TaskArg);
		}
		va_end(valist);

		func(argCount, s_context2->callstack[csCount].args);
	}

// C++ macro magic to get this to be reliable.
#define NUMARGS(...) std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value

#define task_call(func, ...) \
	do { \
	itask_call(func, __LINE__, NUMARGS(__VA_ARGS__), __VA_ARGS__);	\
	if (s_context2->callstackCount) return;  \
	case __LINE__:; \
	} while (0)

	void taskCallTest(u32 argCount, TaskArg* args)
	{
		// Extract arguments.
		assert(argCount == 3);
		s32 x = args[0].iValue;
		s32 y = args[1].iValue;
		s32 z = args[2].iValue;

		// Continue as normal.
	}

	void testFunc(s32 id)
	{
		task_begin;

		task_call(taskCallTest);
		task_call(taskCallTest, 0, 1, 2);

		task_end;
	}
}
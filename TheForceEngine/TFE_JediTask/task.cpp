#include "task.h"
#include <TFE_Memory/chunkedArray.h>
#include <vector>

using namespace TFE_Memory;

struct TaskContext
{
	s32 state;
	u8* ctx;
	u32 ctxSize;
};

struct Task
{
	TaskFunc func;
	TaskContext context;
	fixed16_16 delay;		// delay before the next call.
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

	s32 ctxGetState();
	void ctxAllocate(u32 size);
	void* ctxGet();
	void end();

	Task* createTask(TaskFunc func)
	{
		if (!s_tasks)
		{
			s_tasks = createChunkedArray(sizeof(Task), TASK_CHUNK_SIZE, TASK_PREALLOCATED_CHUNKS);
		}

		Task* newTask = (Task*)allocFromChunkedArray(s_tasks);
		assert(newTask);

		newTask->activeIndex = -1;
		newTask->delay = 0;
		newTask->func = func;
		memset(&newTask->context, 0, sizeof(newTask->context));

		newTask->context.ctxSize = 0;
		newTask->context.ctx = nullptr;

		return newTask;
	}

	Task* pushTask(TaskFunc func)
	{
		Task* newTask = createTask(func);
		newTask->activeIndex = s_activeCount;
		s_activeTasks[s_activeCount++] = newTask;

		return newTask;
	}

	void popTask()
	{
		s_activeCount--;
	}

	void freeTask(Task* task)
	{
		// Remove from the task from the active list, the function is finished.
		if (task->activeIndex)
		{
			if (task->activeIndex == s_activeCount - 1)
			{
				s_activeCount--;
			}
			else
			{
				s_activeTasks[task->activeIndex] = nullptr;
			}
		}
		// Cleanup the active list if possible.
		while (s_activeCount && !s_activeTasks[s_activeCount - 1])
		{
			s_activeCount--;
		}

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
		memset(s_activeTasks, 0, sizeof(Task*) * s_activeCount);

		s_curTask = nullptr;
		s_tasks = nullptr;
		s_curContext = nullptr;
		s_activeCount = 0;
	}

	void runNextTask(u32 id)
	{
		s_curTask = nullptr;
		s_curContext = nullptr;
		if (s_frameTaskIndex >= s_frameTaskCount)
		{
			return;
		}

		s32 index = s_frameTaskIndex;
		s_frameTaskIndex++;

		runTask(s_frameTasks[index], id);
	}

	void runTask(Task* task, u32 id)
	{
		if (!task) { return; }
		s_curTask = task;
		s_curContext = &task->context;

		assert(task->func);
		task->func(id);
	}

	void addCurTaskToActive()
	{
		if (s_curTask->activeIndex < 0)
		{
			s_curTask->activeIndex = s_activeCount;
			s_activeTasks[s_activeCount++] = s_curTask;
		}
	}

	void loop(TickSigned delay)
	{
		s_curTask->delay = delay < 0 ? delay : intToFixed16(delay) / TICKS_PER_SECOND;
		s_curContext->state = 0;

		// If we need to loop and this task isn't in the active list, it will need to get added or it will never get called again...
		addCurTaskToActive();
		// Run the next task.
		runNextTask(0);
	}

	void end()
	{
		freeTask(s_curTask);
		runNextTask(0);
	}

	void yield(TickSigned delay, s32 state)
	{
		s_curTask->delay = delay < 0 ? delay : intToFixed16(delay) / TICKS_PER_SECOND;
		s_curContext->state = state;

		// If we need to loop and this task isn't in the active list, it will need to get added or it will never get called again...
		addCurTaskToActive();
		// Run the next task.
		runNextTask(0);
	}
		
	void runTasks(fixed16_16 dt)
	{
		s_frameTaskCount = 0;
		s_frameTaskIndex = 0;

		for (s32 i = 0; i < s_activeCount; i++)
		{
			if (!s_activeTasks[i]) { continue; }
			
			Task* task = s_activeTasks[i];
			// Update the task delay if greater than 0.
			if (task->delay > 0)
			{
				task->delay = max(0, task->delay - dt);
			}
			// If the delay is not 0, then skip it.
			if (task->delay != 0)
			{
				continue;
			}
			// Otherwise add to the list to call this frame...
			s_frameTasks[s_frameTaskCount++] = task;
		}

		runNextTask(0);
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

	void testFunc(s32 id)
	{
		// First define the local context structure.
		struct LocalContext
		{
			s32 i;
		};

		// Then use TASK_BEGIN_CTX instead of TASK_BEGIN to allocate.
		task_begin_ctx;

		// some code ...
		task_yield(4664);
		id += 3;

		for (taskCtx->i = 0; taskCtx->i < 4; taskCtx->i++)
		{
			task_yield(87);
			id *= 2;
			task_yield(87);
			id -= taskCtx->i;
		}

		task_end;
	}

	void test()
	{
		Task* testTask = createTask(testFunc);
		runTask(testTask, 3);
		freeTask(testTask);
	}
}
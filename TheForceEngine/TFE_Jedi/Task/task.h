#pragma once
//////////////////////////////////////////////////////////////////////
// Task
// Basic "Task" system based on Dark Forces DOS.
// However - this code is greatly simplified, pushing some of the 
// complexity to the task functions.
// The original code used a "coroutine" like task system (similar to
// fibers) - which adds a lot of unrequired complexity (saving stack
// state, yield, delays, etc.).
//
// The TFE version simplifies the system.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_DarkForces/time.h>

struct Task;
#include "taskMacros.h"

enum TaskDelay
{
	TASK_SLEEP = 0xffffffff,
	TASK_NO_DELAY = 0,
};

////////////////////////////////////////////////////////////////////////
// Task System API
namespace TFE_Jedi
{
	Task* createTask(const char* name, TaskFunc func, TaskFunc localRunFunc = nullptr);
	Task* pushTask(const char* name, TaskFunc func, JBool framebreak = JFALSE, TaskFunc localRunFunc = nullptr);

	void  task_free(Task* task);
	void  task_freeAll();
	void  task_shutdown();

	void  task_makeActive(Task* task);
	void  task_setNextTick(Task* task, Tick tick);
	void  task_setUserData(Task* task, void* data);

	void  task_runLocal(Task* task, s32 id);
	
	// Call once per frame to run the tasks.
	// The core idea is to split the original recursive tasks into
	// an iterative loop that can be called into every frame.
	// This interacts better with modern systems, such as Windows.
	// Another option is to run the game and rendering loop in its own
	// thread and then blit the results in the main thread.
	void task_run();
	void task_setDefaults();

	s32 task_getCount();
}
////////////////////////////////////////////////////////////////////////
// Task Function API:
//
// task_begin / task_begin_ctx
//   This must be called at the top of the function
//   after the LocalContext definition.
//   Use task_begin_ctx if persistant local variables
//   are required, otherwise use task_begin
//
// task_end
//   This must be called at the end of the function.
//   Once called, the task will be deleted and removed.
//
// task_yield(delay)
//   Call to return from the function and resume later:
//     TASK_NO_DELAY - next frame.
//     TASK_SLEEP - put to sleep until task_makeActive() is called
//     Other - the delay in ticks before the function is resumed.
//
// taskCtx
//   Access to the persistant local variables, as defined by the
//   LocalContext struct before task_begin_ctx is called.
//   Example: taskCtx->i, for struct LocalContext { s32 i; }
////////////////////////////////////////////////////////////////////////

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
#include <TFE_FileSystem/filestream.h>
#include <TFE_DarkForces/time.h>
#include <TFE_Jedi/InfSystem/message.h>

struct Task;
#include "taskMacros.h"

enum TaskDelay
{
	TASK_SLEEP = 0xffffffff,
	TASK_NO_DELAY = 0,
};
typedef void(*LocalMemorySerCallback)(Stream* stream, void* userData, void* mem);

////////////////////////////////////////////////////////////////////////
// Task System API
namespace TFE_Jedi
{
	// Create a "main" task. Generally, main tasks are executed in the order created.
	Task* createTask(const char* name, TaskFunc func, JBool framebreak = JFALSE, TaskFunc localRunFunc = nullptr);
	// Create a "subtask" under the current task. All sub-tasks under a main task are executed *before* that main task.
	Task* createSubTask(const char* name, TaskFunc func, TaskFunc localRunFunc = nullptr);

	// Serialize task state, such as local memory.
	// This currently assumes "simple" use cases - it does not unwind recursions yet.
	// On load, the task should be created as normal first and then have its state serialized.
	// If local task function state includes pointers or other non-pod data, a local memory serialization callback
	// needs to be provided and the state be properly serialized by the client. The callback should properly handle
	// both saving and loading. See vueLogic_serializeTaskLocalMemory() in TFE_DarkForces/vueLogic.cpp for an example.
	void task_serializeState(Stream* stream, Task* task, void* userData = nullptr, LocalMemorySerCallback localMemCallback = nullptr, bool resetIP = false);

	Task* task_getCurrent();

	void  task_pause(JBool pause, Task* pauseRunTask = nullptr);
	void  task_free(Task* task);
	void  task_freeAll();
	void  task_reset();
	void  task_shutdown();

	void  task_makeActive(Task* task);
	void  task_setNextTick(Task* task, Tick tick);
	void  task_setUserData(Task* task, void* data);
	void  task_setMessage(MessageType msg);
	void* task_getUserData();

	const char* task_getName(Task* task);
	
	void  task_runLocal(Task* task, MessageType msg);
	bool  task_hasLocalMsgFunc(Task* task);
	
	// Call once per frame to run the tasks.
	// The core idea is to split the original recursive tasks into
	// an iterative loop that can be called into every frame.
	// This interacts better with modern systems, such as Windows.
	// Another option is to run the game and rendering loop in its own
	// thread and then blit the results in the main thread.
	// Returns false if tasks cannot be run due to the time interval.
	JBool task_run();
	JBool task_canRun();
	void task_setDefaults();
	void task_setMinStepInterval(f64 minIntervalInSec);

	void task_updateTime();
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

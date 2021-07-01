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

typedef void(*TaskFunc)(s32 id);
struct Task;

/*  EXAMPLE Task function:
void superchargeTaskFunc(u32 id)
{
	// First define the local context structure. If no persistent state is needed, this can be skipped.
	// task_begin_ctx assumes the structure is called 'LocalContext'
	struct LocalContext
	{
		s32 i;
	};
	// A task function must begin with task_begin; or task_begin_ctx;
	task_begin_ctx;

	// Setup supercharged.
	s_superChargeHud = JTRUE;
	s_superCharge = JTRUE;
	
	// Returns and resumes the function in about 45 seconds.
	task_yield(4664);	
	// Then loops 5 times when the powerup is about to run out.
	for (taskCtx->i = 4; taskCtx->i >= 0; taskCtx->i--)
	{
		// Plays an alarm sound, turns off the HUD highlight and than waits 0.6 seconds.
		playSound2D(s_superchargeCountdownSound);
		s_superChargeHud = JFALSE;
		task_yield(87);	

		// Turns the HUD highlight back on and then waits 0.6 seconds.
		s_superChargeHud = JTRUE;
		task_yield(87);
	}
	// Finally turns off the super charge.
	s_superCharge = JFALSE;
	s_superChargeHud = JFALSE;
	// Then clears the supercharge task since the task will be deleted after calling task_end;
	s_superchargeTask = nullptr;

	// Removes the task and frees its memory.
	// If instead task_loop was called, then the task would continue starting from task_begin.
	// A while() loop could be used instead
	task_end;
}
*/

namespace TFE_Task
{
	Task* createTask(TaskFunc func);
	Task* pushTask(TaskFunc func);
	void  popTask();

	void  freeTask(Task* task);
	void  freeAllTasks();

	void  runTask(Task* task, u32 id);
	void  yield(TickSigned delay, s32 state);

	// Call once per frame to run the tasks.
	void runTasks(fixed16_16 dt);
		
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
	loop(0);

#define task_end \
	} \
	end();

#define task_yield(delay) \
	do { \
	yield(delay, __LINE__);	\
	return;	\
	case __LINE__:; \
	} while (0)

	#define taskCtx ((LocalContext*)ctxGet())
}

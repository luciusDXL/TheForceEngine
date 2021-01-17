#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/infAsset.h>
#include <vector>
#include <string>

struct SoundSource;

namespace TFE_InfSystem
{
	enum InfState
	{
		INF_STATE_MOVING = 0,	// Moving to the next stop.
		INF_STATE_WAITING,		// Waiting for a timer to tick down before continuing.
		INF_STATE_HOLDING,		// Holding or waiting to be triggered.
		INF_STATE_TERMINATED,	// Done, nothing more to do here.
		INF_STATE_ACTIVATED,	// Already activated but not terminated (trigger).
		INT_STATE_COUNT
	};

	enum NudgeType
	{
		NUDGE_NONE = 0,
		NUDGE_SET,
		NUDGE_TOGGLE,
	};

	// Runtime state for each INF item.
	struct ItemSlaveState
	{
		f32 initState[2];
		f32 curValue;
	};

	struct ItemState
	{
		InfState state;
		f32 initState[2];	// initial floor height, ceiling height, etc.
		f32 curValue;		// current value.
		// Value state for slaves.
		ItemSlaveState* slaveState;
		SoundSource* moveSound;

		s32 curStop;
		s32 nextStop;
		f32 delay;
	};
}
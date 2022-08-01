#pragma once
//////////////////////////////////////////////////////////////////////
// INF System State
// Internal to the INF system.
// Note - this file should *only* be included by internal cpp files.
//////////////////////////////////////////////////////////////////////
#include "infPublicTypes.h"
#include <TFE_System/types.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_Jedi/Math/fixedPoint.h>

namespace TFE_Jedi
{
	struct Stop;

	// State that should be serialized for quick-saves.
	struct InfSerializableState
	{
		s32 activeTriggerCount = 0;
		Allocator* infElevators = nullptr;
		Allocator* infTeleports = nullptr;
		Allocator* infTriggers = nullptr;
	};
	extern InfSerializableState s_infSerState;

	// General state that should be cleared on reset but does not need to be serialized.
	struct InfState
	{
		Task* infElevTask = nullptr;
		Task* infTriggerTask = nullptr;
		Task* teleportTask = nullptr;
		Stop* nextStop = nullptr;
	};
	extern InfState s_infState;
}

#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Pickup - 
// Items that can be picked up such as ammo, keys, extra lives,
// and weapons.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Level/rsector.h>
#include <TFE_Level/robject.h>
#include "itemId.h"

// TODO: Move into Logic.
typedef void(*LogicFunc)();

namespace TFE_DarkForces
{
	struct Logic;
	struct Task;

	// Note that the Pickup struct mirrors the first 24 bytes of Logic {}
	struct Pickup
	{
		RSector* sector;
		s32 u04;
		SecObject* obj;
		Logic** parent;
		Task* task;
		LogicFunc func;

		ItemId id;
		s32 index;
		ItemType type;
		s32* item;
		s32* value;
		s32 amount;
		s32 msgId[2];
		s32 maxAmount;
	};

	ItemId getPickupItemId(const char* keyword);
}  // namespace TFE_DarkForces
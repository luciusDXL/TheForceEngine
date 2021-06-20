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
#include "logic.h"
#include "itemId.h"

enum PickupLogicId
{
	PICKUP_DELETE  = 1,
	PICKUP_ACQUIRE = 24,
};

namespace TFE_DarkForces
{
	// This is a type of "Logic" and can be safety cast to Logic {}.
	struct Pickup
	{
		Logic logic;		// Logic header.
		ItemId id;
		s32 index;
		ItemType type;
		JBool* item;
		s32* value;
		s32 amount;
		s32 msgId[2];
		s32 maxAmount;
	};

	ItemId getPickupItemId(const char* keyword);
	Logic* obj_createPickup(SecObject* obj, ItemId id);

	// Logic update function, called when pickups are handled.
	void pickupLogicFunc(s32 id);
}  // namespace TFE_DarkForces
#pragma once
//////////////////////////
// TFE Custom Logics
//////////////////////////

#include <TFE_System/types.h>

struct CustomActorLogic
{
	const char* logicName;
	bool hasGravity;
	bool isFlying;

	const char* alertSound;
	const char* painSound;
	const char* attack1Sound;
	const char* attack2Sound;
	const char* dieSound;

	u32 hitPoints;
	u32 dropItem;

	bool hasMeleeAttack;
	bool hasRangedAttack;
	bool litWithMeleeAttack;
	bool litWithRangedAttack;
	u32 projectile;
	u32 rangedAttackDelay;
	u32 meleeAttackDelay;
	u32 meleeRange;
	u32 meleeDamage;
	u32 meleeRate;
	u32 minAttackDist;
	u32 maxAttackDist;

	u32 speed;
	u32 verticalSpeed;
};

struct ExternalLogics
{
	std::vector<CustomActorLogic> actorLogics;
};

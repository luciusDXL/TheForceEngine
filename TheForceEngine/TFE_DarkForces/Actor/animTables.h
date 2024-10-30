#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Core Actor/AI functionality.
// -----------------------------
// Shared animation tables for AI
//////////////////////////////////////////////////////////////////////
#include "actorModule.h"
#include "../sound.h"
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include <TFE_Jedi/Collision/collision.h>

namespace TFE_DarkForces
{
	// Accessors for serialization.
	s32 animTables_getIndex(const s32* table);
	const s32* animTables_getTable(s32 index);

	// Animation Tables, used directly by AI actors.
	// Enemies
	extern const s32 s_reeyeesAnimTable[];
	extern const s32 s_bosskAnimTable[];
	extern const s32 s_gamorAnimTable[];
	// Exploders
	extern const s32 s_mineBarrelAnimTable[];
	// Flyers
	extern const s32 s_intDroidAnimTable[];
	extern const s32 s_probeDroidAnimTable[];
	extern const s32 s_remoteAnimTable[];
	// Scenery
	extern const s32 s_sceneryAnimTable[];
	// Sewer Creature
	extern const s32 s_sewerCreatureAnimTable[];
	// Troopers
	extern const s32 s_officerAnimTable[];
	extern const s32 s_troopAnimTable[];
	extern const s32 s_commandoAnimTable[];

	// Custom
	extern const s32 s_customAnimTable[];
}  // namespace TFE_DarkForces
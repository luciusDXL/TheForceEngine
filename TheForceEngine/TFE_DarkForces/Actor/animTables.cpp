#include "animTables.h"

namespace TFE_DarkForces
{
	// Enemies
	const s32 s_reeyeesAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, -1, 7, 8, -1, -1, -1, 12, -1, -1, -1 };
	const s32 s_bosskAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, 6, -1, -1, -1, -1, -1, 12, -1, -1, -1 };
	const s32 s_gamorAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1 };
	// Exploders
	const s32 s_mineBarrelAnimTable[] =
	{ 0, -1, 1, 1, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	// Flyers
	const s32 s_intDroidAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, -1, 7, -1, -1, -1, -1, 12, -1, -1, -1 };
	const s32 s_probeDroidAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1 };
	const s32 s_remoteAnimTable[] =
	{ 0, 0, 2, 3, -1, 0, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1 };
	// Scenery
	const s32 s_sceneryAnimTable[] =
	{ 0, -1, -1, -1, 1, 0, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1 };
	// Sewer Creature
	const s32 s_sewerCreatureAnimTable[] =
	{ -1, 1, 2, 3, 4, 5, 6, -1, -1, -1, -1, -1, -1, -1, 12, 13 };
	// Troopers
	const s32 s_officerAnimTable[] =
	{ 0, 6, 2, 3, 4, 5,  1, -1, -1, -1, -1, -1, 12, -1, -1, -1 };
	const s32 s_troopAnimTable[] =
	{ 0, 1, 3, 2, 4, 5, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1 };
	const s32 s_commandoAnimTable[] =
	{ 0, 1, 2, 3, 4, 5,  6, -1, -1, -1, -1, -1, 12, -1, -1, -1 };

	// For custom sprites - based on enemies but with all anims available
	const s32 s_customAnimTable[] =
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, -1, -1, -1, 12, -1, -1, -1 };

	// Internal State.
	static const s32* const s_animTables[] =
	{
		s_reeyeesAnimTable,
		s_bosskAnimTable,
		s_gamorAnimTable,
		s_mineBarrelAnimTable,
		s_intDroidAnimTable,
		s_probeDroidAnimTable,
		s_remoteAnimTable,
		s_sceneryAnimTable,
		s_sewerCreatureAnimTable,
		s_officerAnimTable,
		s_troopAnimTable,
		s_commandoAnimTable,
		s_customAnimTable,
	};
	static const s32 s_animTableCount = TFE_ARRAYSIZE(s_animTables);

	// Serialization helpers.
	s32 animTables_getIndex(const s32* table)
	{
		if (!table) { return -1; }
		for (s32 i = 0; i < s_animTableCount; i++)
		{
			if (s_animTables[i] == table)
			{
				return i;
			}
		}
		assert(0);
		return -1;
	}

	const s32* animTables_getTable(s32 index)
	{
		if (index < 0 || index >= s_animTableCount)
		{
			return nullptr;
		}
		return s_animTables[index];
	}
}  // namespace TFE_DarkForces
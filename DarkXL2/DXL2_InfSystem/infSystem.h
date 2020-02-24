#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Script System
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelAsset.h>
#include <DXL2_Asset/infAsset.h>
#include <vector>
#include <string>

class Player;

namespace DXL2_InfSystem
{
	bool init();
	void shutdown();

	void setupLevel(InfData* infData, LevelData* levelData);
	void tick();

	void activate(const Vec3f* pos, s32 curSectorId, u32 keys);
	// Returns true if the player should continue falling.
	bool firePlayerEvent(u32 evt, s32 sectorId, Player* player);
	// player shoots a wall.
	void shootWall(const Vec3f* hitPoint, s32 hitSectorId, s32 hitWallId);

	void advanceCompleteElevator();
}

#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/infAsset.h>
#include <vector>
#include <string>

class Player;
struct MultiRayHitInfo;

namespace TFE_InfSystem
{
	bool init();
	void shutdown();

	void setupLevel(InfData* infData, LevelData* levelData);
	void tick();

	void activate(const Vec3f* pos, const MultiRayHitInfo* hitInfo, s32 curSectorId, u32 keys);
	void explosion(const Vec3f* pos, s32 curSectorId, f32 radius);
	// Returns true if the player should continue falling.
	bool firePlayerEvent(u32 evt, s32 sectorId, Player* player);
	bool firePlayerEvent(u32 evt, s32 sectorId, s32 wallId);
	// player shoots a wall.
	void shootWall(const Vec3f* hitPoint, s32 hitSectorId, s32 hitWallId);

	void advanceCompleteElevator();
	void advanceBossElevator();
	void advanceMohcElevator();
}

#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level Runtime
// Handles the runtime management of level data.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelAsset.h>

class Player;

struct SectorBaseHeight
{
	f32 floorAlt;
	f32 ceilAlt;
};

enum SectorPart
{
	SP_WALL = 0,
	SP_SIGN,
	SP_FLOOR,
	SP_CEILING,
	SP_COUNT,
	SP_SECTOR = SP_FLOOR,
};

enum WallSubPart
{
	WSP_TOP = 0,
	WSP_MID,
	WSP_BOT,
	WSP_NONE,
	WSP_COUNT
};

struct LevelObjectData;

namespace DXL2_Level
{
	bool init();
	void shutdown();

	void startLevel(LevelData* level, LevelObjectData* levelObj);
	void endLevel();
		
	// Player (for interactions)
	void setPlayerSector(Player* player, s32 floorCount, s32 secAltCount, const s32* floorId, const s32* secAltId);

	// Animation
	void wakeUp(u32 sectorId);

	// Floor, ceiling, second height
	void setFloorHeight(s32 sectorId, f32 height, bool addSectorMotion = false);
	void setCeilingHeight(s32 sectorId, f32 height);
	void setSecondHeight(s32 sectorId, f32 height, bool addSectorMotion = false);

	f32 getFloorHeight(s32 sectorId);
	f32 getCeilingHeight(s32 sectorId);
	f32 getSecondHeight(s32 sectorId);

	const SectorBaseHeight* getBaseSectorHeight(s32 sectorId);

	// Morph
	void moveWalls(s32 sectorId, f32 angle, f32 distance, f32 deltaDist, bool addSectorMotion = false, bool addSecMotionSecondAlt = false, bool useVertexCache = true);
	void rotate(s32 sectorId, f32 angle, f32 angleDelta, const Vec2f* center, bool addSectorMotion = false, bool addSecMotionSecondAlt = false, bool useVertexCache = true);

	// Textures.
	void moveTextureOffset(s32 sectorId, SectorPart part, const Vec2f* offsetDelta, bool addSectorMotion = false, bool addSecMotionSecondAlt = false);
	void setTextureOffset(s32 sectorId, SectorPart part, const Vec2f* offset, bool addSectorMotion = false, bool addSecMotionSecondAlt = false);
	void setTextureFrame(s32 sectorId, SectorPart part, WallSubPart subpart, u32 frame, s32 wallId = -1);
	void toggleTextureFrame(s32 sectorId, SectorPart part, WallSubPart subpart, s32 wallId = -1);
	void setTextureId(s32 sectorId, SectorPart part, WallSubPart subpart, u32 textureId, s32 wallId = -1);

	Vec2f getTextureOffset(s32 sectorId, SectorPart part, WallSubPart subpart, s32 wallId = -1);
	u32 getTextureFrame(s32 sectorId, SectorPart part, WallSubPart subpart, s32 wallId = -1);
	u32 getTextureId(s32 sectorId, SectorPart part, WallSubPart subpart, s32 wallId = -1);

	// Flags.
	void setFlagBits(s32 sectorId, SectorPart part, u32 flagIndex, u32 flagBits, s32 wallId = -1);
	void clearFlagBits(s32 sectorId, SectorPart part, u32 flagIndex, u32 flagBits, s32 wallId = -1);

	u32  getFlagBits(s32 sectorId, SectorPart part, u32 flagIndex, s32 wallId = -1);

	// Lighting.
	void setAmbient(s32 sectorId, u8 ambient);
	u8 getAmbient(s32 sectorId);
	void turnOnTheLights();

	// Ajoins
	// Link sector1(wall1) to sector2(wall2)
	void changeAdjoin(s32 sector1, s32 wall1, s32 sector2, s32 wall2);
}

#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine View
// This class handles rendering the world from the player's point of
// view ("3D" rendering).
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include "renderCommon.h"

struct LevelData;
struct LevelObjectData;
struct Model;
class DXL2_Renderer;

struct ViewStats
{
	s32 iterCount;
	s32 segWallRendered;
	s32 segLowerRendered;
	s32 segUpperRendered;
	s32 floorPolyRendered;
	s32 ceilPolyRendered;
	s32 maxTraversalDepth;

	Vec3f pos;
	f32 yaw, pitch;
	s32 sectorId;
};

namespace DXL2_View
{
	bool init(const LevelData* level, DXL2_Renderer* renderer, s32 w=320, s32 h=200, bool enableViewStats=true);
	void shutdown();

	void update(const Vec3f* cameraPos, f32 yaw, f32 pitch, s32 sectorId, LightMode lightMode);
	void draw(const Vec3f* cameraPos, s32 sectorId);
	void setIterationOverride(s32 iterMax = 0);

	const ViewStats* getViewStats();
}

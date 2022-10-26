#pragma once
//////////////////////////////////////////////////////////////////////
// Display list for rendering sector geometry
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "sbuffer.h"

namespace TFE_Jedi
{
	// Pack portal info into a 16-bit value.
	#define PACK_PORTAL_INFO(offset, count) (u32(offset) | u32(count) << 13u)
	#define UNPACK_PORTAL_INFO_COUNT(info) (info >> 13u)
	#define UNPACK_PORTAL_INFO_OFFSET(info) (info & ((1u << 13u) - 1u))

	enum SectorPass
	{
		SECTOR_PASS_OPAQUE = 0,
		SECTOR_PASS_TRANS,
		SECTOR_PASS_COUNT
	};

	enum PlaneType
	{
		PLANE_TYPE_TOP = FLAG_BIT(0),
		PLANE_TYPE_BOT = FLAG_BIT(1),
		PLANE_TYPE_BOTH = PLANE_TYPE_TOP | PLANE_TYPE_BOT,
	};

	enum Contants
	{
		MAX_PORTAL_PLANES = 8,  // Minimum custom clipping plane limit for OpenGL 3.3
		MAX_DISP_ITEMS = 8192,
	};

	struct GPUCachedSector
	{
		f32 floorHeight;
		f32 ceilingHeight;
		u64 builtFrame;

		s32 wallStart;
	};

	void sdisplayList_init(s32* posIndex, s32* dataIndex, s32 planesIndex);
	void sdisplayList_destroy();

	void sdisplayList_clear();
	void sdisplayList_finish();

	void sdisplayList_addSegment(RSector* curSector, GPUCachedSector* cached, SegmentClipped* wallSeg);
	bool sdisplayList_addPortal(Vec3f p0, Vec3f p1, s32 parentPortalId);
	void sdisplayList_draw(SectorPass passId);

	s32  sdisplayList_getSize(SectorPass passId = SECTOR_PASS_OPAQUE);

	u32 sdisplayList_getPackedPortalInfo(s32 portalId);
	u32 sdisplayList_getPlanesFromPortal(u32 portalId, u32 planeType, Vec4f* outPlanes);
}  // TFE_Jedi

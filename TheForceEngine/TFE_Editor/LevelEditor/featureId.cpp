#include "featureId.h"
#include "sharedState.h"

using namespace TFE_Editor;

namespace LevelEditor
{
	enum FeatureIdConst : u32
	{
		// TFE doesn't support levels with over 4 billion sectors, so this is safe.
		INVALID_SECTOR_ID = 0xffffffffu
	};

	FeatureId createFeatureId(const EditorSector* sector, s32 featureIndex, s32 featureData, bool isOverlapped)
	{
		return FeatureId(u64(sector ? u32(sector->id) : INVALID_SECTOR_ID) |
			            (u64(featureIndex) << FID_FEATURE_INDEX) |
			            (u64(featureData) << FID_FEATURE_DATA) |
			            (u64(isOverlapped ? 1 : 0) << FID_OVERLAPPED));
	}

	FeatureId setIsOverlapped(FeatureId id, bool isOverlapped)
	{
		id &= FID_COMPARE_MASK;
		id |= (u64(isOverlapped ? 1 : 0) << FID_OVERLAPPED);
		return id;
	}
		
	EditorSector* unpackFeatureId(FeatureId id, s32* featureIndex, s32* featureData, bool* isOverlapped)
	{
		u32 sectorId  = u32(id & FID_SECTOR_MASK);
		if (featureIndex) { *featureIndex = s32((id >> FID_FEATURE_INDEX) & FID_FEATURE_INDEX_MASK); }
		if (featureData)  { *featureData = s32((id >> FID_FEATURE_DATA) & FID_FEATURE_DATA_MASK); }
		if (isOverlapped) { *isOverlapped = (id & (1ull << FID_OVERLAPPED)) != 0; }
		// Features outside of sectors will have an invalid sector ID.
		return sectorId < (u32)s_level.sectors.size() ? &s_level.sectors[sectorId] : nullptr;
	}
}

#include "featureId.h"
#include "sharedState.h"

using namespace TFE_Editor;

namespace LevelEditor
{
	FeatureId createFeatureId(const EditorSector* sector, s32 featureIndex, s32 featureData, bool isOverlapped)
	{
		return FeatureId(u64(sector ? sector->id : 0) |
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
		*featureIndex = s32((id >> FID_FEATURE_INDEX) & FID_FEATURE_INDEX_MASK);
		if (featureData)  { *featureData = s32((id >> FID_FEATURE_DATA) & FID_FEATURE_DATA_MASK); }
		if (isOverlapped) { *isOverlapped = (id & (1ull << FID_OVERLAPPED)) != 0; }
		// Handle the case where sectorId is invalid, which may happen for guidelines or level notes.
		return sectorId < (u32)s_level.sectors.size() ? &s_level.sectors[sectorId] : nullptr;
	}
}

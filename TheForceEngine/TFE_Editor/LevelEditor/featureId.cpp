#include "featureId.h"
#include "sharedState.h"

using namespace TFE_Editor;

namespace LevelEditor
{
	FeatureId createFeatureId(EditorSector* sector, s32 featureIndex, s32 featureData, bool isOverlapped)
	{
		return FeatureId(u64(sector->id) |
			            (u64(featureIndex) << FID_FEATURE_INDEX) |
			            (u64(featureData) << FID_FEATURE_DATA) |
			            (u64(isOverlapped ? 1 : 0) << FID_OVERLAPPED));
	}

	EditorSector* unpackFeatureId(FeatureId id, s32* featureIndex, s32* featureData, bool* isOverlapped)
	{
		u32 sectorId  = u32(id & FID_SECTOR_MASK);
		*featureIndex = s32((id >> FID_FEATURE_INDEX) & FID_FEATURE_INDEX_MASK);
		if (featureData)  { *featureData = s32((id >> FID_FEATURE_DATA) & FID_FEATURE_DATA_MASK); }
		if (isOverlapped) { *isOverlapped = (id & (1ull << FID_OVERLAPPED)) != 0; }

		return &s_level.sectors[sectorId];
	}
}

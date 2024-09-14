#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <climits> 

namespace LevelEditor
{
	struct EditorSector;
	typedef u64 FeatureId;

	#define FEATUREID_NULL ULLONG_MAX

	enum FeatureIdPacking : u64
	{
		FID_SECTOR = 0ull,
		FID_FEATURE_INDEX = 32ull,
		FID_FEATURE_DATA = 55ull,
		FID_OVERLAPPED = 63ull,

		FID_SECTOR_MASK = (1ull << 32ull) - 1ull,
		FID_FEATURE_INDEX_MASK = (1ull << 23ull) - 1ull,
		FID_FEATURE_DATA_MASK = (1ull << 8ull) - 1ull,
		FID_COMPARE_MASK = (1ull << FID_OVERLAPPED) - 1ull,
		FID_COMPARE_MASK_NO_DATA = (1ull << FID_FEATURE_DATA) - 1ull,
	};

	FeatureId createFeatureId(const EditorSector* sector, s32 featureIndex = 0, s32 featureData = 0, bool isOverlapped = false);
	FeatureId setIsOverlapped(FeatureId id, bool isOverlapped);
	EditorSector* unpackFeatureId(FeatureId id, s32* featureIndex = nullptr, s32* featureData = nullptr, bool* isOverlapped = nullptr);

	inline bool featuresEqual(FeatureId a, FeatureId b)
	{
		return (a & FID_COMPARE_MASK) == (b & FID_COMPARE_MASK);
	}

	inline bool featuresEqualNoData(FeatureId a, FeatureId b)
	{
		return (a & FID_COMPARE_MASK_NO_DATA) == (b & FID_COMPARE_MASK_NO_DATA);
	}
}

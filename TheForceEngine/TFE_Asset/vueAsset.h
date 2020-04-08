#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <string>
#include <vector>

struct VueTransform
{
	Mat3  rotScale;
	Vec3f translation;
};

struct VueAsset
{
	char name[64];
	u32 frameCount;
	u32 transformCount;
	char** transformNames;
	VueTransform* transforms;	// [frameCount * transformCount]
};

namespace TFE_VueAsset
{
	VueAsset* get(const char* name);
	void freeAll();

	s32 getTransformIndex(const VueAsset* vue, const char* name);
	const VueTransform* getTransform(const VueAsset* vue, s32 transformIndex, s32 frameIndex);
	const char* getTransformName(const VueAsset* vue, s32 transformIndex);
};

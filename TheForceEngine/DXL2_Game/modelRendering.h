#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Geometry
// Functions to handle geometry, such as point in polygon tests.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_Asset/modelAsset.h>
#include <DXL2_Asset/vueAsset.h>

struct Sector;

namespace DXL2_ModelRender
{
	void init(DXL2_Renderer* renderer, s32 width, s32 height, f32 focalLength, f32 heightScale);
	void draw(const Model* model, const Vec3f* modelOrientation, const Vec3f* modelPos, const Mat3* optionalTransform, const Vec3f* cameraPos, const f32* cameraRot, f32 heightOffset, const Sector* sector = nullptr);

	void flipVertical();
	void enablePerspectiveCorrectTexturing(bool enable);

	void buildClipPlanes(s32 pitchOffset);
	void setClip(s32 x0, s32 x1, const s16* upperYMask, const s16* lowerYMask);
	void setModelScale(const Vec3f* scale);

	// Inputs: model, position, orientation and camera data.
	// Output: screen rect - the screenspace area that the model covers: min = screenRect[0], max = screenRect[1]
	//         returns true if the model is at least partially on screen, false if not visible.
	// TODO: Save transform matrix.
	bool computeScreenSize(const Model* model, const Vec3f* modelOrientation, const Vec3f* modelPos, const Mat3* optionalTransform, const Vec3f* cameraPos, const f32* cameraRot, f32 heightOffset, Vec2i* screenRect);
}

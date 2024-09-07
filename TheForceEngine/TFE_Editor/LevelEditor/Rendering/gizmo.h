#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>
#include <TFE_RenderShared/gridDef.h>

namespace LevelEditor
{
	enum RotationGizmoPart
	{
		RGP_CIRCLE_OUTER = 0,
		RGP_CIRCLE_CENTER,
		RGP_CIRCLE_INNER,
		RGP_CENTER,
		RGP_COUNT,
		RGP_NONE = RGP_COUNT
	};

	// Helper functions.
	void drawBox2d(const Vec2f* cen, f32 side, f32 lineWidth, u32 color);
	void drawBox3d(const Vec3f* center, f32 side, f32 lineWidth, u32 color);

	// Transform Gizmos.
	void gizmo_drawMove2d(Vec3f p0, Vec3f p1);
	void gizmo_drawMove3d(Vec3f p0, Vec3f p1);
	void gizmo_drawRotation2d(Vec2f center);
	void gizmo_drawRotation3d(Vec3f center);
	f32 gizmo_getRotationRadiusWS(RotationGizmoPart part, Vec3f* center = nullptr);
}

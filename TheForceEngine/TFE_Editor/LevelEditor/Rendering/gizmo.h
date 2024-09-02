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
	void drawBox(const Vec3f* center, f32 side, f32 lineWidth, u32 color);

	// Transform Gizmos.
	void gizmo_drawRotation2d(Vec2f center);
	f32 gizmo_getRotationRadiusWS2d(RotationGizmoPart part);
}

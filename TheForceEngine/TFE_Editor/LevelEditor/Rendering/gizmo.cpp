#include "gizmo.h"
#include "grid.h"
#include "viewport.h"
#include <TFE_System/math.h>
#include <TFE_System/system.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderShared/lineDraw2d.h>
#include <TFE_RenderShared/lineDraw3d.h>
#include <TFE_RenderShared/triDraw2d.h>
#include <TFE_RenderShared/triDraw3d.h>
#include <TFE_Editor/LevelEditor/editTransforms.h>
#include <algorithm>
#include <vector>

using namespace TFE_RenderShared;

namespace LevelEditor
{
	struct RotationGizmo
	{
		f32 radius = 128.0f;
		f32 centerRadius = 4.0f;
		f32 spacing = 24.0f;
		f32 tickSize = 8.0f;
		u32 lineColor  = 0xffff8000;
		u32 cenColor   = 0xffffff40;
		u32 hoverColor = 0x40ff8000;
		u32 highlight  = 0x800060ff;
	};
	// TODO: This can be editable in future versions.
	static RotationGizmo s_rotGizmo = {};

	void drawBox(const Vec3f* center, f32 side, f32 lineWidth, u32 color)
	{
		const f32 w = side * 0.5f;
		const Vec3f lines[] =
		{
			{center->x - w, center->y - w, center->z - w}, {center->x + w, center->y - w, center->z - w},
			{center->x + w, center->y - w, center->z - w}, {center->x + w, center->y - w, center->z + w},
			{center->x + w, center->y - w, center->z + w}, {center->x - w, center->y - w, center->z + w},
			{center->x - w, center->y - w, center->z + w}, {center->x - w, center->y - w, center->z - w},

			{center->x - w, center->y + w, center->z - w}, {center->x + w, center->y + w, center->z - w},
			{center->x + w, center->y + w, center->z - w}, {center->x + w, center->y + w, center->z + w},
			{center->x + w, center->y + w, center->z + w}, {center->x - w, center->y + w, center->z + w},
			{center->x - w, center->y + w, center->z + w}, {center->x - w, center->y + w, center->z - w},

			{center->x - w, center->y - w, center->z - w}, {center->x - w, center->y + w, center->z - w},
			{center->x + w, center->y - w, center->z - w}, {center->x + w, center->y + w, center->z - w},
			{center->x + w, center->y - w, center->z + w}, {center->x + w, center->y + w, center->z + w},
			{center->x - w, center->y - w, center->z + w}, {center->x - w, center->y + w, center->z + w},
		};
		u32 colors[12];
		for (u32 i = 0; i < 12; i++)
		{
			colors[i] = color;
		}

		lineDraw3d_addLines(12, lineWidth, lines, colors);
	}

	void drawCircle2d(Vec2f cen, f32 r, u32 color)
	{
		const u32 colors[] = { color, color };
		const s32 edges = 64;
		const f32 dA = 2.0f * PI / f32(edges);
		f32 angle = 0.0f;

		Vec2f vtx[2];
		vtx[0] = { cosf(angle)*r + cen.x, sinf(angle)*r + cen.z };
		angle += dA;
		for (s32 i = 0; i < edges; i++, angle += dA)
		{
			vtx[1] = { cosf(angle)*r + cen.x, sinf(angle)*r + cen.z };
			lineDraw2d_addLine(1.25f, &vtx[0], colors);

			vtx[0] = vtx[1];
		}
	}

	// Draw a tick mark every 5 degrees with seperations every 45.
	void drawCircleRuler2d(Vec2f cen, f32 r0, f32 fullSize, f32 tickSize, u32 color)
	{
		const u32 colors[] = { color, color };
		const s32 tickCount = 72; // 5 degree ticks, 360/5 = 72
		const f32 dA = 2.0f * PI / f32(tickCount);
		f32 angle = 0.0f;

		Vec2f vtx[2];
		for (s32 i = 0; i < tickCount; i++, angle += dA)
		{
			// 45 degree ticks occur every nine 5 degree ticks.
			const f32 r1 = (i%9)==0 ? r0 - fullSize : r0 - tickSize;
			const f32 ca = cosf(angle), sa = sinf(angle);

			vtx[0] = { ca*r0 + cen.x, sa*r0 + cen.z };
			vtx[1] = { ca*r1 + cen.x, sa*r1 + cen.z };
			lineDraw2d_addLine(1.25f, &vtx[0], colors);
		}
	}

	void drawCirclePath(Vec2f cen, f32 r0, f32 r1, u32 color)
	{
		const s32 edges = 64;
		const f32 dA = 2.0f * PI / f32(edges);
		const s32 idx[6] = { 0, 2, 1, 1, 2, 3 };
		f32 angle = 0.0f;

		Vec2f vtx[4];
		f32 ca = cosf(angle), sa = sinf(angle);
		vtx[0] = { ca*r0 + cen.x, sa*r0 + cen.z };
		vtx[1] = { ca*r1 + cen.x, sa*r1 + cen.z };
		angle += dA;

		for (s32 i = 1; i <= edges; i++, angle += dA)
		{
			ca = cosf(angle); sa = sinf(angle);
			vtx[2] = { ca*r0 + cen.x, sa*r0 + cen.z };
			vtx[3] = { ca*r1 + cen.x, sa*r1 + cen.z };

			// 2 triangles per step.
			triDraw2d_addColored(6, 4, vtx, idx, color);

			vtx[0] = vtx[2];
			vtx[1] = vtx[3];
		}
	}

	void drawCirclePathPartial(Vec2f cen, f32 r0, f32 r1, u32 color, f32 angle0Deg, f32 angle1Deg, f32 dir)
	{
		f32 angle0 = angle0Deg * PI / 180.0f;
		f32 angle1 = angle1Deg * PI / 180.0f;
		if (dir > 0.0f && angle1 - angle0 < 0.0f)
		{
			angle1 += 2.0f * PI;
		}
		else if (dir < 0.0f && angle1 - angle0 > 0.0f)
		{
			angle1 -= 2.0f * PI;
		}

		const s32 edges = 64;
		const f32 dA = (angle1 - angle0) / f32(edges);
		const s32 idx[6] = { 0, 2, 1, 1, 2, 3 };
		f32 angle = angle0;

		Vec2f vtx[4];
		f32 ca = cosf(angle), sa = sinf(angle);
		vtx[0] = { ca*r0 + cen.x, sa*r0 + cen.z };
		vtx[1] = { ca*r1 + cen.x, sa*r1 + cen.z };
		angle += dA;

		for (s32 i = 1; i <= edges; i++, angle += dA)
		{
			ca = cosf(angle); sa = sinf(angle);
			vtx[2] = { ca*r0 + cen.x, sa*r0 + cen.z };
			vtx[3] = { ca*r1 + cen.x, sa*r1 + cen.z };

			// 2 triangles per step.
			triDraw2d_addColored(6, 4, vtx, idx, color);

			vtx[0] = vtx[2];
			vtx[1] = vtx[3];
		}
	}

	f32 gizmo_getRotationRadiusWS2d(RotationGizmoPart part)
	{
		f32 radius = 0.0f;
		switch (part)
		{
			case RGP_CIRCLE_OUTER:
			{
				radius = (s_rotGizmo.radius + s_rotGizmo.spacing) / s_viewportTrans2d.x;
			} break;
			case RGP_CIRCLE_CENTER:
			{
				radius = (s_rotGizmo.radius) / s_viewportTrans2d.x;
			} break;
			case RGP_CIRCLE_INNER:
			{
				radius = (s_rotGizmo.radius - s_rotGizmo.spacing) / s_viewportTrans2d.x;
			} break;
			case RGP_CENTER:
			{
				radius = s_rotGizmo.centerRadius / s_viewportTrans2d.x;
			} break;
		};
		return radius;
	}
		
	void gizmo_drawRotation2d(Vec2f center)
	{
		RotationGizmoPart hoveredPart = (RotationGizmoPart)edit_getTransformRotationHover();
		Vec2f cen = { center.x * s_viewportTrans2d.x + s_viewportTrans2d.y, center.z * s_viewportTrans2d.z + s_viewportTrans2d.w };
		f32 r = s_rotGizmo.radius;

		// For now just draw a rect in the center and a circle.
		
		// Outter circle
		drawCircle2d(cen, r + s_rotGizmo.spacing, s_rotGizmo.lineColor);

		// Center circle
		drawCircle2d(cen, r, s_rotGizmo.lineColor);

		// Inner circle
		drawCircle2d(cen, r - s_rotGizmo.spacing, s_rotGizmo.lineColor);

		// Center
		drawCircle2d(cen, s_rotGizmo.centerRadius, hoveredPart == RGP_CENTER ? s_rotGizmo.highlight : s_rotGizmo.cenColor);

		// Draw snap markings.
		drawCircleRuler2d(cen, r, s_rotGizmo.spacing, s_rotGizmo.tickSize, s_rotGizmo.lineColor);

		// Draw hovered.
		if (hoveredPart == RGP_CIRCLE_OUTER)
		{
			drawCirclePath(cen, r + s_rotGizmo.spacing, r, s_rotGizmo.hoverColor);
		}
		else if (hoveredPart == RGP_CIRCLE_INNER)
		{
			drawCirclePath(cen, r, r - s_rotGizmo.spacing, s_rotGizmo.hoverColor);
		}
		else if (hoveredPart == RGP_CENTER)
		{
			drawCircle2d(cen, s_rotGizmo.centerRadius - 1.0f, s_rotGizmo.highlight);
		}

		// Draw rotation.
		Vec3f rot = edit_getTransformRotation();
		if (rot.z != 0.0f)
		{
			if (hoveredPart == RGP_CIRCLE_OUTER)
			{
				drawCirclePathPartial(cen, r + s_rotGizmo.spacing, r, s_rotGizmo.highlight, rot.x, rot.y, rot.z);
			}
			else if (hoveredPart == RGP_CIRCLE_INNER)
			{
				drawCirclePathPartial(cen, r, r - s_rotGizmo.spacing, s_rotGizmo.highlight, rot.x, rot.y, rot.z);
			}
		}
	}
}

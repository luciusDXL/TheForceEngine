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
	// Fixed size in 3D = tan(FOV/2) * Gizmo-Distance-From-Camera * radius/scaleFactor
	// Reciprocal avoids the division.
	const f32 c_rotationScaleFactor3d = 1.0f / 384.0f;
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

	f32 getRotation3dScale(Vec3f* center);
	void drawCircle2d(Vec2f cen, f32 r, u32 color);
	void drawCircleRuler2d(Vec2f cen, f32 r0, f32 fullSize, f32 tickSize, u32 color);
	void drawCirclePath2d(Vec2f cen, f32 r0, f32 r1, u32 color);
	void drawCirclePathPartial2d(Vec2f cen, f32 r0, f32 r1, u32 color, f32 angle0Deg, f32 angle1Deg, f32 dir);
	void drawCircle3d(Vec3f cen, f32 r, u32 color);
	void drawCircleRuler3d(Vec3f cen, f32 r0, f32 fullSize, f32 tickSize, u32 color);
	void drawCirclePath3d(Vec3f cen, f32 r0, f32 r1, u32 color);
	void drawCirclePathPartial3d(Vec3f cen, f32 r0, f32 r1, u32 color, f32 angle0Deg, f32 angle1Deg, f32 dir);

	//////////////////////////////////////////
	// API
	//////////////////////////////////////////
	void drawBox2d(const Vec2f* cen, f32 side, f32 lineWidth, u32 color)
	{
		const u32 colors[] = { color, color };
		Vec2f vtx[] =
		{
			{cen->x - side, cen->z - side},
			{cen->x + side, cen->z - side},
			{cen->x + side, cen->z + side},
			{cen->x - side, cen->z + side},
			{cen->x - side, cen->z - side},
		};
		for (s32 i = 0; i < 4; i++)
		{
			lineDraw2d_addLine(lineWidth, &vtx[i], colors);
		}
	}

	void drawBox3d(const Vec3f* center, f32 side, f32 lineWidth, u32 color)
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
				
	f32 gizmo_getRotationRadiusWS(RotationGizmoPart part, Vec3f* center)
	{
		const f32 scale = (!center) ? 1.0f / s_viewportTrans2d.x : getRotation3dScale(center);

		f32 radius = 0.0f;
		switch (part)
		{
			case RGP_CIRCLE_OUTER:
			{
				radius = scale * (s_rotGizmo.radius + s_rotGizmo.spacing);
			} break;
			case RGP_CIRCLE_CENTER:
			{
				radius = scale * (s_rotGizmo.radius);
			} break;
			case RGP_CIRCLE_INNER:
			{
				radius = scale * (s_rotGizmo.radius - s_rotGizmo.spacing);
			} break;
			case RGP_CENTER:
			{
				radius = scale * s_rotGizmo.centerRadius;
			} break;
		};
		return radius;
	}

	void gizmo_drawMove2d(Vec3f p0, Vec3f p1)
	{
		const u32 colorMain  = 0xff00a5ff;
		const u32 colorXAxis = 0xff4040ff;
		const u32 colorZAxis = 0xffff4040;
		const Vec2f vtx[] =
		{
			{p0.x * s_viewportTrans2d.x + s_viewportTrans2d.y, p0.z * s_viewportTrans2d.z + s_viewportTrans2d.w},
			{p1.x * s_viewportTrans2d.x + s_viewportTrans2d.y, p1.z * s_viewportTrans2d.z + s_viewportTrans2d.w}
		};
		const u32 colorsMain[] = { colorMain, colorMain };
		const u32 colorsXAxis[] = { colorXAxis, colorXAxis };
		const u32 colorsZAxis[] = { colorZAxis, colorZAxis };

		f32 dx = fabsf(p1.x - p0.x);
		f32 dz = fabsf(p1.z - p0.z);

		lineDraw2d_setLineDrawMode(LINE_DRAW_DASHED);
		// Draw movement line.
		const u32* distColor = colorsMain;
		if (dx < 0.001f) distColor = colorsZAxis;
		else if (dz < 0.001f) distColor = colorsXAxis;
		lineDraw2d_addLine(1.5f, vtx, distColor);
		// Draw X & Y movement lines.
		if (fabsf(p1.x - p0.x) > 0.001f && fabsf(p1.z - p0.z) > 0.001f)
		{
			const Vec2f zaxis[] = { {vtx[0].x, vtx[0].z}, {vtx[0].x, vtx[1].z} };
			const Vec2f xaxis[] = { {vtx[0].x, vtx[1].z}, {vtx[1].x, vtx[1].z} };

			lineDraw2d_addLine(1.5f, zaxis, colorsZAxis);
			lineDraw2d_addLine(1.5f, xaxis, colorsXAxis);
		}
		lineDraw2d_setLineDrawMode();

		drawBox2d(&vtx[0], 2.0f, 1.25f, colorMain);
		drawBox2d(&vtx[1], 2.0f, 1.25f, colorMain);
	}
		
	void gizmo_drawRotation2d(Vec2f center)
	{
		RotationGizmoPart hoveredPart = (RotationGizmoPart)edit_getTransformRotationHover();
		Vec2f cen = { center.x * s_viewportTrans2d.x + s_viewportTrans2d.y, center.z * s_viewportTrans2d.z + s_viewportTrans2d.w };
		f32 r = s_rotGizmo.radius;

		// Draw circles
		drawCircle2d(cen, r + s_rotGizmo.spacing, s_rotGizmo.lineColor);
		drawCircle2d(cen, r, s_rotGizmo.lineColor);
		drawCircle2d(cen, r - s_rotGizmo.spacing, s_rotGizmo.lineColor);
		drawCircle2d(cen, s_rotGizmo.centerRadius, hoveredPart == RGP_CENTER ? s_rotGizmo.highlight : s_rotGizmo.cenColor);

		// Draw snap markings.
		drawCircleRuler2d(cen, r, s_rotGizmo.spacing, s_rotGizmo.tickSize, s_rotGizmo.lineColor);

		// Draw hovered.
		if (hoveredPart == RGP_CIRCLE_OUTER)
		{
			drawCirclePath2d(cen, r + s_rotGizmo.spacing, r, s_rotGizmo.hoverColor);
		}
		else if (hoveredPart == RGP_CIRCLE_INNER)
		{
			drawCirclePath2d(cen, r, r - s_rotGizmo.spacing, s_rotGizmo.hoverColor);
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
				drawCirclePathPartial2d(cen, r + s_rotGizmo.spacing, r, s_rotGizmo.highlight, rot.x, rot.y, rot.z);
			}
			else if (hoveredPart == RGP_CIRCLE_INNER)
			{
				drawCirclePathPartial2d(cen, r, r - s_rotGizmo.spacing, s_rotGizmo.highlight, rot.x, rot.y, rot.z);
			}
		}
	}

	void gizmo_drawMove3d(Vec3f p0, Vec3f p1)
	{
		Vec3f offset0 = { p0.x - s_camera.pos.x, p0.y - s_camera.pos.y, p0.z - s_camera.pos.z };
		Vec3f offset1 = { p1.x - s_camera.pos.x, p1.y - s_camera.pos.y, p1.z - s_camera.pos.z };
		f32 scale0 = sqrtf(TFE_Math::dot(&offset0, &offset0));
		f32 scale1 = sqrtf(TFE_Math::dot(&offset1, &offset1));

		const u32 colorMain = 0xff00a5ff;
		const u32 colorXAxis = 0xff4040ff;
		const u32 colorYAxis = 0xff40ff40;
		const u32 colorZAxis = 0xffff4040;
		const Vec3f vtx[] =
		{
			p0, p1
		};
		const u32 colorsMain[] = { colorMain, colorMain };
		const u32 colorsXAxis[] = { colorXAxis, colorXAxis };
		const u32 colorsYAxis[] = { colorYAxis, colorYAxis };
		const u32 colorsZAxis[] = { colorZAxis, colorZAxis };

		f32 dx = fabsf(p1.x - p0.x);
		f32 dy = fabsf(p1.y - p0.y);
		f32 dz = fabsf(p1.z - p0.z);

		lineDraw3d_setLineDrawMode(LINE_DRAW_DASHED);
		if (dy > dx && dy > dz && dy != 0.0f)
		{
			lineDraw3d_addLine(2.5f, vtx, colorsYAxis);
		}
		else
		{
			// Draw movement line.
			const u32* distColor = colorsMain;
			if (dx < 0.001f) distColor = colorsZAxis;
			else if (dz < 0.001f) distColor = colorsXAxis;
			lineDraw3d_addLine(2.5f, vtx, distColor);
			// Draw X & Y movement lines.
			if (fabsf(p1.x - p0.x) > 0.001f && fabsf(p1.z - p0.z) > 0.001f)
			{
				const Vec3f zaxis[] = { {vtx[0].x, vtx[0].y, vtx[0].z}, {vtx[0].x, vtx[0].y, vtx[1].z} };
				const Vec3f xaxis[] = { {vtx[0].x, vtx[0].y, vtx[1].z}, {vtx[1].x, vtx[0].y, vtx[1].z} };

				lineDraw3d_addLine(2.5f, zaxis, colorsZAxis);
				lineDraw3d_addLine(2.5f, xaxis, colorsXAxis);
			}
		}
		lineDraw3d_setLineDrawMode();

		drawBox3d(&vtx[0], 0.0075f * scale0, 2.0f, colorMain);
		drawBox3d(&vtx[1], 0.0075f * scale1, 2.0f, colorMain);
	}

	void gizmo_drawRotation3d(Vec3f center)
	{
		const RotationGizmoPart hoveredPart = (RotationGizmoPart)edit_getTransformRotationHover();
		const f32 scale = getRotation3dScale(&center);
		const f32 r = s_rotGizmo.radius;
		const f32 rOuter = scale * (r + s_rotGizmo.spacing);
		const f32 rCenter = scale * r;
		const f32 rInner = scale * (r - s_rotGizmo.spacing);
		const f32 rTrans = scale * s_rotGizmo.centerRadius;

		// Draw circles
		drawCircle3d(center, rOuter, s_rotGizmo.lineColor);
		drawCircle3d(center, rCenter, s_rotGizmo.lineColor);
		drawCircle3d(center, rInner, s_rotGizmo.lineColor);
		drawCircle3d(center, rTrans, hoveredPart == RGP_CENTER ? s_rotGizmo.highlight : s_rotGizmo.cenColor);

		// Draw snap markings.
		drawCircleRuler3d(center, rCenter, scale*s_rotGizmo.spacing, scale*s_rotGizmo.tickSize, s_rotGizmo.lineColor);

		// Draw hovered.
		if (hoveredPart == RGP_CIRCLE_OUTER)
		{
			drawCirclePath3d(center, rOuter, rCenter, s_rotGizmo.hoverColor);
		}
		else if (hoveredPart == RGP_CIRCLE_INNER)
		{
			drawCirclePath3d(center, rCenter, rInner, s_rotGizmo.hoverColor);
		}
		else if (hoveredPart == RGP_CENTER)
		{
			drawCircle3d(center, scale*(s_rotGizmo.centerRadius - 1.0f), s_rotGizmo.highlight);
		}

		// Draw rotation.
		const Vec3f rot = edit_getTransformRotation();
		if (rot.z != 0.0f)
		{
			if (hoveredPart == RGP_CIRCLE_OUTER)
			{
				drawCirclePathPartial3d(center, rOuter, rCenter, s_rotGizmo.highlight, rot.x, rot.y, rot.z);
			}
			else if (hoveredPart == RGP_CIRCLE_INNER)
			{
				drawCirclePathPartial3d(center, rCenter, rInner, s_rotGizmo.highlight, rot.x, rot.y, rot.z);
			}
		}
	}

	//////////////////////////////////////////
	// Internal
	//////////////////////////////////////////
	f32 getRotation3dScale(Vec3f* center)
	{
		const Vec3f offset = { center->x - s_camera.pos.x, center->y - s_camera.pos.y, center->z - s_camera.pos.z };
		return sqrtf(offset.x*offset.x + offset.y*offset.y + offset.z*offset.z) * c_rotationScaleFactor3d;
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
			const f32 r1 = (i % 9) == 0 ? r0 - fullSize : r0 - tickSize;
			const f32 ca = cosf(angle), sa = sinf(angle);

			vtx[0] = { ca*r0 + cen.x, sa*r0 + cen.z };
			vtx[1] = { ca*r1 + cen.x, sa*r1 + cen.z };
			lineDraw2d_addLine(1.25f, &vtx[0], colors);
		}
	}
		
	void drawCirclePath2d(Vec2f cen, f32 r0, f32 r1, u32 color)
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

	void drawCirclePathPartial2d(Vec2f cen, f32 r0, f32 r1, u32 color, f32 angle0Deg, f32 angle1Deg, f32 dir)
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

	void drawCircle3d(Vec3f cen, f32 r, u32 color)
	{
		const u32 colors[] = { color, color };
		const s32 edges = 64;
		const f32 dA = 2.0f * PI / f32(edges);
		f32 angle = 0.0f;

		Vec3f vtx[2];
		vtx[0] = { cosf(angle)*r + cen.x, cen.y, sinf(angle)*r + cen.z };
		angle += dA;
		for (s32 i = 0; i < edges; i++, angle += dA)
		{
			vtx[1] = { cosf(angle)*r + cen.x, cen.y, sinf(angle)*r + cen.z };
			lineDraw3d_addLine(2.0f, vtx, colors);

			vtx[0] = vtx[1];
		}
	}

	// Draw a tick mark every 5 degrees with seperations every 45.
	void drawCircleRuler3d(Vec3f cen, f32 r0, f32 fullSize, f32 tickSize, u32 color)
	{
		const u32 colors[] = { color, color };
		const s32 tickCount = 72; // 5 degree ticks, 360/5 = 72
		const f32 dA = 2.0f * PI / f32(tickCount);
		f32 angle = 0.0f;

		Vec3f vtx[2];
		for (s32 i = 0; i < tickCount; i++, angle += dA)
		{
			// 45 degree ticks occur every nine 5-degree ticks.
			const f32 r1 = (i % 9) == 0 ? r0 - fullSize : r0 - tickSize;
			const f32 ca = cosf(angle), sa = sinf(angle);

			vtx[0] = { ca*r0 + cen.x, cen.y, sa*r0 + cen.z };
			vtx[1] = { ca*r1 + cen.x, cen.y, sa*r1 + cen.z };
			lineDraw3d_addLine(2.0f, vtx, colors);
		}
	}
		
	void drawCirclePath3d(Vec3f cen, f32 r0, f32 r1, u32 color)
	{
		const s32 edges = 64;
		const f32 dA = 2.0f * PI / f32(edges);
		const s32 idx[6] = { 0, 2, 1, 1, 2, 3 };
		f32 angle = 0.0f;

		Vec3f vtx[4];
		f32 ca = cosf(angle), sa = sinf(angle);
		vtx[0] = { ca*r0 + cen.x, cen.y, sa*r0 + cen.z };
		vtx[1] = { ca*r1 + cen.x, cen.y, sa*r1 + cen.z };
		angle += dA;

		for (s32 i = 1; i <= edges; i++, angle += dA)
		{
			ca = cosf(angle); sa = sinf(angle);
			vtx[2] = { ca*r0 + cen.x, cen.y, sa*r0 + cen.z };
			vtx[3] = { ca*r1 + cen.x, cen.y, sa*r1 + cen.z };

			// 2 triangles per step.
			triDraw3d_addColored(TRIMODE_BLEND, 6, 4, vtx, idx, color, false, false);

			vtx[0] = vtx[2];
			vtx[1] = vtx[3];
		}
	}

	void drawCirclePathPartial3d(Vec3f cen, f32 r0, f32 r1, u32 color, f32 angle0Deg, f32 angle1Deg, f32 dir)
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

		Vec3f vtx[4];
		f32 ca = cosf(-angle), sa = sinf(-angle);
		vtx[0] = { ca*r0 + cen.x, cen.y, sa*r0 + cen.z };
		vtx[1] = { ca*r1 + cen.x, cen.y, sa*r1 + cen.z };
		angle += dA;

		for (s32 i = 1; i <= edges; i++, angle += dA)
		{
			ca = cosf(-angle); sa = sinf(-angle);
			vtx[2] = { ca*r0 + cen.x, cen.y, sa*r0 + cen.z };
			vtx[3] = { ca*r1 + cen.x, cen.y, sa*r1 + cen.z };

			// 2 triangles per step.
			triDraw3d_addColored(TRIMODE_BLEND, 6, 4, vtx, idx, color, false, false);

			vtx[0] = vtx[2];
			vtx[1] = vtx[3];
		}
	}
}

#include "guidelines.h"
#include "error.h"
#include "levelEditor.h"
#include "infoPanel.h"
#include "sharedState.h"
#include "editGeometry.h"
#include "userPreferences.h"
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/editorMath.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_Polygon/polygon.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Input/input.h>

#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	s32 s_hoveredGuideline = -1;
	s32 s_curGuideline = -1;

	void guideline_clearSelection()
	{
		s_hoveredGuideline = -1;
		s_curGuideline = -1;
	}

	void guideline_delete(s32 id)
	{
		s32 count = (s32)s_level.guidelines.size();
		if (id < 0 || id >= count)
		{
			return;
		}
		for (s32 i = id; i < count - 1; i++)
		{
			s_level.guidelines[i] = s_level.guidelines[i + 1];
		}
		s_level.guidelines.pop_back();

		// Reset the IDs.
		count--;
		for (s32 i = 0; i < count; i++)
		{
			s_level.guidelines[i].id = i;
		}

		guideline_clearSelection();
	}
		
	void guideline_computeBounds(Guideline* guideline)
	{
		Vec2f* vtx = guideline->vtx.data();
		const s32 vtxCount = (s32)guideline->vtx.size();
		guideline->bounds = { vtx[0].x, vtx[0].z, vtx[0].x, vtx[0].z };
		for (s32 v = 1; v < vtxCount; v++)
		{
			guideline->bounds.x = std::min(guideline->bounds.x, vtx[v].x);
			guideline->bounds.y = std::min(guideline->bounds.y, vtx[v].z);
			guideline->bounds.z = std::max(guideline->bounds.z, vtx[v].x);
			guideline->bounds.w = std::max(guideline->bounds.w, vtx[v].z);
		}
		// Account for offsets.
		guideline->bounds.x -= guideline->maxOffset;
		guideline->bounds.y -= guideline->maxOffset;
		guideline->bounds.z += guideline->maxOffset;
		guideline->bounds.w += guideline->maxOffset;
	}

	void guideline_computeKnots(Guideline* guideline)
	{
		guideline->knots.clear();

		const s32 edgeCount = (s32)guideline->edge.size();
		const GuidelineEdge* edge = guideline->edge.data();
		const Vec2f* vtx = guideline->vtx.data();
		const s32 idx0 = edge->idx[0];
		for (s32 e = 0; e < edgeCount; e++, edge++)
		{
			const Vec4f knot0 = { vtx[edge->idx[0]].x, 0.0f, vtx[edge->idx[0]].z, f32(e) };
			guideline->knots.push_back(knot0);

			if (e == edgeCount - 1 && edge->idx[0] != idx0)
			{
				const Vec4f knot1 = { vtx[edge->idx[1]].x, 1.0f, vtx[edge->idx[1]].z, f32(e) };
				guideline->knots.push_back(knot1);
			}
		}
	}

	f32 getClosestPointToCurve(Vec2f pos, Vec2f v0, Vec2f v1, Vec2f c, f32 offset, f32* t)
	{
		f32 sd = signedDistQuadraticBezier(v0, v1, c, pos, *t);
		if (offset == 0.0f) { return fabsf(sd); }

		Vec2f curPos, nrm;
		evaluateQuadraticBezier(v0, v1, c, *t, &curPos, &nrm);
		curPos.x += nrm.x * offset;
		curPos.z += nrm.z * offset;
		const Vec2f offsetToCurve = { curPos.x - pos.x, curPos.z - pos.z };
		return sqrtf(offsetToCurve.x*offsetToCurve.x + offsetToCurve.z*offsetToCurve.z);
	}

	f32 getClosestPointToLine(Vec2f pos, Vec2f v0, Vec2f v1, f32 offset, f32* t)
	{
		Vec2f pointOnLine;
		*t = TFE_Polygon::closestPointOnLineSegment(v0, v1, pos, &pointOnLine);

		// Offset.
		Vec2f nrm = { 0 };
		if (offset)
		{
			nrm.x = -(v1.z - v0.z);
			nrm.z = (v1.x - v0.x);
			nrm = TFE_Math::normalize(&nrm);
		}
		pointOnLine.x += nrm.x * offset;
		pointOnLine.z += nrm.z * offset;

		Vec2f posLineOffset = { pointOnLine.x - pos.x, pointOnLine.z - pos.z };
		return sqrtf(posLineOffset.x*posLineOffset.x + posLineOffset.z*posLineOffset.z);
	}

	f32 getClosestDistToEdge(const GuidelineEdge* edge, const Vec2f* vtx, const Vec2f& pos, f32 offset, f32* t)
	{
		f32 dist;
		if (edge->idx[2] >= 0)
		{
			dist = getClosestPointToCurve(pos, vtx[edge->idx[0]], vtx[edge->idx[1]], vtx[edge->idx[2]], offset, t);
		}
		else
		{
			dist = getClosestPointToLine(pos, vtx[edge->idx[0]], vtx[edge->idx[1]], offset, t);
		}
		return dist;
	}

	Vec2f guideline_getOffsetKnot(const Guideline* guideline, const Vec4f* knot, f32 offset)
	{
		const f32 t = knot->y;
		const s32 edgeIndex = s32(knot->w + 0.5f);
		const GuidelineEdge* edge = &guideline->edge[edgeIndex];
		const Vec2f* vtx = guideline->vtx.data();

		const Vec2f& v0 = vtx[edge->idx[0]];
		const Vec2f& v1 = vtx[edge->idx[1]];
		Vec2f nrm;
		if (edge->idx[2] >= 0)
		{
			Vec2f pos;
			evaluateQuadraticBezier(v0, v1, vtx[edge->idx[2]], t, &pos, &nrm);
		}
		else
		{
			nrm.x = -(v1.z - v0.z);
			nrm.z =  (v1.x - v0.x);
			nrm = TFE_Math::normalize(&nrm);
		}
		return { knot->x + nrm.x * offset, knot->z + nrm.z * offset };
	}
	
	bool guideline_getClosestPoint(const Guideline* guideline, const Vec2f& pos, s32* edgeIndex, s32* offsetIndex, f32* t, Vec2f* closestPoint)
	{
		*edgeIndex = -1;
		*offsetIndex = -1;
		*t = 0.0f;

		const s32 edgeCount = (s32)guideline->edge.size();
		const GuidelineEdge* edge = guideline->edge.data();
		const s32 offsetCount = (s32)guideline->offsets.size();

		f32 s, closestDist = FLT_MAX;
		const Vec2f* vtx = guideline->vtx.data();
		for (s32 e = 0; e < edgeCount; e++, edge++)
		{
			const f32* offsetList = guideline->offsets.data();
			// offset = -1 ==> Center.
			for (s32 o = -1; o < offsetCount; o++)
			{
				const f32 offset = o < 0 ? 0.0f : offsetList[o];
				const f32 dist = getClosestDistToEdge(edge, vtx, pos, offset, &s);
				if (dist < closestDist)
				{
					closestDist = dist;
					*edgeIndex = e;
					*offsetIndex = o;
					*t = s;
				}
			}
		}

		bool success = *edgeIndex >= 0;
		if (success && closestPoint)
		{
			*closestPoint = guideline_getPosition(guideline, *edgeIndex, *offsetIndex, *t);
		}

		// Prefer knots as closest point if within the "knot radius".
		// This allows for precise snapping at those points, which are often on the grid as well.
		const s32 knotCount = (s32)guideline->knots.size();
		const Vec4f* knot = guideline->knots.data();
		const f32 knotRadiusSq = guideline->maxSnapRange * guideline->maxSnapRange;
		f32 minKnotDistSq = FLT_MAX;
		for (s32 k = 0; k < knotCount; k++, knot++)
		{
			// Snap to the base knot and virtual offsets.
			const s32 knotEdge = s32(knot->w + 0.5f);
			const f32 knotT = knot->y;
			const f32* offsetList = guideline->offsets.data();
			const Vec2f knotPos = { knot->x, knot->z };
			for (s32 o = -1; o < offsetCount; o++)
			{
				const Vec2f offsetKnot = o < 0 ? knotPos : guideline_getOffsetKnot(guideline, knot, offsetList[o]);
				const Vec2f offset = { offsetKnot.x - pos.x, offsetKnot.z - pos.z };
				const f32 distSq = offset.x*offset.x + offset.z*offset.z;
				if (distSq <= knotRadiusSq && distSq < minKnotDistSq)
				{
					minKnotDistSq = distSq;
					closestPoint->x = offsetKnot.x;
					closestPoint->z = offsetKnot.z;
					*edgeIndex = knotEdge;
					*offsetIndex = o;
					*t = knotT;
					success = true;
				}
			}
		}

		return success;
	}

	Vec2f guideline_getPosition(const Guideline* guideline, s32 edgeIndex, s32 offsetIndex, f32 t)
	{
		const GuidelineEdge* edge = &guideline->edge[edgeIndex];
		const Vec2f* vtx = guideline->vtx.data();
		const f32 offset = offsetIndex < 0 ? 0.0f : guideline->offsets[offsetIndex];

		Vec2f pos = { 0 };
		Vec2f nrm = { 0 };
		Vec2f v0 = vtx[edge->idx[0]];
		Vec2f v1 = vtx[edge->idx[1]];
		if (edge->idx[2] >= 0)
		{
			evaluateQuadraticBezier(v0, v1, vtx[edge->idx[2]], t, &pos, &nrm);
		}
		else
		{
			pos.x = v0.x * (1.0f - t) + v1.x * t;
			pos.z = v0.z * (1.0f - t) + v1.z * t;
			nrm.x = -(v1.z - v0.z);
			nrm.z = (v1.x - v0.x);
			nrm = TFE_Math::normalize(&nrm);
		}
		pos.x += offset * nrm.x;
		pos.z += offset * nrm.z;

		return pos;
	}

	Vec2f snapToGridOrGuidelines2d(Vec2f pos)
	{
		Vec2f snapPos = pos;
		f32 closestSnapSq = FLT_MAX;

		// Ignore the grid if disabled.
		const bool gridSnapEnabled = !TFE_Input::keyModDown(KEYMOD_ALT) && s_grid.size != 0.0f;
		if (gridSnapEnabled)
		{
			Vec2f posGrid = pos;
			snapToGrid(&posGrid);

			Vec2f offset = { posGrid.x - pos.x, posGrid.z - pos.z };
			f32 distSq = offset.x*offset.x + offset.z*offset.z;
			if (distSq < closestSnapSq)
			{
				closestSnapSq = distSq;
				snapPos = posGrid;
			}
		}

		if (!(s_editorConfig.interfaceFlags & PIF_HIDE_GUIDELINES))
		{
			Vec4f posBoundsWS = { pos.x, pos.z, pos.x, pos.z };
			const s32 count = (s32)s_level.guidelines.size();
			const Guideline* guideline = s_level.guidelines.data();
			for (s32 i = 0; i < count; i++, guideline++)
			{
				if (guideline->flags & GLFLAG_DISABLE_SNAPPING) { continue; }
				if (guideline->flags & GLFLAG_LIMIT_HEIGHT)
				{
					if (s_grid.height < guideline->minHeight || s_grid.height > guideline->maxHeight)
					{
						continue;
					}
				}

				if (!boundsOverlap(guideline->bounds, posBoundsWS, guideline->maxSnapRange)) { continue; }
				const f32 rSq = guideline->maxSnapRange * guideline->maxSnapRange;

				f32 t;
				Vec2f posGuideline;
				s32 edgeIndex, offsetIndex;
				if (guideline_getClosestPoint(guideline, pos, &edgeIndex, &offsetIndex, &t, &posGuideline))
				{
					const Vec2f offset = { posGuideline.x - pos.x, posGuideline.z - pos.z };
					const f32 distSq = offset.x*offset.x + offset.z*offset.z;
					if (distSq < closestSnapSq && distSq <= rSq)
					{
						closestSnapSq = distSq;
						snapPos = posGuideline;
					}
				}
			}
		}

		// No snapping has occured.
		if (!gridSnapEnabled && closestSnapSq == FLT_MAX)
		{
			// snap to the fine grid.
			snapToGrid(&snapPos);
		}

		return snapPos;
	}
}

#include "editGeometry.h"
#include "hotkeys.h"
#include "error.h"
#include "infoPanel.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "levelEditorHistory.h"
#include "sharedState.h"
#include "selection.h"
#include "guidelines.h"
#include <TFE_System/math.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_Input/input.h>

#include <climits>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;
using namespace TFE_Jedi;

namespace LevelEditor
{
	const f32 c_relativeVtxSnapScale = 0.125f;
	const f32 c_defaultSectorHeight = 16.0f;

	/////////////////////////////////////////////////////
	// Internal Variables
	/////////////////////////////////////////////////////
	// Used to restore clipped/split wall attributes.
	struct SourceWall
	{
		LevelTexture tex[WP_COUNT] = {};
		Vec2f vtx[2];
		Vec2f dir;
		Vec2f rcpDelta;
		f32 len;
		f32 floorHeight;
		u32 flags[3] = { 0 };
		s32 wallLight = 0;
	};
	static std::vector<SourceWall> s_sourceWallList;
	static s32 s_newWallTexOverride = -1;
	static s32 s_curveSegDelta = 0;

	/////////////////////////////////////////////////////
	// Shared Variables
	/////////////////////////////////////////////////////
	GeometryEdit s_geoEdit = {};

	void handlePartialShape();
	bool snapToLine(Vec2f& pos, f32 maxDist, Vec2f& newPos, FeatureId& snappedFeature);
	Vec2f evaluateQuadraticBezier(const Vec2f& ac, const Vec2f& bc, const Vec2f& c, f32 t);

	bool snapToLine2d(Vec2f& pos, f32 maxDist, Vec2f& newPos, FeatureId& snappedFeature)
	{
		SectorList sectorList;
		Vec3f pos3d = { pos.x, s_grid.height, pos.z };
		if (!getOverlappingSectorsPt(&pos3d, &sectorList, 0.5f))
		{
			return false;
		}

		const s32 count = (s32)sectorList.size();
		if (count < 1)
		{
			return false;
		}

		EditorSector** list = sectorList.data();
		EditorSector* closestSector = nullptr;

		// Higher weighting to vertices.
		f32 closestDistSqVtx = FLT_MAX;
		f32 closestDistSqWall = FLT_MAX;
		EditorSector* closestSectorVtx = nullptr;
		EditorSector* closestSectorWall = nullptr;
		f32 maxDistSq = maxDist * maxDist;
		s32 vtxIndex = -1;
		s32 wallIndex = -1;

		Vec2f closestPtVtx;
		Vec2f closestPtWall;

		for (s32 i = 0; i < count; i++)
		{
			EditorSector* sector = list[i];
			const s32 wallCount = (s32)sector->walls.size();
			const s32 vtxCount = (s32)sector->vtx.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vtx.data();

			for (s32 v = 0; v < vtxCount; v++)
			{
				Vec2f offset = { pos.x - vtx[v].x, pos.z - vtx[v].z };
				f32 distSq = offset.x*offset.x + offset.z*offset.z;
				if (distSq < closestDistSqVtx && distSq < maxDistSq)
				{
					closestDistSqVtx = distSq;
					vtxIndex = v;
					closestSectorVtx = sector;
					closestPtVtx = vtx[v];
				}
			}

			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				const Vec2f* v0 = &vtx[wall->idx[0]];
				const Vec2f* v1 = &vtx[wall->idx[1]];
				Vec2f pointOnSeg;
				f32 s = TFE_Polygon::closestPointOnLineSegment(*v0, *v1, pos, &pointOnSeg);

				const Vec2f diff = { pointOnSeg.x - pos.x, pointOnSeg.z - pos.z };
				const f32 distSq = diff.x*diff.x + diff.z*diff.z;
				if (distSq < closestDistSqWall && distSq < maxDistSq)
				{
					closestDistSqWall = distSq;
					wallIndex = w;
					closestSectorWall = sector;
					closestPtWall = pointOnSeg;
				}
			}
		}

		if (vtxIndex >= 0 && (wallIndex < 0 || closestDistSqVtx <= closestDistSqWall || closestDistSqVtx < maxDistSq * c_relativeVtxSnapScale))
		{
			newPos = closestPtVtx;
			snappedFeature = createFeatureId(closestSectorVtx, vtxIndex, 1);
		}
		else if (wallIndex >= 0)
		{
			newPos = closestPtWall;
			snappedFeature = createFeatureId(closestSectorWall, wallIndex, 0);
		}

		return vtxIndex >= 0 || wallIndex >= 0;
	}

	bool snapToLine3d(Vec3f& pos3d, f32 maxDist, f32 maxCamDist, Vec2f& newPos, FeatureId& snappedFeature, f32* outDistSq, f32* outCameraDistSq)
	{
		SectorList sectorList;
		if (!getOverlappingSectorsPt(&pos3d, &sectorList, 0.5f))
		{
			return false;
		}

		const s32 count = (s32)sectorList.size();
		if (count < 1)
		{
			return false;
		}

		EditorSector** list = sectorList.data();
		EditorSector* closestSector = nullptr;

		// Higher weighting to vertices.
		f32 closestDistSqVtx = FLT_MAX;
		f32 closestDistSqWall = FLT_MAX;
		EditorSector* closestSectorVtx = nullptr;
		EditorSector* closestSectorWall = nullptr;
		f32 maxDistSq = maxDist * maxDist;
		f32 maxCamDistSq = maxCamDist * maxCamDist;
		s32 vtxIndex = -1;
		s32 wallIndex = -1;

		Vec2f closestPtVtx;
		Vec2f closestPtWall;
		f32 camDistSqVtx = FLT_MAX;
		f32 camDistSqWall = FLT_MAX;

		for (s32 i = 0; i < count; i++)
		{
			EditorSector* sector = list[i];
			const s32 wallCount = (s32)sector->walls.size();
			const s32 vtxCount = (s32)sector->vtx.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vtx.data();

			for (s32 v = 0; v < vtxCount; v++)
			{
				const Vec2f offset = { pos3d.x - vtx[v].x, pos3d.z - vtx[v].z };
				const f32 distSq = offset.x*offset.x + offset.z*offset.z;

				const Vec3f camOffset = { s_camera.pos.x - vtx[v].x, s_camera.pos.y - sector->floorHeight, s_camera.pos.z - vtx[v].z };
				const f32 camDistSq = camOffset.x*camOffset.x + camOffset.y*camOffset.y + camOffset.z*camOffset.z;

				if (distSq < closestDistSqVtx && distSq < maxDistSq && camDistSq < maxCamDistSq)
				{
					closestDistSqVtx = distSq;
					camDistSqVtx = camDistSq;
					vtxIndex = v;
					closestSectorVtx = sector;
					closestPtVtx = vtx[v];
				}
			}

			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				const Vec2f* v0 = &vtx[wall->idx[0]];
				const Vec2f* v1 = &vtx[wall->idx[1]];
				Vec2f pointOnSeg;
				f32 s = TFE_Polygon::closestPointOnLineSegment(*v0, *v1, { pos3d.x, pos3d.z }, &pointOnSeg);

				const Vec2f diff = { pointOnSeg.x - pos3d.x, pointOnSeg.z - pos3d.z };
				const f32 distSq = diff.x*diff.x + diff.z*diff.z;

				const Vec3f camOffset = { s_camera.pos.x - pointOnSeg.x, s_camera.pos.y - sector->floorHeight, s_camera.pos.z - pointOnSeg.z };
				const f32 camDistSq = camOffset.x*camOffset.x + camOffset.y*camOffset.y + camOffset.z*camOffset.z;

				if (distSq < closestDistSqWall && distSq < maxDistSq && camDistSq < maxCamDistSq)
				{
					closestDistSqWall = distSq;
					camDistSqWall = camDistSq;
					wallIndex = w;
					closestSectorWall = sector;
					closestPtWall = pointOnSeg;
				}
			}
		}

		if (vtxIndex >= 0 && (wallIndex < 0 || closestDistSqVtx <= closestDistSqWall || closestDistSqVtx < maxDistSq * c_relativeVtxSnapScale))
		{
			newPos = closestPtVtx;
			snappedFeature = createFeatureId(closestSectorVtx, vtxIndex, 1);

			if (outDistSq) { *outDistSq = closestDistSqVtx; }
			if (outCameraDistSq) { *outCameraDistSq = camDistSqVtx; }
		}
		else if (wallIndex >= 0)
		{
			newPos = closestPtWall;
			snappedFeature = createFeatureId(closestSectorWall, wallIndex, 0);

			if (outDistSq) { *outDistSq = closestDistSqWall; }
			if (outCameraDistSq) { *outCameraDistSq = camDistSqWall; }
		}

		return vtxIndex >= 0 || wallIndex >= 0;
	}

	bool gridCursorIntersection(Vec3f* pos)
	{
		if ((s_camera.pos.y > s_grid.height && s_cursor3d.y < s_camera.pos.y) ||
			(s_camera.pos.y < s_grid.height && s_cursor3d.y > s_camera.pos.y))
		{
			f32 u = (s_grid.height - s_camera.pos.y) / (s_cursor3d.y - s_camera.pos.y);
			assert(fabsf(s_camera.pos.y + u * (s_cursor3d.y - s_camera.pos.y) - s_grid.height) < 0.001f);

			pos->x = s_camera.pos.x + u * (s_cursor3d.x - s_camera.pos.x);
			pos->y = s_grid.height;
			pos->z = s_camera.pos.z + u * (s_cursor3d.z - s_camera.pos.z);
			return true;
		}
		return false;
	}

	bool snapToLine(Vec2f& pos, f32 maxDist, Vec2f& newPos, FeatureId& snappedFeature)
	{
		if (s_view == EDIT_VIEW_2D)
		{
			return snapToLine2d(pos, maxDist, newPos, snappedFeature);
		}
		else
		{
			Vec2f _newPos[2];
			FeatureId _snappedFeature[2];
			f32 distSq[2], camDistSq[2];
			s32 foundCount = 0;

			// Current cursor position.
			if (snapToLine3d(s_cursor3d, maxDist, 25.0f, _newPos[foundCount], _snappedFeature[foundCount], &distSq[foundCount], &camDistSq[foundCount]))
			{
				foundCount++;
			}

			// Position on grid.
			Vec3f testPos;
			if (gridCursorIntersection(&testPos))
			{
				if (testPos.y != s_cursor3d.y && snapToLine3d(testPos, maxDist, 25.0f, _newPos[foundCount], _snappedFeature[foundCount], &distSq[foundCount], &camDistSq[foundCount]))
				{
					foundCount++;
				}
			}
			if (!foundCount) { return false; }

			if (foundCount == 1)
			{
				newPos = _newPos[0];
				snappedFeature = _snappedFeature[0];
			}
			else
			{
				// Which the closer of the two to the camera.
				if (camDistSq[0] <= camDistSq[1])
				{
					newPos = _newPos[0];
					snappedFeature = _snappedFeature[0];
				}
				else
				{
					newPos = _newPos[1];
					snappedFeature = _snappedFeature[1];
				}
			}
		}
		return true;
	}
		
	void clearSourceList()
	{
		s_sourceWallList.clear();
	}

	void addWallToSourceList(const EditorSector* sector, const EditorWall* wall)
	{
		SourceWall swall;
		for (s32 i = 0; i < WP_COUNT; i++) { swall.tex[i] = wall->tex[i]; }
		for (s32 i = 0; i < 3; i++) { swall.flags[i] = wall->flags[i]; }
		swall.vtx[0] = sector->vtx[wall->idx[0]];
		swall.vtx[1] = sector->vtx[wall->idx[1]];
		swall.wallLight = wall->wallLight;

		swall.dir = { swall.vtx[1].x - swall.vtx[0].x, swall.vtx[1].z - swall.vtx[0].z };
		swall.rcpDelta.x = fabsf(swall.dir.x) > FLT_EPSILON ? 1.0f / swall.dir.x : 1.0f;
		swall.rcpDelta.z = fabsf(swall.dir.z) > FLT_EPSILON ? 1.0f / swall.dir.z : 1.0f;
		swall.len = sqrtf(swall.dir.x*swall.dir.x + swall.dir.z*swall.dir.z);
		swall.dir = TFE_Math::normalize(&swall.dir);

		swall.floorHeight = sector->floorHeight;

		s_sourceWallList.push_back(swall);
	}

	bool mapWallToSourceList(const EditorSector* sector, EditorWall* wall)
	{
		if (s_sourceWallList.empty()) { return false; }

		const Vec2f v0 = sector->vtx[wall->idx[0]];
		const Vec2f v1 = sector->vtx[wall->idx[1]];
		Vec2f dir = { v1.x - v0.x, v1.z - v0.z };
		dir = TFE_Math::normalize(&dir);

		const f32 eps = 0.001f;
		const s32 count = (s32)s_sourceWallList.size();
		const SourceWall* swall = s_sourceWallList.data();
		s32 singleTouch = -1;
		for (s32 i = 0; i < count; i++, swall++)
		{
			// The direction has to match.
			if (dir.x*swall->dir.x + dir.z*swall->dir.z < 1.0f - eps)
			{
				continue;
			}

			f32 u, v;
			if (fabsf(dir.x) >= fabsf(dir.z))
			{
				u = (v0.x - swall->vtx[0].x) * swall->rcpDelta.x;
				v = (v1.x - swall->vtx[0].x) * swall->rcpDelta.x;
			}
			else
			{
				u = (v0.z - swall->vtx[0].z) * swall->rcpDelta.z;
				v = (v1.z - swall->vtx[0].z) * swall->rcpDelta.z;
			}

			// Verify that the closest point is close enough.
			Vec2f uPt = { swall->vtx[0].x + u * (swall->vtx[1].x - swall->vtx[0].x), swall->vtx[0].z + u * (swall->vtx[1].z - swall->vtx[0].z) };
			Vec2f offset = { uPt.x - v0.x, uPt.z - v0.z };
			if (offset.x*offset.x + offset.z*offset.z > eps)
			{
				continue;
			}

			f32 absU = fabsf(u);
			f32 absV = fabsf(v);
			if (absU < eps) { u = 0.0f; }
			if (absV < eps) { v = 0.0f; }

			absU = fabsf(1.0f - u);
			absV = fabsf(1.0f - v);
			if (absU < eps) { u = 1.0f; }
			if (absV < eps) { v = 1.0f; }

			if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f)
			{
				// Handle adjacent walls as well.
				if ((u >= 0.0f && u <= 1.0f) || (v >= 0.0f && v <= 1.0f))
				{
					singleTouch = i;
				}
				continue;
			}

			// Found a match!
			for (s32 i = 0; i < WP_COUNT; i++) { wall->tex[i] = swall->tex[i]; }
			for (s32 i = 0; i < 3; i++) { wall->flags[i] = swall->flags[i]; }
			wall->wallLight = swall->wallLight;

			// Only the first wall will keep the sign...
			if (swall->tex[WP_SIGN].texIndex >= 0 && u > 0.0f && v > 0.0f)
			{
				wall->tex[WP_SIGN].texIndex = -1;
				wall->tex[WP_SIGN].offset = { 0 };
			}

			// Handle texture offsets.
			const f32 uSplit = u * swall->len;
			const f32 vSplit = swall->floorHeight - sector->floorHeight;
			for (s32 i = 0; i < WP_COUNT; i++)
			{
				if (i == WP_SIGN || wall->tex[i].texIndex < 0) { continue; }
				wall->tex[i].offset.x += uSplit;
				wall->tex[i].offset.z += vSplit;
			}
			return true;
		}

		if (s_newWallTexOverride >= 0)
		{
			for (s32 i = 0; i < WP_COUNT; i++)
			{
				if (i != WP_SIGN)
				{
					wall->tex[i].texIndex = s_newWallTexOverride;
				}
			}
		}
		else
		{
			// Copy the texture from the touching wall.
			if (singleTouch >= 0)
			{
				swall = s_sourceWallList.data() + singleTouch;
			}
			// Or just give up and use the first wall in the list.
			else
			{
				swall = s_sourceWallList.data();
			}

			for (s32 i = 0; i < WP_COUNT; i++)
			{
				if (i != WP_SIGN)
				{
					wall->tex[i] = swall->tex[i];
				}
			}
		}
		return true;
	}

	bool polyOverlap(Polygon* polyA, Polygon* polyB)
	{
		const s32 vtxCountA = (s32)polyA->vtx.size();
		const s32 vtxCountB = (s32)polyB->vtx.size();
		const s32 edgeCountA = (s32)polyA->edge.size();
		const s32 edgeCountB = (s32)polyB->edge.size();
		const Vec2f* vtxA = polyA->vtx.data();
		const Vec2f* vtxB = polyB->vtx.data();
		// Any vertex of A inside B?
		for (s32 v = 0; v < vtxCountA; v++)
		{
			if (TFE_Polygon::pointInsidePolygon(polyB, vtxA[v]))
			{
				return true;
			}
		}
		// Any vertex of B inside A?
		for (s32 v = 0; v < vtxCountB; v++)
		{
			if (TFE_Polygon::pointInsidePolygon(polyA, vtxB[v]))
			{
				return true;
			}
		}
		// Any segment of A intersect segment of B.
		// This is O(N*M), where N = edgeCountA, M = edgeCountB.
		const f32 eps = 1e-3f;
		const Edge* edgeA = polyA->edge.data();
		for (s32 eA = 0; eA < edgeCountA; eA++, edgeA++)
		{
			Vec2f vA0 = vtxA[edgeA->i0];
			Vec2f vA1 = vtxA[edgeA->i1];
			const Edge* edgeB = polyB->edge.data();
			for (s32 eB = 0; eB < edgeCountB; eB++, edgeB++)
			{
				Vec2f vB0 = vtxB[edgeB->i0];
				Vec2f vB1 = vtxB[edgeB->i1];

				Vec2f vI;
				if (TFE_Polygon::lineSegmentsIntersect(vA0, vA1, vB0, vB1, &vI, nullptr, nullptr, eps))
				{
					return true;
				}
			}
		}

		return false;
	}

	void updateSectorFromPolygon(EditorSector* sector, const BPolygon& poly)
	{
		sector->walls.clear();
		sector->vtx.clear();

		const s32 edgeCount = (s32)poly.edges.size();
		const BEdge* edge = poly.edges.data();
		sector->walls.reserve(edgeCount);
		for (s32 e = 0; e < edgeCount; e++, edge++)
		{
			EditorWall wall = {};
			wall.idx[0] = insertVertexIntoSector(sector, edge->v0);
			wall.idx[1] = insertVertexIntoSector(sector, edge->v1);
			// Avoid placing denegerate walls.
			if (wall.idx[0] == wall.idx[1])
			{
				continue;
			}

			if (!mapWallToSourceList(sector, &wall))
			{
				wall.tex[WP_MID].texIndex = getDefaultTextureIndex(WP_MID);
				wall.tex[WP_TOP].texIndex = getDefaultTextureIndex(WP_TOP);
				wall.tex[WP_BOT].texIndex = getDefaultTextureIndex(WP_BOT);
			}
			sector->walls.push_back(wall);
		}
	}

	void copySectorForSplit(const EditorSector* src, EditorSector* dst)
	{
		dst->groupId = src->groupId;
		dst->groupIndex = src->groupIndex;
		dst->floorHeight = src->floorHeight;
		dst->ceilHeight = src->ceilHeight;
		dst->secHeight = src->secHeight;
		dst->floorTex = src->floorTex;
		dst->ceilTex = src->ceilTex;
		dst->ambient = src->ambient;
		dst->flags[0] = src->flags[0];
		dst->flags[1] = src->flags[1];
		dst->flags[2] = src->flags[2];
		dst->layer = src->layer;
	}

	void addToAdjoinFixupList(s32 id, std::vector<s32>& adjoinSectorsToFix)
	{
		const s32 count = (s32)adjoinSectorsToFix.size();
		const s32* idList = adjoinSectorsToFix.data();
		for (s32 i = 0; i < count; i++)
		{
			if (idList[i] == id) { return; }
		}
		adjoinSectorsToFix.push_back(id);
	}

	struct ExactIntersect
	{
		f32 u;
		Vec2f v;
	};

	bool sortByU(ExactIntersect& a, ExactIntersect& b)
	{
		return a.u < b.u;
	}

	void handleClipInteriorPolygons(s32 candidateCount, const BPolygon* candidateList, u32 heightFlags, f32 clipFloor, f32 clipCeil, EditorSector* newSector, BPolygon* outPoly)
	{
		// Add intersections from all candidate polygons.
		for (s32 i = 0; i < candidateCount; i++)
		{
			TFE_Polygon::addEdgeIntersectionsToPoly(outPoly, &candidateList[i]);
		}
		// Set the floor and ceiling height based on flags.
		if (heightFlags & HFLAG_SET_FLOOR)
		{
			newSector->floorHeight = clipFloor;
		}
		if (heightFlags & HFLAG_SET_CEIL)
		{
			newSector->ceilHeight = clipCeil;
		}
		// This is a new sector, so add to the current group.
		newSector->groupId = groups_getCurrentId();
		newSector->groupIndex = 0;
	}

	// Add vertices from shape that touch the polygon edges to those edges to avoid precision issues when clipping.
	void addTouchingVertsToPolygon(const s32 shapeVtxCount, const Vec2f* shapeVtx, BPolygon* cpoly)
	{
		BPolygon tmp = {};
		s32 edgeCount = (s32)cpoly->edges.size();
		bool hasIntersections = false;

		std::vector<ExactIntersect> eit;
		BEdge* cedge = cpoly->edges.data();
		for (s32 e = 0; e < edgeCount; e++, cedge++)
		{
			Vec2f v0 = cedge->v0;
			Vec2f v1 = cedge->v1;

			eit.clear();
			for (s32 v = 0; v < shapeVtxCount; v++)
			{
				Vec2f v2 = shapeVtx[v + 0];
				Vec2f v3 = shapeVtx[(v + 1) % shapeVtxCount];

				Vec2f vI;
				f32 s, t;
				if (TFE_Polygon::lineSegmentsIntersect(v0, v1, v2, v3, &vI, &s, &t))
				{
					if (s > 0.0f && s < 1.0f && (t < 0.001f || t > 1.0f - 0.001f))
					{
						vI = t < 0.001f ? v2 : v3;
						hasIntersections = true;
						eit.push_back({ s, vI });
					}
				}
			}

			if (!eit.empty())
			{
				std::sort(eit.begin(), eit.end(), sortByU);
				const s32 iCount = (s32)eit.size();
				ExactIntersect* it = eit.data();
				for (s32 i = 0; i < iCount; i++, it++)
				{
					TFE_Polygon::addEdgeToBPoly(v0, it->v, &tmp);
					v0 = it->v;
				}
				// Insert the final edge.
				TFE_Polygon::addEdgeToBPoly(v0, v1, &tmp);
			}
			else
			{
				TFE_Polygon::addEdgeToBPoly(v0, v1, &tmp);
			}
		}
		if (hasIntersections)
		{
			*cpoly = tmp;
		}
	}

	bool isShapeSubsector(const EditorSector* sector, s32 firstVertex)
	{
		const s32 vtxCount = (s32)s_geoEdit.shape.size();
		const Vec2f* vtx = s_geoEdit.shape.data();
		bool firstInside = TFE_Polygon::pointInsidePolygon(&sector->poly, vtx[firstVertex]);
		bool firstOnEdge = TFE_Polygon::pointOnPolygonEdge(&sector->poly, vtx[firstVertex]) >= 0;
		// Early out
		if (!firstInside && !firstOnEdge) { return false; }
		if (firstInside && !firstOnEdge) { return true; }

		// The shape is a subsector _if_
		//   * Any shape vertex is inside the sector but *not* on the edge.
		//   * All shape vertices are on the edges.
		bool allOnEdges = true;
		for (s32 v = 0; v < vtxCount; v++)
		{
			bool inside = TFE_Polygon::pointInsidePolygon(&sector->poly, vtx[v]);
			bool onEdge = TFE_Polygon::pointOnPolygonEdge(&sector->poly, vtx[v]) >= 0;

			// If not on an edge but inside.
			if (!onEdge)
			{
				allOnEdges = false;
				if (inside)
				{
					return true;
				}
			}
		}
		// All vertices are on edges.
		return allOnEdges;
	}

	void shapeToPolygon(s32 count, Vec2f* vtx, Polygon& poly)
	{
		poly.edge.resize(count);
		poly.vtx.resize(count);

		poly.bounds[0] = { FLT_MAX,  FLT_MAX };
		poly.bounds[1] = { -FLT_MAX, -FLT_MAX };

		for (size_t v = 0; v < count; v++, vtx++)
		{
			poly.vtx[v] = *vtx;
			poly.bounds[0].x = std::min(poly.bounds[0].x, vtx->x);
			poly.bounds[0].z = std::min(poly.bounds[0].z, vtx->z);
			poly.bounds[1].x = std::max(poly.bounds[1].x, vtx->x);
			poly.bounds[1].z = std::max(poly.bounds[1].z, vtx->z);
		}

		for (s32 w = 0; w < count; w++)
		{
			s32 a = w;
			s32 b = (w + 1) % count;
			poly.edge[w] = { a, b };
		}

		// Clear out cached triangle data.
		poly.triVtx.clear();
		poly.triIdx.clear();

		TFE_Polygon::computeTriangulation(&poly);
	}

	void createNewSector(EditorSector* sector, const f32* heights)
	{
		*sector = {};
		sector->id = (s32)s_level.sectors.size();
		// Just copy for now.
		bool hasSectors = !s_level.sectors.empty();

		if (s_editorConfig.levelEditorFlags & LEVEDITOR_FLAG_ALWAYS_USE_DEFTEX)
		{
			sector->floorTex.texIndex = getTextureIndex("DEFAULT.BM");
			sector->ceilTex.texIndex  = getTextureIndex("DEFAULT.BM");
		}
		else
		{
			sector->floorTex.texIndex = hasSectors ? s_level.sectors[0].floorTex.texIndex : getTextureIndex("DEFAULT.BM");
			sector->ceilTex.texIndex = hasSectors ? s_level.sectors[0].ceilTex.texIndex : getTextureIndex("DEFAULT.BM");
		}

		sector->floorHeight = std::min(heights[0], heights[1]);
		sector->ceilHeight = std::max(heights[0], heights[1]);
		sector->ambient = 20;
		sector->layer = s_curLayer;
		sector->groupId = groups_getCurrentId();
		sector->groupIndex = 0;
	}

	bool findShapeOverlapCandidates(const f32* heights, bool allowSubsectorExtrude, s32 firstVertex,
		Polygon* shapePoly, std::vector<EditorSector*>* candidates, Vec3f* shapeBounds, u32* heightFlags, f32* clipFloorHeight, f32* clipCeilHeight)
	{
		// Rules: 
		// * layers have to match, unless all layers are visible.
		// * heights have to overlap, no merging with sectors out of range.
		// * sector bounds needs to overlap.

		// 1. compute the bounds.
		const s32 vtxCount = (s32)s_geoEdit.shape.size();
		const Vec2f* vtx = s_geoEdit.shape.data();
		shapeBounds[0] = { vtx[0].x, min(heights[0], heights[1]), vtx[0].z };
		shapeBounds[1] = { vtx[0].x, max(heights[0], heights[1]), vtx[0].z };
		for (s32 v = 1; v < vtxCount; v++)
		{
			shapeBounds[0].x = min(shapeBounds[0].x, vtx[v].x);
			shapeBounds[0].z = min(shapeBounds[0].z, vtx[v].z);

			shapeBounds[1].x = max(shapeBounds[1].x, vtx[v].x);
			shapeBounds[1].z = max(shapeBounds[1].z, vtx[v].z);
		}

		// Convert the shape to a polygon.
		shapeToPolygon(vtxCount, s_geoEdit.shape.data(), *shapePoly);

		// 2a. Check to see if this is a sub-sector and if so adjust the heights, flags, and bounds.
		const s32 sectorCount = (s32)s_level.sectors.size();
		EditorSector* sector = nullptr;
		if (s_view == EDIT_VIEW_3D && s_geoEdit.boolMode == BMODE_MERGE && allowSubsectorExtrude)
		{
			// TODO: Better spatial queries to avoid looping over everything...
			// TODO: Factor out so the rendering can be altered accordingly...
			sector = s_level.sectors.data();
			for (s32 s = 0; s < sectorCount; s++, sector++)
			{
				if (!sector_isInteractable(sector) || !sector_onActiveLayer(sector)) { continue; }

				const bool onFloor = fabsf(heights[0] - sector->floorHeight) < FLT_EPSILON;
				const bool onCeil = fabsf(heights[0] - sector->ceilHeight) < FLT_EPSILON;
				if ((onFloor || onCeil) && isShapeSubsector(sector, firstVertex))
				{
					// This is a sub-sector, so adjust the heights accordingly.
					*heightFlags = onFloor ? HFLAG_SET_FLOOR : HFLAG_SET_CEIL;
					*clipFloorHeight = onFloor ? heights[1] : sector->floorHeight;
					*clipCeilHeight = onFloor ? sector->ceilHeight : heights[1];

					// Expand the bounds.
					shapeBounds[0].y = min(shapeBounds[0].y, min(sector->floorHeight, sector->ceilHeight));
					shapeBounds[1].y = max(shapeBounds[1].y, max(sector->floorHeight, sector->ceilHeight));
					break;
				}
			}
		}

		// 2b. Expand the bounds slightly to avoid precision issues.
		const f32 expand = 0.01f;
		shapeBounds[0].x -= expand;
		shapeBounds[0].y -= expand;
		shapeBounds[0].z -= expand;
		shapeBounds[1].x += expand;
		shapeBounds[1].y += expand;
		shapeBounds[1].z += expand;

		// 2c. Then add potentially overlapping sectors.
		sector = s_level.sectors.data();
		candidates->clear();
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if (!sector_isInteractable(sector) || !sector_onActiveLayer(sector)) { continue; }
			// The bounds have to overlap.
			if (!aabbOverlap3d(shapeBounds, sector->bounds)) { continue; }
			// Polygons have to overlap.
			if (!polyOverlap(shapePoly, &sector->poly)) { continue; }

			// Insert walls for future reference.
			const s32 wallCount = (s32)sector->walls.size();
			EditorWall* wall = sector->walls.data();
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				addWallToSourceList(sector, wall);
			}

			// Add to the list of candidates.
			candidates->push_back(sector);
		}
		return !candidates->empty();
	}

	void fixupSectorAdjoins(std::vector<s32>& adjoinSectorsToFix)
	{
		if (adjoinSectorsToFix.empty())
		{
			return;
		}

		// Fix-up adjoins.
		const s32 adjoinListCount = (s32)adjoinSectorsToFix.size();
		const s32* adjoinListId = adjoinSectorsToFix.data();
		for (s32 i = 0; i < adjoinListCount; i++)
		{
			EditorSector* src = &s_level.sectors[adjoinListId[i]];
			const s32 wallCountSrc = (s32)src->walls.size();
			const Vec2f* vtxSrc = src->vtx.data();
			EditorWall* wallSrc = src->walls.data();
			for (s32 w0 = 0; w0 < wallCountSrc; w0++, wallSrc++)
			{
				if (wallSrc->adjoinId >= 0) { continue; }
				const Vec2f v0 = vtxSrc[wallSrc->idx[0]];
				const Vec2f v1 = vtxSrc[wallSrc->idx[1]];

				for (s32 j = 0; j < adjoinListCount; j++)
				{
					if (i == j) { continue; }
					EditorSector* dst = &s_level.sectors[adjoinListId[j]];
					const s32 wallCountDst = (s32)dst->walls.size();
					Vec2f* vtxDst = dst->vtx.data();
					EditorWall* wallDst = dst->walls.data();
					for (s32 w1 = 0; w1 < wallCountDst; w1++, wallDst++)
					{
						if (wallDst->adjoinId >= 0) { continue; }
						const Vec2f v2 = vtxDst[wallDst->idx[0]];
						const Vec2f v3 = vtxDst[wallDst->idx[1]];
						if (TFE_Polygon::vtxEqual(&v0, &v3) && TFE_Polygon::vtxEqual(&v1, &v2))
						{
							// Make sure the vertices are *exactly* the same.
							vtxDst[wallDst->idx[0]] = v1;
							vtxDst[wallDst->idx[1]] = v0;

							wallSrc->adjoinId = dst->id;
							wallSrc->mirrorId = w1;
							wallDst->adjoinId = src->id;
							wallDst->mirrorId = w0;
							break;
						}
					}
				}
			}
		}
	}

	void addSectorToList(s32 id, std::vector<s32>& list)
	{
		const s32 count = (s32)list.size();
		const s32* ids = list.data();
		for (s32 i = 0; i < count; i++)
		{
			if (ids[i] == id) { return; }
		}
		list.push_back(id);
	}

	void insertIndependentSector(const f32* heights, std::vector<EditorSector*>& candidates, std::vector<s32>& changedSectors)
	{
		EditorSector newSector;
		createNewSector(&newSector, heights);
				
		s32 shapeCount = (s32)s_geoEdit.shape.size();
		newSector.vtx.resize(shapeCount);
		newSector.walls.resize(shapeCount);
		memcpy(newSector.vtx.data(), s_geoEdit.shape.data(), sizeof(Vec2f) * shapeCount);

		EditorWall* wall = newSector.walls.data();
		for (s32 w = 0; w < shapeCount; w++, wall++)
		{
			s32 a = w;
			s32 b = (w + 1) % shapeCount;

			*wall = {};
			wall->idx[0] = a;
			wall->idx[1] = b;

			// Defaults.
			wall->tex[WP_MID].texIndex = getDefaultTextureIndex(WP_MID);
			wall->tex[WP_TOP].texIndex = getDefaultTextureIndex(WP_TOP);
			wall->tex[WP_BOT].texIndex = getDefaultTextureIndex(WP_BOT);
		}

		newSector.groupId = groups_getCurrentId();
		newSector.groupIndex = 0;
		s_level.sectors.push_back(newSector);
		
		EditorSector* newSectorLvl = &s_level.sectors.back();
		sectorToPolygon(newSectorLvl);
		addSectorToList(newSectorLvl->id, changedSectors);

		// Deal with adjoins - note for this mode, edges have to match - new vertices are *NOT* inserted.
		std::vector<s32> adjoinSectorsToFix;
		addToAdjoinFixupList(newSectorLvl->id, adjoinSectorsToFix);
		const s32 overlapCount = (s32)candidates.size();
		const EditorSector* const* candidateList = candidates.data();
		for (s32 i = 0; i < overlapCount; i++)
		{
			addToAdjoinFixupList(candidateList[i]->id, adjoinSectorsToFix);
			addSectorToList(candidateList[i]->id, changedSectors);
		}

		// We need to add the next layer of adjoins to be fixed.
		const s32 nextLayerCount = (s32)adjoinSectorsToFix.size();
		for (s32 n = 0; n < nextLayerCount; n++)
		{
			EditorSector* sector = &s_level.sectors[adjoinSectorsToFix[n]];
			const s32 wallCount = (s32)sector->walls.size();
			EditorWall* wall = sector->walls.data();

			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				if (wall->adjoinId >= 0 && wall->mirrorId >= 0)
				{
					addToAdjoinFixupList(wall->adjoinId, adjoinSectorsToFix);
				}
			}
		}

		fixupSectorAdjoins(adjoinSectorsToFix);
		edit_cleanSectorList(adjoinSectorsToFix, false);

		// Now if the sector was adjoined to anything else, copy its attributes.
		const s32 newWallCount = (s32)newSectorLvl->walls.size();
		EditorWall* newWall = newSectorLvl->walls.data();
		EditorSector* next = nullptr;
		for (s32 w = 0; w < newWallCount; w++, newWall++)
		{
			if (newWall->adjoinId >= 0 && newWall->mirrorId >= 0)
			{
				next = &s_level.sectors[newWall->adjoinId];
				break;
			}
		}

		if (next && !next->walls.empty())
		{
			// Find a source wall that is *not* an adjoin.
			// If they are all adjoins, we'll just use the last wall.
			const s32 nextWallCount = (s32)next->walls.size();
			const EditorWall* srcWall = next->walls.data();
			for (s32 w = 0; w < nextWallCount; w++, srcWall++)
			{
				if (srcWall->adjoinId < 0)
				{
					break;
				}
			}

			// Now apply the textures to the other walls.
			newWall = newSectorLvl->walls.data();
			for (s32 w = 0; w < newWallCount; w++, newWall++)
			{
				for (s32 i = 0; i < WP_COUNT; i++)
				{
					if (i != WP_SIGN)
					{
						newWall->tex[i] = srcWall->tex[i];
					}
				}
			}

			// Finally apply the sector flags and textures.
			for (s32 i = 0; i < 3; i++)
			{
				newSectorLvl->flags[i] = next->flags[i];
			}
			newSectorLvl->ambient = next->ambient;
			newSectorLvl->floorTex = next->floorTex;
			newSectorLvl->ceilTex = next->ceilTex;
			if (s_view == EDIT_VIEW_2D)
			{
				newSectorLvl->floorHeight = next->floorHeight;
				newSectorLvl->ceilHeight = next->ceilHeight;
			}
		}
	}

	bool insertShapeWithBoolean(const f32* heights, std::vector<EditorSector*>& candidates, u32 heightFlags, f32 clipFloorHeight, f32 clipCeilHeight, std::vector<s32>& changedSectors)
	{
		// The shape polygon may be split up during clipping.
		BPolygon shapePolyClip = {};
		const s32 vtxCount = (s32)s_geoEdit.shape.size();
		const Vec2f* vtx = s_geoEdit.shape.data();
		for (s32 i = 0; i < vtxCount; i++)
		{
			TFE_Polygon::addEdgeToBPoly(vtx[i], vtx[(i + 1) % vtxCount], &shapePolyClip);
		}

		//  Intersect the shape polygon with the candidates.
		//    a. If the mode is subtract, then the source polygon does *not* need to be split.
		std::vector<s32> adjoinSectorsToFix;
		const s32 candidateCount = (s32)candidates.size();
		EditorSector** candidateList = candidates.data();
		std::vector<BPolygon> outPoly;
		std::vector<s32> deleteId;

		// Mark and clear adjoins
		for (s32 s = 0; s < candidateCount; s++)
		{
			EditorSector* candidate = candidateList[s];
			const s32 wallCount = (s32)candidate->walls.size();
			EditorWall* wall = candidate->walls.data();

			addSectorToList(candidate->id, changedSectors);

			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				if (wall->adjoinId >= 0 && wall->mirrorId >= 0)
				{
					addToAdjoinFixupList(wall->adjoinId, adjoinSectorsToFix);
					EditorSector* next = &s_level.sectors[wall->adjoinId];
					if (wall->mirrorId < (s32)next->walls.size())
					{
						next->walls[wall->mirrorId].adjoinId = -1;
						next->walls[wall->mirrorId].mirrorId = -1;
					}
					else
					{
						// Invalid mirror, clear out all matching walls.
						const s32 wallCountNext = (s32)next->walls.size();
						EditorWall* wallNext = next->walls.data();
						for (s32 w2 = 0; w2 < wallCountNext; w2++, wallNext++)
						{
							if (wallNext->adjoinId == candidate->id)
							{
								wallNext->adjoinId = -1;
								wallNext->mirrorId = -1;
							}
						}
					}
					wall->adjoinId = -1;
					wall->mirrorId = -1;
				}
			}
		}
		// We need to add the next layer of adjoins to be fixed.
		const s32 nextLayerCount = (s32)adjoinSectorsToFix.size();
		for (s32 n = 0; n < nextLayerCount; n++)
		{
			EditorSector* sector = &s_level.sectors[adjoinSectorsToFix[n]];
			const s32 wallCount = (s32)sector->walls.size();
			EditorWall* wall = sector->walls.data();

			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				if (wall->adjoinId >= 0 && wall->mirrorId >= 0)
				{
					addToAdjoinFixupList(wall->adjoinId, adjoinSectorsToFix);
				}
			}
		}

		std::vector<BPolygon> clipPolyList[2];
		std::vector<BPolygon> candidatePoly;
		EditorSector sectorToCopy = {};
		sectorToCopy.groupId = groups_getCurrentId();
		sectorToCopy.groupIndex = 0;
		s32 curClipRead = 0;
		s32 curClipWrite = 1;
		clipPolyList[curClipRead].push_back(shapePolyClip);

		// Build the candidate polygons.
		const s32 shapeVtxCount = (s32)s_geoEdit.shape.size();
		const Vec2f* shapeVtx = s_geoEdit.shape.data();

		std::vector<BPolygon> cpolyList;
		cpolyList.resize(candidateCount);
		for (s32 s = 0; s < candidateCount; s++)
		{
			EditorSector* candidate = candidateList[s];
			const s32 wallCount = (s32)candidate->walls.size();
			EditorWall* wall = candidate->walls.data();

			BPolygon* cpoly = &cpolyList[s];
			*cpoly = {};

			wall = candidate->walls.data();
			const Vec2f* cvtx = candidate->vtx.data();
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				TFE_Polygon::addEdgeToBPoly(cvtx[wall->idx[0]], cvtx[wall->idx[1]], cpoly);
			}

			// Insert vertices where the shape polygon touches or nearly touches the candidate edges.
			// This is to make sure that points on sloped edges are handled properly.
			addTouchingVertsToPolygon(shapeVtxCount, shapeVtx, cpoly);
		}

		// Clip the candidate polygons with the clip polygon based on draw mode.
		for (s32 s = 0; s < candidateCount; s++)
		{
			EditorSector* candidate = candidateList[s];
			const s32 wallCount = (s32)candidate->walls.size();
			EditorWall* wall = candidate->walls.data();
			BPolygon* cpoly = &cpolyList[s];

			// * Clip the shape polygons to the bpolygon.
			outPoly.clear();
			TFE_Polygon::clipPolygons(cpoly, &shapePolyClip, outPoly, s_geoEdit.boolMode);

			// Then create sectors for each outPoly.
			const s32 outPolyCount = (s32)outPoly.size();
			if (outPolyCount >= 1)
			{
				// Re-use the existing sector for the first result.
				std::vector<EditorWall> walls = candidate->walls;
				updateSectorFromPolygon(candidate, outPoly[0]);
				sectorToPolygon(candidate);
				addToAdjoinFixupList(candidate->id, adjoinSectorsToFix);

				// Handle clip-interior polygons.
				if (!outPoly[0].outsideClipRegion)
				{
					handleClipInteriorPolygons(candidateCount, cpolyList.data(), heightFlags, clipFloorHeight, clipCeilHeight, candidate, &outPoly[0]);
				}

				// Create new sectors for the rest.
				for (s32 p = 1; p < outPolyCount; p++)
				{
					EditorSector newSector = {};
					newSector.id = (s32)s_level.sectors.size();
					copySectorForSplit(candidate, &newSector);
					
					// Handle clip-interior polygons.
					if (!outPoly[p].outsideClipRegion)
					{
						handleClipInteriorPolygons(candidateCount, cpolyList.data(), heightFlags, clipFloorHeight, clipCeilHeight, &newSector, &outPoly[p]);
					}

					s_level.sectors.push_back(newSector);
					EditorSector* newPtr = &s_level.sectors.back();
					addToAdjoinFixupList(newPtr->id, adjoinSectorsToFix);
					addSectorToList(newPtr->id, changedSectors);

					updateSectorFromPolygon(newPtr, outPoly[p]);
					sectorToPolygon(newPtr);
				}

				candidatePoly.insert(candidatePoly.end(), outPoly.begin(), outPoly.end());
				if (sectorToCopy.id == 0)
				{
					sectorToCopy = *candidate;
				}
			}
			else
			{
				deleteId.push_back(candidate->id);
			}
		}

		// * Then, if MERGE, adjust the clip polygons.
		s32 outPolyCount = (s32)candidatePoly.size();
		if (s_geoEdit.boolMode == BMODE_MERGE)
		{
			if (outPolyCount)
			{
				std::vector<Vec2f> insertionPt;

				BPolygon* poly = candidatePoly.data();
				for (s32 p = 0; p < outPolyCount; p++, poly++)
				{
					TFE_Polygon::buildInsertionPointList(poly, &insertionPt);

					s32 clipCount = (s32)clipPolyList[curClipRead].size();
					BPolygon* clipPoly = clipPolyList[curClipRead].data();

					std::vector<BPolygon>& clipWrite = clipPolyList[curClipWrite];
					clipWrite.clear();

					for (s32 c = 0; c < clipCount; c++, clipPoly++)
					{
						std::vector<BPolygon> clipOut;
						TFE_Polygon::clipPolygons(clipPoly, poly, clipOut, BMODE_SUBTRACT);
						clipWrite.insert(clipWrite.end(), clipOut.begin(), clipOut.end());
					}

					std::swap(curClipRead, curClipWrite);
				}

				// Insert shared vertices.
				s32 clipCount = (s32)clipPolyList[curClipRead].size();
				if (clipCount > 0)
				{
					TFE_Polygon::insertPointsIntoPolygons(insertionPt, &clipPolyList[curClipRead]);
				}

				// Final result.
				BPolygon* clipPoly = clipPolyList[curClipRead].data();

				// Create new sectors for the rest.
				for (s32 p = 0; p < clipCount; p++)
				{
					EditorSector newSector = {};
					newSector.id = (s32)s_level.sectors.size();
					copySectorForSplit(&sectorToCopy, &newSector);

					if (heightFlags & HFLAG_SET_FLOOR)
					{
						newSector.floorHeight = clipFloorHeight;
					}
					if (heightFlags & HFLAG_SET_CEIL)
					{
						newSector.ceilHeight = clipCeilHeight;
					}
					newSector.groupId = groups_getCurrentId();
					newSector.groupIndex = 0;
					assert(groups_isIdValid(newSector.groupId));

					s_level.sectors.push_back(newSector);
					EditorSector* newPtr = &s_level.sectors.back();
					addToAdjoinFixupList(newPtr->id, adjoinSectorsToFix);
					addSectorToList(newPtr->id, changedSectors);

					updateSectorFromPolygon(newPtr, clipPoly[p]);
					sectorToPolygon(newPtr);
				}
			}
			else  // Nothing to clip against, just accept it as-is...
			{
				EditorSector newSector;
				createNewSector(&newSector, heights);
				assert(groups_isIdValid(newSector.groupId));

				s_level.sectors.push_back(newSector);
				EditorSector* newPtr = &s_level.sectors.back();
				addToAdjoinFixupList(newPtr->id, adjoinSectorsToFix);
				addSectorToList(newPtr->id, changedSectors);

				updateSectorFromPolygon(newPtr, shapePolyClip);
				sectorToPolygon(newPtr);
			}
		}

		// Fix-up adjoins and clean sectors.
		fixupSectorAdjoins(adjoinSectorsToFix);
		edit_cleanSectorList(adjoinSectorsToFix, false);

		// Delete any sectors - note this should only happen in the SUBTRACT mode unless degenerate sectors are found.
		bool sectorsDeleted = !deleteId.empty();
		if (sectorsDeleted)
		{
			std::sort(deleteId.begin(), deleteId.end());
			const s32 deleteCount = (s32)deleteId.size();
			const s32* idToDel = deleteId.data();
			for (s32 i = deleteCount - 1; i >= 0; i--)
			{
				edit_deleteSector(idToDel[i]);
			}
		}
		return sectorsDeleted;
	}

	// Merge a shape into the existing sectors.
	bool edit_insertShape(const f32* heights, BoolMode mode, s32 firstVertex, bool allowSubsectorExtrude, std::vector<s32>& changedSectors)
	{
		bool hasSectors = !s_level.sectors.empty();
		clearSourceList();
		changedSectors.clear();

		u32 heightFlags = s_view == EDIT_VIEW_3D ? HFLAG_DEFAULT : HFLAG_NONE;
		f32 clipFloorHeight = min(heights[0], heights[1]);
		f32 clipCeilHeight = max(heights[0], heights[1]);
		if (clipFloorHeight == clipCeilHeight)
		{
			heightFlags = HFLAG_NONE;
		}

		// Reserve extra sectors to avoid pointer issues...
		// TODO: This isn't great - this is here to avoid pointers changing during this process.
		s_level.sectors.reserve(s_level.sectors.size() + 256);

		// Find overlap candidates.
		Polygon shapePoly;
		Vec3f shapeBounds[2];
		std::vector<EditorSector*> candidates;
		findShapeOverlapCandidates(heights, allowSubsectorExtrude && mode != BMODE_SET, firstVertex,
			&shapePoly, &candidates, shapeBounds, &heightFlags, &clipFloorHeight, &clipCeilHeight);

		bool sectorsDeleted = false;
		if (mode == BMODE_SET)
		{
			// In this mode, no sector intersections are required. Simply create the sector
			// and then fixup adjoins with existing sectors.
			insertIndependentSector(heights, candidates, changedSectors);
		}
		else
		{
			sectorsDeleted = insertShapeWithBoolean(heights, candidates, heightFlags, clipFloorHeight, clipCeilHeight, changedSectors);
		}
		return sectorsDeleted;
	}

	bool edit_createSectorFromRect(const f32* heights, const Vec2f* vtx, bool allowSubsectorExtrude)
	{
		s_geoEdit.drawStarted = false;
		bool hasSectors = !s_level.sectors.empty();

		const Vec2f* shapeVtx = vtx;

		Vec2f g[2];
		g[0] = posToGrid(shapeVtx[0]);
		g[1] = posToGrid(shapeVtx[1]);

		Vec2f gridCorner[4];
		gridCorner[0] = g[0];
		gridCorner[1] = { g[1].x, g[0].z };
		gridCorner[2] = { g[1].x, g[1].z };
		gridCorner[3] = { g[0].x, g[1].z };

		Vec2f rect[] =
		{
			gridToPos(gridCorner[0]),
			gridToPos(gridCorner[1]),
			gridToPos(gridCorner[2]),
			gridToPos(gridCorner[3])
		};

		// Invert the order if the signed area is negative.
		const f32 area = TFE_Polygon::signedArea(4, rect);
		if (area < 0.0f)
		{
			std::swap(rect[0], rect[3]);
			std::swap(rect[1], rect[2]);
		}

		// Which is first?
		s32 firstVertex = 0;
		for (s32 i = 0; i < 4; i++)
		{
			if (TFE_Polygon::vtxEqual(&rect[i], &shapeVtx[0]))
			{
				firstVertex = i;
				break;
			}
		}

		s_geoEdit.shape.resize(4);
		Vec2f* outVtx = s_geoEdit.shape.data();
		for (s32 v = 0; v < 4; v++)
		{
			outVtx[v] = rect[v];
		}

		std::vector<s32> changedSectors;
		bool sectorsDeleted = edit_insertShape(heights, s_geoEdit.boolMode, firstVertex, allowSubsectorExtrude, changedSectors);

		selection_clearHovered();
		selection_clear(SEL_GEO | SEL_ENTITY_BIT);
		infoPanelClearFeatures();

		if (sectorsDeleted)
		{
			levHistory_createSnapshot("CreateSector-Rect");
		}
		else
		{
			cmd_sectorSnapshot(LName_CreateSectorFromRect, changedSectors);
		}
		return sectorsDeleted;
	}

	bool edit_createSectorFromShape(const f32* heights, s32 vertexCount, const Vec2f* vtx, bool allowSubsectorExtrude)
	{
		s_geoEdit.drawStarted = false;

		const f32 area = TFE_Polygon::signedArea(vertexCount, vtx);
		s32 firstVertex = 0;
		// Make sure the shape has the correct winding order.
		if (area < 0.0f)
		{
			std::vector<Vec2f> outVtxList;
			outVtxList.resize(vertexCount);
			Vec2f* outVtx = outVtxList.data();
			// If area is negative, than the polygon winding is counter-clockwise, so read the vertices in reverse-order.
			for (s32 v = 0; v < vertexCount; v++)
			{
				outVtx[v] = vtx[vertexCount - v - 1];
			}
			firstVertex = vertexCount - 1;
			s_geoEdit.shape = outVtxList;
		}
		//TFE_Polygon::cleanUpShape(s_geoEdit.shape);

		std::vector<s32> changedSectors;
		bool sectorsDeleted = edit_insertShape(heights, s_geoEdit.boolMode, firstVertex, allowSubsectorExtrude, changedSectors);

		selection_clearHovered();
		selection_clear(SEL_GEO | SEL_ENTITY_BIT);
		infoPanelClearFeatures();

		if (sectorsDeleted)
		{
			levHistory_createSnapshot("CreateSector-Shape");
		}
		else
		{
			cmd_sectorSnapshot(LName_CreateSectorFromRect, changedSectors);
		}
		return sectorsDeleted;
	}
		
	Vec3f extrudePoint2dTo3d(const Vec2f pt2d)
	{
		const Vec3f& S = s_geoEdit.extrudePlane.S;
		const Vec3f& T = s_geoEdit.extrudePlane.T;
		const Vec3f& origin = s_geoEdit.extrudePlane.origin;
		const Vec3f pt3d =
		{
			origin.x + pt2d.x*S.x + pt2d.z*T.x,
			origin.y + pt2d.x*S.y + pt2d.z*T.y,
			origin.z + pt2d.x*S.z + pt2d.z*T.z
		};
		return pt3d;
	}

	Vec3f extrudePoint2dTo3d(const Vec2f pt2d, f32 height)
	{
		const Vec3f& S = s_geoEdit.extrudePlane.S;
		const Vec3f& T = s_geoEdit.extrudePlane.T;
		const Vec3f& N = s_geoEdit.extrudePlane.N;
		const Vec3f& origin = s_geoEdit.extrudePlane.origin;
		const Vec3f pt3d =
		{
			origin.x + pt2d.x*S.x + pt2d.z*T.x + height * N.x,
			origin.y + pt2d.x*S.y + pt2d.z*T.y + height * N.y,
			origin.z + pt2d.x*S.z + pt2d.z*T.z + height * N.z
		};
		return pt3d;
	}

	s32 insertVertexIntoSector(EditorSector* sector, Vec2f newVtx)
	{
		const s32 vtxCount = (s32)sector->vtx.size();
		const Vec2f* vtx = sector->vtx.data();
		for (s32 v = 0; v < vtxCount; v++, vtx++)
		{
			if (TFE_Polygon::vtxEqual(vtx, &newVtx))
			{
				return v;
			}
		}
		sector->vtx.push_back(newVtx);
		return vtxCount;
	}

	bool canCreateNewAdjoin(const EditorWall* wall, const EditorSector* sector)
	{
		bool canAdjoin = true;
		if (wall->adjoinId >= 0 && wall->mirrorId >= 0)
		{
			// This is still valid *IF* the passage is blocked.
			const EditorSector* next = &s_level.sectors[wall->adjoinId];
			const f32 top = min(sector->ceilHeight, next->ceilHeight);
			const f32 bot = max(sector->floorHeight, next->floorHeight);
			canAdjoin = (top - bot) <= 0.0f;
		}
		return canAdjoin;
	}

	bool edgesColinear(const Vec2f* points, s32& newPointCount, s32* newPoints)
	{
		const f32 eps = 0.00001f;
		newPointCount = 0;

		// Is v0 or v1 on v2->v3?
		Vec2f point;
		f32 s = TFE_Polygon::closestPointOnLineSegment(points[2], points[3], points[0], &point);
		if (s > -eps && s < 1.0f + eps && TFE_Polygon::vtxEqual(&points[0], &point)) { newPoints[newPointCount++] = 0 | EDGE1; }

		s = TFE_Polygon::closestPointOnLineSegment(points[2], points[3], points[1], &point);
		if (s > -eps && s < 1.0f + eps && TFE_Polygon::vtxEqual(&points[1], &point)) { newPoints[newPointCount++] = 1 | EDGE1; }

		// Is v2 or v3 on v0->v1?
		s = TFE_Polygon::closestPointOnLineSegment(points[0], points[1], points[2], &point);
		if (s > -eps && s < 1.0f + eps && TFE_Polygon::vtxEqual(&points[2], &point)) { newPoints[newPointCount++] = 2 | EDGE0; }

		s = TFE_Polygon::closestPointOnLineSegment(points[0], points[1], points[3], &point);
		if (s > -eps && s < 1.0f + eps && TFE_Polygon::vtxEqual(&points[3], &point)) { newPoints[newPointCount++] = 3 | EDGE0; }

		const bool areColinear = newPointCount >= 2;
		if (newPointCount > 2)
		{
			s32 p0 = newPoints[0] & 255, p1 = newPoints[1] & 255, p2 = newPoints[2] & 255;
			s32 d0 = -1, d1 = -1;
			if (TFE_Polygon::vtxEqual(&points[p0], &points[p1]))
			{
				d0 = 0;
				d1 = 1;
			}
			else if (TFE_Polygon::vtxEqual(&points[p0], &points[p2]))
			{
				d0 = 0;
				d1 = 2;
			}
			else if (TFE_Polygon::vtxEqual(&points[p1], &points[p2]))
			{
				d0 = 1;
				d1 = 2;
			}

			// Disambiguate duplicates.
			if (d0 >= 0 && d1 >= 0)
			{
				// Keep the one that isn't already on the segment.
				s32 p01 = newPoints[d0] & 255;
				s32 p11 = newPoints[d1] & 255;
				s32 edge0 = newPoints[d0] >> 8;
				s32 edge1 = newPoints[d1] >> 8;
				s32 discard = d0;
				if (edge0 != edge1)
				{
					if ((edge1 == 0 && (TFE_Polygon::vtxEqual(&points[p11], &points[0]) || TFE_Polygon::vtxEqual(&points[p11], &points[1]))) ||
						(edge1 == 1 && (TFE_Polygon::vtxEqual(&points[p11], &points[2]) || TFE_Polygon::vtxEqual(&points[p11], &points[3]))))
					{
						discard = d1;
					}
				}
				for (s32 i = discard; i < newPointCount - 1; i++)
				{
					newPoints[i] = newPoints[i + 1];
				}
				newPointCount--;
			}
		}
		return areColinear && !TFE_Polygon::vtxEqual(&points[newPoints[0] & 255], &points[newPoints[1] & 255]);
	}

	void mergeAdjoins(EditorSector* sector0)
	{
		const s32 levelSectorCount = (s32)s_level.sectors.size();
		EditorSector* levelSectors = s_level.sectors.data();

		s32 id0 = sector0->id;

		// Build a list ahead of time, instead of traversing through all sectors multiple times.
		std::vector<EditorSector*> mergeList;
		for (s32 i = 0; i < levelSectorCount; i++)
		{
			EditorSector* sector1 = &levelSectors[i];
			if (sector1 == sector0) { continue; }

			// Cannot merge adjoins if the sectors don't overlap in 3D space.
			if (!aabbOverlap3d(sector0->bounds, sector1->bounds))
			{
				continue;
			}
			mergeList.push_back(sector1);
		}

		// This loop needs to be more careful with overlaps.
		// Loop through each wall of the new sector and see if it can be adjoined to another sector/wall in the list created above.
		// Once colinear walls are found, properly splits are made and the loop is restarted. Adjoins are only added when exact
		// matches are found.
		bool restart = true;
		while (restart)
		{
			restart = false;
			const s32 sectorCount = (s32)mergeList.size();
			EditorSector** sectorList = mergeList.data();
			sector0 = &s_level.sectors[id0];

			s32 wallCount0 = (s32)sector0->walls.size();
			EditorWall* wall0 = sector0->walls.data();
			bool wallLoop = true;
			for (s32 w0 = 0; w0 < wallCount0 && wallLoop; w0++, wall0++)
			{
				if (!canCreateNewAdjoin(wall0, sector0)) { continue; }
				const Vec2f* v0 = &sector0->vtx[wall0->idx[0]];
				const Vec2f* v1 = &sector0->vtx[wall0->idx[1]];

				// Now check for overlaps in other sectors in the list.
				bool foundMirror = false;
				for (s32 s1 = 0; s1 < sectorCount && !foundMirror; s1++)
				{
					EditorSector* sector1 = sectorList[s1];
					const s32 wallCount1 = (s32)sector1->walls.size();
					EditorWall* wall1 = sector1->walls.data();

					for (s32 w1 = 0; w1 < wallCount1 && !foundMirror; w1++, wall1++)
					{
						if (!canCreateNewAdjoin(wall1, sector1)) { continue; }
						const Vec2f* v2 = &sector1->vtx[wall1->idx[0]];
						const Vec2f* v3 = &sector1->vtx[wall1->idx[1]];

						// Do the vertices match exactly?
						if (TFE_Polygon::vtxEqual(v0, v3) && TFE_Polygon::vtxEqual(v1, v2))
						{
							// Found an adjoin!
							wall0->adjoinId = sector1->id;
							wall0->mirrorId = w1;
							wall1->adjoinId = sector0->id;
							wall1->mirrorId = w0;
							// For now, assume one adjoin per wall.
							foundMirror = true;
							continue;
						}

						// Are these edges co-linear?
						s32 newPointCount;
						s32 newPoints[4];
						Vec2f points[] = { *v0, *v1, *v2, *v3 };
						bool collinear = edgesColinear(points, newPointCount, newPoints);
						if (collinear && newPointCount == 2)
						{
							// 4 Cases:
							// 1. v0,v1 left of v2,v3
							bool wallSplit = false;
							if ((newPoints[0] >> 8) == 0 && (newPoints[1] >> 8) == 1)
							{
								assert((newPoints[0] & 255) >= 2);
								assert((newPoints[1] & 255) < 2);

								// Insert newPoints[0] & 255 into wall0
								wallSplit |= edit_splitWall(sector0->id, w0, points[newPoints[0] & 255]);
								// Insert newPoints[1] & 255 into wall1
								wallSplit |= edit_splitWall(sector1->id, w1, points[newPoints[1] & 255]);
							}
							// 2. v0,v1 right of v2,v3
							else if ((newPoints[0] >> 8) == 1 && (newPoints[1] >> 8) == 0)
							{
								assert((newPoints[0] & 255) < 2);
								assert((newPoints[1] & 255) >= 2);

								// Insert newPoints[0] & 255 into wall1
								wallSplit |= edit_splitWall(sector1->id, w1, points[newPoints[0] & 255]);
								// Insert newPoints[1] & 255 into wall0
								wallSplit |= edit_splitWall(sector0->id, w0, points[newPoints[1] & 255]);
							}
							// 3. v2,v3 inside of v0,v1
							else if ((newPoints[0] >> 8) == 0 && (newPoints[1] >> 8) == 0)
							{
								assert((newPoints[0] & 255) >= 2);
								assert((newPoints[1] & 255) >= 2);

								// Insert newPoints[0] & 255 into wall0
								// Insert newPoints[1] & 255 into wall0
								if (edit_splitWall(sector0->id, w0, points[newPoints[1] & 255])) { w0++; wallSplit |= true; }
								wallSplit |= edit_splitWall(sector0->id, w0, points[newPoints[0] & 255]);
							}
							// 4. v0,v1 inside of v2,v3
							else if ((newPoints[0] >> 8) == 1 && (newPoints[1] >> 8) == 1)
							{
								assert((newPoints[0] & 255) < 2);
								assert((newPoints[1] & 255) < 2);

								// Insert newPoints[0] & 255 into wall1
								// Insert newPoints[1] & 255 into wall1
								if (edit_splitWall(sector1->id, w1, points[newPoints[1] & 255])) { w1++; wallSplit |= true; }
								wallSplit |= edit_splitWall(sector1->id, w1, points[newPoints[0] & 255]);
							}
							if (wallSplit)
							{
								restart = true;
								wallLoop = false;
								break;
							}
						}
					}
				}
			}
		}

		const s32 sectorCount = (s32)mergeList.size();
		EditorSector** sectorList = mergeList.data();
		for (s32 s = 0; s < sectorCount; s++)
		{
			EditorSector* sector = sectorList[s];
			polygonToSector(sector);
		}
		polygonToSector(sector0);
	}

	s32 extrudeSectorFromRect()
	{
		const Project* proj = project_get();
		const bool dualAdjoinSupported = proj->featureSet != FSET_VANILLA || proj->game != Game_Dark_Forces;

		s_geoEdit.drawStarted = false;
		s_geoEdit.extrudeEnabled = false;
		s32 nextWallId = -1;

		const f32 minDelta = 1.0f / 64.0f;
		EditorSector* sector = &s_level.sectors[s_geoEdit.extrudePlane.sectorId];
		assert(s_geoEdit.extrudePlane.sectorId >= 0 && s_geoEdit.extrudePlane.sectorId < (s32)s_level.sectors.size());

		// Extrude away - create a new sector.
		if (s_geoEdit.drawHeight[1] > 0.0f)
		{
			EditorWall* extrudeWall = &sector->walls[s_geoEdit.extrudePlane.wallId];
			assert(s_geoEdit.extrudePlane.wallId >= 0 && s_geoEdit.extrudePlane.wallId < (s32)sector->walls.size());

			// Does the wall already have an ajoin?
			if (extrudeWall->adjoinId >= 0 && !dualAdjoinSupported)
			{
				LE_ERROR("Cannot complete operation:");
				LE_ERROR("  Dual adjoins are not supported using this FeatureSet.");
				return -1;
			}
			const f32 floorHeight = std::min(s_geoEdit.shape[0].z, s_geoEdit.shape[1].z) + s_geoEdit.extrudePlane.origin.y;
			const f32 ceilHeight = std::max(s_geoEdit.shape[0].z, s_geoEdit.shape[1].z) + s_geoEdit.extrudePlane.origin.y;

			// When extruding inward, copy the mid-texture to the top and bottom.
			extrudeWall->tex[WP_BOT] = extrudeWall->tex[WP_MID];
			extrudeWall->tex[WP_TOP] = extrudeWall->tex[WP_MID];
			extrudeWall->tex[WP_TOP].offset.z += (ceilHeight - sector->floorHeight);

			const f32 x0 = std::min(s_geoEdit.shape[0].x, s_geoEdit.shape[1].x);
			const f32 x1 = std::max(s_geoEdit.shape[0].x, s_geoEdit.shape[1].x);
			const f32 d = s_geoEdit.drawHeight[1];
			if (fabsf(ceilHeight - floorHeight) >= minDelta && fabsf(x1 - x0) >= minDelta)
			{
				// Build the sector shape.
				EditorSector newSector;
				const f32 heights[] = { floorHeight, ceilHeight };
				createNewSector(&newSector, heights);
				// Copy from the sector being extruded from.
				newSector.floorTex = sector->floorTex;
				newSector.ceilTex = sector->ceilTex;
				newSector.ambient = sector->ambient;

				s32 count = 4;
				newSector.vtx.resize(count);
				newSector.walls.resize(count);
				const Vec3f rect[] =
				{
					extrudePoint2dTo3d({x0, 0.0f}, s_geoEdit.drawHeight[0]),
					extrudePoint2dTo3d({x0, 0.0f}, s_geoEdit.drawHeight[1]),
					extrudePoint2dTo3d({x1, 0.0f}, s_geoEdit.drawHeight[1]),
					extrudePoint2dTo3d({x1, 0.0f}, s_geoEdit.drawHeight[0])
				};

				Vec2f* outVtx = newSector.vtx.data();
				for (s32 v = 0; v < count; v++)
				{
					outVtx[v] = { rect[v].x, rect[v].z };
				}

				EditorWall* wall = newSector.walls.data();
				for (s32 w = 0; w < count; w++, wall++)
				{
					const s32 a = w;
					const s32 b = (w + 1) % count;

					*wall = {};
					wall->idx[0] = a;
					wall->idx[1] = b;

					// Copy from the wall being extruded from.
					s32 texIndex = extrudeWall->tex[WP_MID].texIndex;
					if (texIndex < 0) { texIndex = getTextureIndex("DEFAULT.BM"); }
					wall->tex[WP_MID].texIndex = texIndex;
					wall->tex[WP_TOP].texIndex = texIndex;
					wall->tex[WP_BOT].texIndex = texIndex;
				}

				// Split the wall if needed.
				s32 extrudeSectorId = s_geoEdit.extrudePlane.sectorId;
				s32 extrudeWallId = s_geoEdit.extrudePlane.wallId;
				nextWallId = extrudeWallId;
				if (x0 > 0.0f)
				{
					if (edit_splitWall(extrudeSectorId, extrudeWallId, { rect[0].x, rect[0].z }))
					{
						extrudeWallId++;
						nextWallId++;
					}
				}
				if (x1 < s_geoEdit.extrudePlane.ext.x)
				{
					if (edit_splitWall(extrudeSectorId, extrudeWallId, { rect[3].x, rect[3].z }))
					{
						nextWallId++;
					}
				}

				// Link wall 3 to the wall extruded from.
				wall = &newSector.walls.back();
				wall->adjoinId = extrudeSectorId;
				wall->mirrorId = extrudeWallId;

				EditorWall* exWall = &s_level.sectors[extrudeSectorId].walls[extrudeWallId];
				exWall->adjoinId = (s32)s_level.sectors.size();
				exWall->mirrorId = count - 1;

				newSector.groupId = groups_getCurrentId();
				s_level.sectors.push_back(newSector);
				sectorToPolygon(&s_level.sectors.back());

				mergeAdjoins(&s_level.sectors.back());

				selection_clearHovered();
				selection_clear(SEL_GEO | SEL_ENTITY_BIT);
				infoPanelClearFeatures();
			}
		}
		// Extrude inward - adjust the current sector.
		else if (s_geoEdit.drawHeight[1] < 0.0f)
		{
			const f32 floorHeight = std::min(s_geoEdit.shape[0].z, s_geoEdit.shape[1].z) + s_geoEdit.extrudePlane.origin.y;
			const f32 ceilHeight = std::max(s_geoEdit.shape[0].z, s_geoEdit.shape[1].z) + s_geoEdit.extrudePlane.origin.y;
			const f32 x0 = std::min(s_geoEdit.shape[0].x, s_geoEdit.shape[1].x);
			const f32 x1 = std::max(s_geoEdit.shape[0].x, s_geoEdit.shape[1].x);
			const f32 d = -s_geoEdit.drawHeight[1];
			nextWallId = s_geoEdit.extrudePlane.wallId;

			EditorWall* extrudeWall = s_geoEdit.extrudePlane.wallId >= 0 && s_geoEdit.extrudePlane.wallId < (s32)sector->walls.size() ? &sector->walls[s_geoEdit.extrudePlane.wallId] : nullptr;
			if (extrudeWall && extrudeWall->tex[WP_MID].texIndex >= 0)
			{
				s_newWallTexOverride = extrudeWall->tex[WP_MID].texIndex;
				if (extrudeWall->adjoinId >= 0)
				{
					if (floorHeight == sector->floorHeight && extrudeWall->tex[WP_BOT].texIndex >= 0)
					{
						s_newWallTexOverride = extrudeWall->tex[WP_BOT].texIndex;
					}
					else if (ceilHeight == sector->ceilHeight && extrudeWall->tex[WP_TOP].texIndex >= 0)
					{
						s_newWallTexOverride = extrudeWall->tex[WP_TOP].texIndex;
					}
				}
			}

			if (!dualAdjoinSupported && floorHeight != sector->floorHeight && ceilHeight != sector->ceilHeight)
			{
				LE_ERROR("Cannot complete operation:");
				LE_ERROR("  Dual adjoins are not supported using this FeatureSet.");
				return -1;
			}

			if (fabsf(ceilHeight - floorHeight) >= minDelta && fabsf(x1 - x0) >= minDelta)
			{
				s32 count = 4;
				s_geoEdit.shape.resize(count);
				const Vec3f rect[] =
				{
					extrudePoint2dTo3d({x0, 0.0f}, s_geoEdit.drawHeight[0]),
					extrudePoint2dTo3d({x1, 0.0f}, s_geoEdit.drawHeight[0]),
					extrudePoint2dTo3d({x1, 0.0f}, s_geoEdit.drawHeight[1]),
					extrudePoint2dTo3d({x0, 0.0f}, s_geoEdit.drawHeight[1])
				};

				Vec2f* outVtx = s_geoEdit.shape.data();
				for (s32 v = 0; v < count; v++)
				{
					outVtx[v] = { rect[v].x, rect[v].z };
				}

				// Merge
				f32 heights[2];
				bool forceSubtract = false;
				if (floorHeight == sector->floorHeight && ceilHeight == sector->ceilHeight)
				{
					heights[0] = sector->floorHeight;
					heights[1] = sector->ceilHeight;
					forceSubtract = true;
				}
				else if (floorHeight == sector->floorHeight)
				{
					heights[0] = ceilHeight;
					heights[1] = sector->ceilHeight;
				}
				else if (ceilHeight == sector->ceilHeight)
				{
					heights[0] = sector->floorHeight;
					heights[1] = floorHeight;
				}

				BoolMode boolMode = s_geoEdit.boolMode;
				s_geoEdit.boolMode = forceSubtract ? BMODE_SUBTRACT : boolMode;
				edit_createSectorFromShape(heights, 4, outVtx, false);
				s_geoEdit.boolMode = boolMode;
			}
			s_newWallTexOverride = -1;
		}

		return nextWallId;
	}

	void addSplitToExtrude(std::vector<f32>& splits, f32 x)
	{
		// Does it already exist?
		s32 count = (s32)splits.size();
		f32* splitList = splits.data();
		for (s32 i = 0; i < count; i++)
		{
			if (splitList[i] == x)
			{
				return;
			}
		}
		splits.push_back(x);
	}

	std::vector<Vec2f> s_testShape;

	void getExtrudeHeightExtends(f32 x, Vec2f* ext)
	{
		*ext = { FLT_MAX, -FLT_MAX };

		const s32 vtxCount = (s32)s_testShape.size();
		const Vec2f* vtx = s_testShape.data();
		for (s32 v = 0; v < vtxCount; v++)
		{
			const Vec2f* v0 = &vtx[v];
			const Vec2f* v1 = &vtx[(v + 1) % vtxCount];
			// Only check edges with a horizontal component.
			if (v0->x == v1->x) { continue; }

			const f32 u = (x - v0->x) / (v1->x - v0->x);
			if (u < -FLT_EPSILON || u > 1.0f + FLT_EPSILON)
			{
				continue;
			}

			const f32 z = v0->z + u * (v1->z - v0->z);
			ext->x = min(ext->x, z);
			ext->z = max(ext->z, z);
		}
	}

	void extrudeSectorFromShape()
	{
		s_geoEdit.drawStarted = false;
		s_geoEdit.extrudeEnabled = false;

		// Split the shape into multiple components.
		const s32 vtxCount = (s32)s_geoEdit.shape.size();
		std::vector<f32> splits;

		// Add the splits.
		for (s32 v = 0; v < vtxCount; v++)
		{
			addSplitToExtrude(splits, s_geoEdit.shape[v].x);
		}
		// Sort them from lowest to higest.
		std::sort(splits.begin(), splits.end());
		// Then find the height extends of each split.
		s32 extrudeSectorId = s_geoEdit.extrudePlane.sectorId;
		s32 extrudeWallId = s_geoEdit.extrudePlane.wallId;

		s_testShape = s_geoEdit.shape;
		s_geoEdit.shape.resize(2);
		const s32 splitCount = (s32)splits.size();
		for (s32 i = 0; i < splitCount - 1 && extrudeWallId >= 0; i++)
		{
			Vec2f ext;
			f32 xHalf = (splits[i] + splits[i + 1]) * 0.5f;
			getExtrudeHeightExtends(xHalf, &ext);
			if (ext.x < FLT_MAX && ext.z > -FLT_MAX)
			{
				s_geoEdit.shape[0] = { splits[i], ext.x };
				s_geoEdit.shape[1] = { splits[i + 1], ext.z };
				s_geoEdit.extrudePlane.sectorId = extrudeSectorId;
				s_geoEdit.extrudePlane.wallId = extrudeWallId;

				extrudeWallId = extrudeSectorFromRect();
			}
		}
	}

	void removeLastShapePoint()
	{
		// Remove the last place vertex.
		if (!s_geoEdit.shape.empty())
		{
			s_geoEdit.shape.pop_back();
		}
		// If no vertices are left, cancel drawing.
		if (s_geoEdit.shape.empty())
		{
			s_geoEdit.drawStarted = false;
		}
	}

	void handleSectorExtrude(RayHitInfo* hitInfo)
	{
		const Project* project = project_get();
		const bool allowSlopes = project->featureSet != FSET_VANILLA || project->game != Game_Dark_Forces;

		EditorSector* hoverSector = nullptr;
		EditorWall* hoverWall = nullptr;

		// Get the hover sector in 2D.
		if (!s_geoEdit.drawStarted)
		{
			s_geoEdit.extrudeEnabled = false;
			return;
		}

		// Snap the cursor to the grid.
		Vec2f onGrid = { s_cursor3d.x, s_cursor3d.z };

		const f32 maxLineDist = 0.5f * s_grid.size;
		Vec2f newPos;
		FeatureId featureId;
		if (snapToLine(onGrid, maxLineDist, newPos, featureId))
		{
			s32 wallIndex;
			EditorSector* sector = unpackFeatureId(featureId, &wallIndex);

			// Snap the result to the surface grid.
			Vec3f snapPos = { newPos.x, s_grid.height, newPos.z };
			snapToSurfaceGrid(sector, &sector->walls[wallIndex], snapPos);
			onGrid = { snapPos.x, snapPos.z };
		}
		else
		{
			snapToGrid(&onGrid);
		}

		s_cursor3d.x = onGrid.x;
		s_cursor3d.z = onGrid.z;
		snapToGridY(&s_cursor3d.y);

		// Project the shape onto the polygon.
		if (s_view == EDIT_VIEW_3D && s_geoEdit.drawStarted)
		{
			// Project using the extrude plane.
			const Vec3f offset = { s_cursor3d.x - s_geoEdit.extrudePlane.origin.x, s_cursor3d.y - s_geoEdit.extrudePlane.origin.y, s_cursor3d.z - s_geoEdit.extrudePlane.origin.z };
			onGrid.x = offset.x*s_geoEdit.extrudePlane.S.x + offset.y*s_geoEdit.extrudePlane.S.y + offset.z*s_geoEdit.extrudePlane.S.z;
			onGrid.z = offset.x*s_geoEdit.extrudePlane.T.x + offset.y*s_geoEdit.extrudePlane.T.y + offset.z*s_geoEdit.extrudePlane.T.z;

			// Clamp.
			onGrid.x = max(0.0f, min(s_geoEdit.extrudePlane.ext.x, onGrid.x));
			onGrid.z = max(0.0f, min(s_geoEdit.extrudePlane.ext.z, onGrid.z));
		}

		if (s_geoEdit.drawStarted)
		{
			s_geoEdit.drawCurPos = onGrid;
			if (s_geoEdit.drawMode == DMODE_RECT)
			{
				s_geoEdit.shape[1] = onGrid;
				if (!TFE_Input::mouseDown(MouseButton::MBUTTON_LEFT))
				{
					if (s_view == EDIT_VIEW_3D)
					{
						s_geoEdit.drawMode = DMODE_RECT_VERT;
						s_curVtxPos = s_cursor3d;
					}
					else
					{
						s_geoEdit.drawHeight[1] = s_geoEdit.drawHeight[0] + c_defaultSectorHeight;
						createSectorFromRect();
					}
				}
			}
			else if (s_geoEdit.drawMode == DMODE_SHAPE)
			{
				if (s_singleClick)
				{
					if (TFE_Polygon::vtxEqual(&onGrid, &s_geoEdit.shape[0]))
					{
						if (s_geoEdit.shape.size() >= 3)
						{
							// Need to form a polygon.
							if (s_view == EDIT_VIEW_3D)
							{
								s_geoEdit.drawMode = DMODE_SHAPE_VERT;
								s_curVtxPos = s_cursor3d;
								shapeToPolygon((s32)s_geoEdit.shape.size(), s_geoEdit.shape.data(), s_geoEdit.shapePolygon);
							}
							else
							{
								s_geoEdit.drawHeight[1] = s_geoEdit.drawHeight[0] + c_defaultSectorHeight;
								createSectorFromShape();
							}
						}
						else
						{
							s_geoEdit.drawStarted = false;
							s_geoEdit.extrudeEnabled = false;
						}
					}
					else
					{
						// Make sure the vertex hasn't already been placed.
						const s32 count = (s32)s_geoEdit.shape.size();
						const Vec2f* vtx = s_geoEdit.shape.data();
						bool vtxExists = false;
						for (s32 i = 0; i < count; i++, vtx++)
						{
							if (TFE_Polygon::vtxEqual(vtx, &onGrid))
							{
								vtxExists = true;
								break;
							}
						}

						if (!vtxExists)
						{
							vtx = count > 0 ? &s_geoEdit.shape.back() : nullptr;
							const bool error = !allowSlopes && vtx && onGrid.x != vtx->x && onGrid.z != vtx->z;
							if (error)
							{
								LE_ERROR("Cannot complete operation:");
								LE_ERROR("  Slopes are not supported using this feature set.");
							}
							else
							{
								s_geoEdit.shape.push_back(onGrid);
							}
						}
					}
				}
				else if (s_drawActions & DRAW_ACTION_CANCEL)
				{
					removeLastShapePoint();
				}
				else if ((s_drawActions & DRAW_ACTION_FINISH) || s_doubleClick)
				{
					if (s_geoEdit.shape.size() >= 3)
					{
						if (s_view == EDIT_VIEW_3D)
						{
							s_geoEdit.drawMode = DMODE_SHAPE_VERT;
							s_curVtxPos = s_cursor3d;
							shapeToPolygon((s32)s_geoEdit.shape.size(), s_geoEdit.shape.data(), s_geoEdit.shapePolygon);
						}
						else
						{
							s_geoEdit.drawHeight[1] = s_geoEdit.drawHeight[0] + c_defaultSectorHeight;
							createSectorFromShape();
						}
					}
					else
					{
						// Accept the entire wall, which is a quad.
						if (s_geoEdit.shape.size() <= 1)
						{
							s_geoEdit.shape.resize(2);
							s_geoEdit.shape[0] = { 0.0f, 0.0f };
							s_geoEdit.shape[1] = { s_geoEdit.extrudePlane.ext.x, s_geoEdit.extrudePlane.ext.z };
							s_geoEdit.drawMode = DMODE_RECT_VERT;
							s_curVtxPos = s_cursor3d;
						}
						else
						{
							s_geoEdit.drawStarted = false;
							s_geoEdit.extrudeEnabled = false;
						}
					}
				}
			}
			else if (s_geoEdit.drawMode == DMODE_RECT_VERT || s_geoEdit.drawMode == DMODE_SHAPE_VERT)
			{
				if ((s_drawActions & DRAW_ACTION_CANCEL) && s_geoEdit.drawMode == DMODE_SHAPE_VERT)
				{
					s_geoEdit.drawMode = DMODE_SHAPE;
					s_geoEdit.drawHeight[1] = s_geoEdit.drawHeight[0];
					removeLastShapePoint();
				}
				else if (s_singleClick)
				{
					if (s_geoEdit.drawMode == DMODE_SHAPE_VERT) { extrudeSectorFromShape(); }
					else { extrudeSectorFromRect(); }

					// TODO: Replace with local sector snapshot.
					levHistory_createSnapshot("Extrude Sector");
				}

				const Vec3f worldPos = moveAlongRail(s_geoEdit.extrudePlane.N);
				const Vec3f offset = { worldPos.x - s_curVtxPos.x, worldPos.y - s_curVtxPos.y, worldPos.z - s_curVtxPos.z };
				s_geoEdit.drawHeight[1] = offset.x*s_geoEdit.extrudePlane.N.x + offset.y*s_geoEdit.extrudePlane.N.y + offset.z*s_geoEdit.extrudePlane.N.z;
				snapToGridY(&s_geoEdit.drawHeight[1]);
			}
		}
	}

	void editGeometry_init()
	{
		s_geoEdit = {};
	}

	s32 getCurveSegDelta()
	{
		return s_curveSegDelta;
	}

	void setCurveSegDelta(s32 newDelta)
	{
		s_curveSegDelta = newDelta;
	}
		
	void buildCurve(const Vec2f& a, const Vec2f& b, const Vec2f& c, std::vector<Vec2f>* curve)
	{
		const Vec2f ac = { a.x - c.x, a.z - c.z };
		const Vec2f bc = { b.x - c.x, b.z - c.z };

		// 1. Get the approximate arc length.
		const f32 arcLen = getQuadraticBezierArcLength(a, b, c);

		// 2. Determine the number of segments from the length.
		const s32 segCount = max(s32(arcLen / s_editorConfig.curve_segmentSize) + s_curveSegDelta, 2);

		// Clamp s_curveSegDelta.
		s32 clampValue = 2 - s32(arcLen / s_editorConfig.curve_segmentSize);
		if (s_curveSegDelta < clampValue)
		{
			s_curveSegDelta = clampValue;
		}

		// TODO: Support even wall lengths versus denser sampling in areas with more change.
		// Even sampling will be more expensive and approximate, so default to variable sampling.

		// 3. Determine the delta.
		const f32 dt = 1.0f / f32(segCount);

		// 4. Generate the curve.
		// Skip the first point - already encoded.
		f32 t = dt;
		for (s32 i = 1; i < segCount; i++, t += dt)
		{
			const Vec2f p = evaluateQuadraticBezier(ac, bc, c, t);
			curve->push_back(p);
		}
		// Final point.
		curve->push_back(b);
	}

	void finishSectorDraw(Vec2f onGrid, bool handlePartial)
	{
		// Try to handle as a partial shape, if it fails the shape will be unmodified.
		if (handlePartial) { handlePartialShape(); }
		if (s_geoEdit.shape.size() >= 3)
		{
			if (s_view == EDIT_VIEW_3D && s_geoEdit.boolMode != BMODE_SUBTRACT)
			{
				s_geoEdit.drawMode = DMODE_SHAPE_VERT;
				s_curVtxPos = { onGrid.x, s_grid.height, onGrid.z };
				shapeToPolygon((s32)s_geoEdit.shape.size(), s_geoEdit.shape.data(), s_geoEdit.shapePolygon);
			}
			else
			{
				s_geoEdit.drawHeight[1] = s_geoEdit.drawHeight[0] + c_defaultSectorHeight;
				createSectorFromShape();
			}
		}
		else
		{
			s_geoEdit.drawStarted = false;
		}
	}

	void handleSectorDraw(RayHitInfo* hitInfo)
	{
		EditorSector* hoverSector = nullptr;

		// Get the hover sector in 2D.
		if (s_view == EDIT_VIEW_2D)
		{
			hoverSector = edit_getHoverSector2dAtCursor();
		}
		else if (hitInfo->hitSectorId >= 0) // Or the hit sector in 3D.
		{
			hoverSector = &s_level.sectors[hitInfo->hitSectorId];
		}

		// Snap the cursor to the grid.
		Vec2f onGrid = { s_cursor3d.x, s_cursor3d.z };

		const f32 maxLineDist = 0.5f * s_grid.size;
		Vec2f newPos;
		FeatureId featureId;
		if (snapToLine(onGrid, maxLineDist, newPos, featureId))
		{
			s32 index;
			s32 type;
			EditorSector* sector = unpackFeatureId(featureId, &index, &type);

			// Snap the result to the surface grid.
			if (type == 0)
			{
				Vec3f snapPos = { newPos.x, s_grid.height, newPos.z };
				snapToSurfaceGrid(sector, &sector->walls[index], snapPos);
				onGrid = { snapPos.x, snapPos.z };
			}
			else if (type == 1)
			{
				onGrid = newPos;
			}
			// Snap to the grid if drawing has already started.
			if (s_geoEdit.drawStarted)
			{
				s_cursor3d.y = s_grid.height;
			}
		}
		else if (s_view == EDIT_VIEW_2D)
		{
			onGrid = snapToGridOrGuidelines2d(onGrid);
		}
		else
		{
			// snap to the closer: 3d cursor position or grid intersection.
			Vec3f posOnGrid;
			if (gridCursorIntersection(&posOnGrid) && s_geoEdit.drawStarted)
			{
				s_cursor3d = posOnGrid;
				onGrid = { s_cursor3d.x, s_cursor3d.z };
			}
			onGrid = snapToGridOrGuidelines2d(onGrid);
		}

		s_cursor3d.x = onGrid.x;
		s_cursor3d.z = onGrid.z;

		// Hot Key
		const bool heightMove = isShortcutHeld(SHORTCUT_MOVE_GRID);

		// Two ways to draw: rectangle (shift + left click and drag, escape to cancel), shape (left click to start, backspace to go back one point, escape to cancel)
		if (s_geoEdit.gridMoveStart)
		{
			if (!TFE_Input::mouseDown(MouseButton::MBUTTON_LEFT))
			{
				s_geoEdit.gridMoveStart = false;
			}
			else
			{
				Vec3f worldPos = moveAlongRail({ 0.0f, 1.0f, 0.0f });
				f32 yValue = worldPos.y;
				snapToGridY(&yValue);
				s_grid.height = yValue;
			}
		}
		else if (s_geoEdit.drawStarted)
		{
			s_geoEdit.drawCurPos = onGrid;
			if (s_geoEdit.drawMode == DMODE_RECT)
			{
				s_geoEdit.shape[1] = onGrid;
				if (!TFE_Input::mouseDown(MouseButton::MBUTTON_LEFT))
				{
					if (s_view == EDIT_VIEW_3D && s_geoEdit.boolMode != BMODE_SUBTRACT)
					{
						s_geoEdit.drawMode = DMODE_RECT_VERT;
						s_curVtxPos = { onGrid.x, s_grid.height, onGrid.z };
					}
					else
					{
						s_geoEdit.drawHeight[1] = s_geoEdit.drawHeight[0] + c_defaultSectorHeight;
						createSectorFromRect();
					}
				}
			}
			else if (s_geoEdit.drawMode == DMODE_SHAPE)
			{
				if (s_singleClick)
				{
					if (s_drawActions & DRAW_ACTION_CURVE)
					{
						// New mode: place control point.
						s_geoEdit.drawMode = DMODE_CURVE_CONTROL;
						// Compute the starting control point position.
						s_geoEdit.controlPoint.x = (onGrid.x + s_geoEdit.shape.back().x) * 0.5f;
						s_geoEdit.controlPoint.z = (onGrid.z + s_geoEdit.shape.back().z) * 0.5f;
					}

					if (TFE_Polygon::vtxEqual(&onGrid, &s_geoEdit.shape[0]))
					{
						s_geoEdit.shapeComplete = true;
						if (s_geoEdit.drawMode == DMODE_SHAPE)
						{
							finishSectorDraw(onGrid, false);
						}
						else if (s_geoEdit.drawMode == DMODE_CURVE_CONTROL)
						{
							s_geoEdit.shape.push_back(onGrid);
						}
					}
					else
					{
						// Make sure the vertex hasn't already been placed.
						const s32 count = (s32)s_geoEdit.shape.size();
						const Vec2f* vtx = s_geoEdit.shape.data();
						bool vtxExists = false;
						for (s32 i = 0; i < count; i++, vtx++)
						{
							if (TFE_Polygon::vtxEqual(vtx, &onGrid))
							{
								vtxExists = true;
								break;
							}
						}
						if (!vtxExists)
						{
							s_geoEdit.shape.push_back(onGrid);
						}
					}
				}
				else if (s_drawActions & DRAW_ACTION_CANCEL)
				{
					removeLastShapePoint();
				}
				else if ((s_drawActions & DRAW_ACTION_FINISH) || s_doubleClick)
				{
					finishSectorDraw(onGrid, true);
				}
			}
			else if (s_geoEdit.drawMode == DMODE_CURVE_CONTROL)
			{
				s_geoEdit.controlPoint = onGrid;

				// Click to finalize the curve.
				if (s_drawActions & DRAW_ACTION_CANCEL)
				{
					s_geoEdit.drawMode = DMODE_SHAPE;
					removeLastShapePoint();
				}
				else if ((s_drawActions & DRAW_ACTION_FINISH) || s_doubleClick)
				{
					s32 vtxCount = (s32)s_geoEdit.shape.size();
					std::vector<Vec2f> curve;

					Vec2f lastPoint = s_geoEdit.shape[vtxCount - 1];
					buildCurve(s_geoEdit.shape[vtxCount - 2], lastPoint, s_geoEdit.controlPoint, &curve);
					s_geoEdit.shape.pop_back();
					s_geoEdit.shape.insert(s_geoEdit.shape.end(), curve.begin(), curve.end());
					
					finishSectorDraw(lastPoint, true);
				}
				else if (s_singleClick)
				{
					s32 vtxCount = (s32)s_geoEdit.shape.size();
					std::vector<Vec2f> curve;
					buildCurve(s_geoEdit.shape[vtxCount-2], s_geoEdit.shape[vtxCount - 1], s_geoEdit.controlPoint, &curve);
					// Remove the last point since it is being replaced by the curve.
					s_geoEdit.shape.pop_back();
					// Insert the curve.
					s_geoEdit.shape.insert(s_geoEdit.shape.end(), curve.begin(), curve.end());
					// Change back to shape draw mode.
					s_geoEdit.drawMode = DMODE_SHAPE;
					// If the last point and first point is the same, the shape is complete and we're done drawing it.
					if (s_geoEdit.shapeComplete)
					{
						// Remove the last vertex, since it is a duplicate of the first.
						s_geoEdit.shape.pop_back();
						// Finish.
						finishSectorDraw(onGrid, false);
					}
				}
			}
			else if (s_geoEdit.drawMode == DMODE_RECT_VERT || s_geoEdit.drawMode == DMODE_SHAPE_VERT)
			{
				if ((s_drawActions & DRAW_ACTION_CANCEL) && s_geoEdit.drawMode == DMODE_SHAPE_VERT)
				{
					s_geoEdit.drawMode = DMODE_SHAPE;
					s_geoEdit.drawHeight[1] = s_geoEdit.drawHeight[0];
					removeLastShapePoint();
				}
				else if (s_singleClick)
				{
					if (s_geoEdit.drawMode == DMODE_SHAPE_VERT) { createSectorFromShape(); }
					else { createSectorFromRect(); }
				}

				Vec3f worldPos = moveAlongRail({ 0.0f, 1.0f, 0.0f });
				s_geoEdit.drawHeight[1] = worldPos.y;
				snapToGridY(&s_geoEdit.drawHeight[1]);
			}
		}
		else if (heightMove && s_singleClick)
		{
			s_geoEdit.gridMoveStart = true;
			s_curVtxPos = { onGrid.x, s_grid.height, onGrid.z };
		}
		else if (s_singleClick)
		{
			if (hoverSector && !(s_gridFlags & GFLAG_OVER))
			{
				s_grid.height = hoverSector->floorHeight;
				if (s_view == EDIT_VIEW_3D)
				{
					if (fabsf(s_cursor3d.y - hoverSector->floorHeight) < FLT_EPSILON)
					{
						s_grid.height = hoverSector->floorHeight;
					}
					else if (fabsf(s_cursor3d.y - hoverSector->ceilHeight) < FLT_EPSILON)
					{
						s_grid.height = hoverSector->ceilHeight;
					}
					else if (hitInfo->hitWallId >= 0 && (hitInfo->hitPart == HP_BOT || hitInfo->hitPart == HP_TOP || hitInfo->hitPart == HP_MID))
					{
						s_geoEdit.extrudeEnabled = true;

						// Extrude from wall.
						s_geoEdit.extrudePlane.sectorId = hoverSector->id;
						s_geoEdit.extrudePlane.wallId = hitInfo->hitWallId;

						EditorWall* hitWall = &hoverSector->walls[hitInfo->hitWallId];
						const Vec2f* p0 = &hoverSector->vtx[hitWall->idx[0]];
						const Vec2f* p1 = &hoverSector->vtx[hitWall->idx[1]];

						s_geoEdit.extrudePlane.origin = { p0->x, hoverSector->floorHeight, p0->z };
						const Vec3f S = { p1->x - p0->x, 0.0f, p1->z - p0->z };
						const Vec3f T = { 0.0f, 1.0f, 0.0f };
						const Vec3f N = { -(p1->z - p0->z), 0.0f, p1->x - p0->x };

						s_geoEdit.extrudePlane.ext.x = TFE_Math::distance(p0, p1);
						s_geoEdit.extrudePlane.ext.z = hoverSector->ceilHeight - hoverSector->floorHeight;

						s_geoEdit.extrudePlane.S = TFE_Math::normalize(&S);
						s_geoEdit.extrudePlane.N = TFE_Math::normalize(&N);
						s_geoEdit.extrudePlane.T = T;

						// Project using the extrude plane.
						Vec3f wallPos = s_cursor3d;
						snapToSurfaceGrid(hoverSector, hitWall, wallPos);

						const Vec3f offset = { wallPos.x - s_geoEdit.extrudePlane.origin.x, wallPos.y - s_geoEdit.extrudePlane.origin.y, wallPos.z - s_geoEdit.extrudePlane.origin.z };
						onGrid.x = offset.x*s_geoEdit.extrudePlane.S.x + offset.y*s_geoEdit.extrudePlane.S.y + offset.z*s_geoEdit.extrudePlane.S.z;
						onGrid.z = offset.x*s_geoEdit.extrudePlane.T.x + offset.y*s_geoEdit.extrudePlane.T.y + offset.z*s_geoEdit.extrudePlane.T.z;
					}
				}
			}

			s_geoEdit.drawStarted = true;
			s_geoEdit.shapeComplete = false;
			s_geoEdit.drawMode = TFE_Input::keyModDown(KEYMOD_SHIFT) ? DMODE_RECT : DMODE_SHAPE;
			if (s_geoEdit.extrudeEnabled)
			{
				s_geoEdit.drawHeight[0] = 0.0f;
				s_geoEdit.drawHeight[1] = 0.0f;
			}
			else
			{
				s_geoEdit.drawHeight[0] = s_grid.height;
				s_geoEdit.drawHeight[1] = s_grid.height;
			}
			s_geoEdit.drawCurPos = onGrid;

			s_geoEdit.shape.clear();
			if (s_geoEdit.drawMode == DMODE_RECT)
			{
				s_geoEdit.shape.resize(2);
				s_geoEdit.shape[0] = onGrid;
				s_geoEdit.shape[1] = onGrid;
			}
			else
			{
				s_geoEdit.shape.push_back(onGrid);
			}
		}
		else
		{
			s_geoEdit.drawCurPos = onGrid;
		}
	}
		
	void createSectorFromRect()
	{
		edit_createSectorFromRect(s_geoEdit.drawHeight, s_geoEdit.shape.data());
	}

	void createSectorFromShape()
	{
		edit_createSectorFromShape(s_geoEdit.drawHeight, (s32)s_geoEdit.shape.size(), s_geoEdit.shape.data());
	}

	// Form a path between each open end using edges on the map.
	// Edges come from potentially overlapping sectors (in 2D + same layer + interactible group).
	// Since we don't care about the source sectors, the clipper will deal with that,
	// we can just add all of the edges to test in a bucket and build up an adjacency map.
	struct PathEdge
	{
		Vec2f v0 = { 0 };
		Vec2f v1 = { 0 };
		s32 index = 0;
		// Each edge may link to multiple edges along a specific direction.
		std::vector<s32> nextEdge;
		std::vector<s32> prevEdge;
	};

	struct EdgeMatch
	{
		PathEdge* edge;
		f32 u;
	};
	typedef std::vector<const PathEdge*> EdgePath;
	std::vector<PathEdge> s_adjacencyMap;

	bool isInIntList(s32 value, std::vector<s32>* list)
	{
		const s32 count = (s32)list->size();
		const s32* values = list->data();
		for (s32 i = 0; i < count; i++)
		{
			if (value == values[i])
			{
				return true;
			}
		}
		return false;
	}

	void insertIntoIntList(s32 value, std::vector<s32>* list)
	{
		const s32 count = (s32)list->size();
		const s32* values = list->data();
		for (s32 i = 0; i < count; i++)
		{
			if (value == values[i])
			{
				return;
			}
		}
		list->push_back(value);
	}

	void addEdgeToAdjacencyMap(Vec2f v0, Vec2f v1)
	{
		const s32 count = (s32)s_adjacencyMap.size();
		PathEdge* edge = s_adjacencyMap.data();

		// Verify it isn't in the list already and add to any edges.
		for (s32 e = 0; e < count; e++, edge++)
		{
			if (TFE_Polygon::vtxEqual(&v0, &edge->v0) && TFE_Polygon::vtxEqual(&v1, &edge->v1))
			{
				// This edge is already in the map.
				return;
			}
		}

		// Setup new edge
		PathEdge newEdge = {};
		newEdge.v0 = v0;
		newEdge.v1 = v1;
		newEdge.index = count;

		// Find any connections.
		edge = s_adjacencyMap.data();
		for (s32 e = 0; e < count; e++, edge++)
		{
			if (TFE_Polygon::vtxEqual(&v0, &edge->v1))
			{
				insertIntoIntList(newEdge.index, &edge->nextEdge);
				insertIntoIntList(e, &newEdge.prevEdge);
			}
			else if (TFE_Polygon::vtxEqual(&v1, &edge->v0))
			{
				insertIntoIntList(newEdge.index, &edge->prevEdge);
				insertIntoIntList(e, &newEdge.nextEdge);
			}
		}

		// Insert.
		s_adjacencyMap.push_back(newEdge);
	}

	void buildAdjacencyMap()
	{
		// 1. compute the bounds.
		const s32 vtxCount = (s32)s_geoEdit.shape.size();
		const Vec2f* vtx = s_geoEdit.shape.data();
		Vec3f shapeBounds[2];
		shapeBounds[0] = { vtx[0].x, 0.0f, vtx[0].z };
		shapeBounds[1] = { vtx[0].x, 0.0f, vtx[0].z };
		for (s32 v = 1; v < vtxCount; v++)
		{
			shapeBounds[0].x = min(shapeBounds[0].x, vtx[v].x);
			shapeBounds[0].z = min(shapeBounds[0].z, vtx[v].z);
			shapeBounds[1].x = max(shapeBounds[1].x, vtx[v].x);
			shapeBounds[1].z = max(shapeBounds[1].z, vtx[v].z);
		}
		f32 extend = 0.01f;
		shapeBounds[0].x -= extend;
		shapeBounds[0].z -= extend;
		shapeBounds[1].x += extend;
		shapeBounds[1].z += extend;

		// 2. Add sector edges to the map.
		const s32 sectorCount = (s32)s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		s_adjacencyMap.clear();
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if (!sector_isInteractable(sector) || !sector_onActiveLayer(sector)) { continue; }
			// The bounds have to overlap.
			if (!aabbOverlap2d(shapeBounds, sector->bounds)) { continue; }

			// Add the walls to the map.
			const s32 wallCount = (s32)sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vtx.data();
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				const Vec2f v0 = vtx[wall->idx[0]];
				const Vec2f v1 = vtx[wall->idx[1]];
				addEdgeToAdjacencyMap(v0, v1);
			}
		}
	}

	bool whichEdges(const Vec2f vtx, std::vector<EdgeMatch>* edgeList)
	{
		const f32 eps = 0.001f;

		edgeList->clear();
		const s32 count = (s32)s_adjacencyMap.size();
		PathEdge* edge = s_adjacencyMap.data();
		for (s32 i = 0; i < count; i++, edge++)
		{
			if (TFE_Polygon::vtxEqual(&vtx, &edge->v0))
			{
				edgeList->push_back({ edge, 0.0f });
			}
			else if (TFE_Polygon::vtxEqual(&vtx, &edge->v1))
			{
				edgeList->push_back({ edge, 1.0f });
			}
			else
			{
				Vec2f vI;
				const f32 u = TFE_Polygon::closestPointOnLineSegment(edge->v0, edge->v1, vtx, &vI);
				if (u < FLT_EPSILON || u > 1.0f - FLT_EPSILON)
				{
					continue;
				}

				const Vec2f offset = { vI.x - vtx.x, vI.z - vtx.z };
				const f32 distSq = offset.x*offset.x + offset.z*offset.z;
				if (distSq < eps)
				{
					edgeList->push_back({ edge, u });
				}
			}
		}
		return !edgeList->empty();
	}

	struct SearchNode
	{
		s32 distToStart;
		s32 edgeIndex;
		s32 prevIndex;
	};

	s32 findMinimumDistInQueue(std::vector<SearchNode*>& queue)
	{
		const s32 count = (s32)queue.size();
		const SearchNode* const* nodeList = queue.data();
		s32 minDist = INT_MAX;
		s32 minNode = -1;
		for (s32 i = 0; i < count; i++)
		{
			const SearchNode* node = nodeList[i];
			if (node->distToStart < minDist)
			{
				minNode = i;
				minDist = node->distToStart;
			}
		}
		return minNode;
	}

	// Dijkstra
	s32 findEdgePath(EdgeMatch* e0, EdgeMatch* e1, s32 dir, EdgePath* path)
	{
		const s32 count = (s32)s_adjacencyMap.size();
		std::vector<SearchNode> nodes;
		std::vector<SearchNode*> queue;
		nodes.resize(count);

		// Initialize nodes.
		SearchNode* nodeList = nodes.data();
		PathEdge* edge = s_adjacencyMap.data();
		for (s32 i = 0; i < count; i++, edge++)
		{
			SearchNode* node = &nodeList[i];
			if (edge == e0->edge)
			{
				*node = { 0, i, -1 };
			}
			else
			{
				*node = { INT_MAX, i, -1 };
			}
			queue.push_back(node);
		}

		// Process the Queue until all nodes are visited.
		while (!queue.empty())
		{
			const s32 indexInQueue = findMinimumDistInQueue(queue);
			if (indexInQueue < 0)
			{
				break;
			}

			SearchNode* node = queue[indexInQueue];
			PathEdge* edge = &s_adjacencyMap[node->edgeIndex];
			queue.erase(queue.begin() + indexInQueue);

			// Which direction?
			const s32 count = (dir == 1) ? (s32)edge->nextEdge.size() : (s32)edge->prevEdge.size();
			const s32* edgeIndex = (dir == 1) ? edge->nextEdge.data() : edge->prevEdge.data();

			// Update the distance to source values for all connections going in the correct direction (if any).
			const s32 nextDist = node->distToStart + 1;
			const s32 prevIndex = node->edgeIndex;
			for (s32 i = 0; i < count; i++)
			{
				PathEdge* next = &s_adjacencyMap[edgeIndex[i]];
				SearchNode* nextNode = &nodeList[next->index];
				if (nextDist < nextNode->distToStart)
				{
					nextNode->distToStart = nextDist;
					nextNode->prevIndex = prevIndex;
				}
			}
		}

		// Finally go to the target and walk backwards to build the path.
		path->clear();
		PathEdge* curEdge = e1->edge;
		PathEdge* startEdge = e0->edge;
		while (curEdge != startEdge)
		{
			path->push_back(curEdge);

			SearchNode* node = &nodeList[curEdge->index];
			// There is no path from the end back to the start.
			if (node->prevIndex < 0)
			{
				return 0;
			}
			curEdge = &s_adjacencyMap[node->prevIndex];
		}
		// Something went wrong - we still couldn't find our way back...
		if (curEdge != startEdge)
		{
			return 0;
		}
		path->push_back(startEdge);
		return (s32)path->size();
	}

	void handlePartialShape()
	{
		s32 count = (s32)s_geoEdit.shape.size();
		Vec2f* shapeVtx = s_geoEdit.shape.data();
		// Nothing to work with.
		if (count < 1) { return; }

		// If the end points are equal than something went wrong.
		Vec2f v0 = shapeVtx[0];
		Vec2f v1 = shapeVtx[count - 1];
		if (TFE_Polygon::vtxEqual(&v0, &v1))
		{
			return;
		}

		// Build the adjacency map for path finding.
		buildAdjacencyMap();

		// Find possible edges.
		std::vector<EdgeMatch> edgeList0;
		std::vector<EdgeMatch> edgeList1;
		if (!whichEdges(v0, &edgeList0) || !whichEdges(v1, &edgeList1))
		{
			// No edges found.
			return;
		}

		// Are they both on the same edge?
		const s32 edgeCount0 = (s32)edgeList0.size();
		const s32 edgeCount1 = (s32)edgeList1.size();
		EdgeMatch* edge0 = edgeList0.data();
		for (s32 e0 = 0; e0 < edgeCount0; e0++, edge0++)
		{
			EdgeMatch* edge1 = edgeList1.data();
			for (s32 e1 = 0; e1 < edgeCount1; e1++, edge1++)
			{
				if (edge0->edge == edge1->edge)
				{
					// Both points are on the same edge, so the straight line connection is correct.
					return;
				}
			}
		}

		// Finally search from a valid path from edgeList0 -> edgeList1
		edge0 = edgeList0.data();
		EdgePath curPath;
		EdgePath shortestPath;
		s32 shortestLen = INT_MAX;
		Vec2f newVtx[32];
		s32 newVtxCount = 0;
		const f32 eps = 0.001f;
		for (s32 e0 = 0; e0 < edgeCount0; e0++, edge0++)
		{
			EdgeMatch* edge1 = edgeList1.data();
			for (s32 e1 = 0; e1 < edgeCount1; e1++, edge1++)
			{
				s32 pathLen = findEdgePath(edge0, edge1, 1, &curPath);
				if (pathLen > 0 && pathLen < shortestLen && pathLen <= 32)
				{
					shortestPath = curPath;
					shortestLen = pathLen;

					newVtxCount = 0;
					if (edge1->u > eps)
					{
						newVtx[newVtxCount++] = edge1->edge->v0;
					}
					for (s32 p = 1; p < pathLen - 1; p++)
					{
						newVtx[newVtxCount++] = curPath[p]->v0;
					}
					if (edge0->u <= eps)
					{
						newVtx[newVtxCount++] = edge0->edge->v0;
					}
				}
				pathLen = findEdgePath(edge0, edge1, 0, &curPath);
				if (pathLen > 0 && pathLen < shortestLen && pathLen <= 32)
				{
					shortestPath = curPath;
					shortestLen = pathLen;

					newVtxCount = 0;
					if (edge1->u < 1.0f - eps)
					{
						newVtx[newVtxCount++] = edge1->edge->v1;
					}
					for (s32 p = 1; p < pathLen - 1; p++)
					{
						newVtx[newVtxCount++] = curPath[p]->v1;
					}
					if (edge0->u >= 1.0f - eps)
					{
						newVtx[newVtxCount++] = edge0->edge->v1;
					}
				}
			}
		}
		// No path found.
		if (shortestPath.empty())
		{
			return;
		}

		// Now insert the path into the shape.
		for (s32 i = 0; i < newVtxCount; i++)
		{
			s_geoEdit.shape.push_back(newVtx[i]);
		}
		// Sometimes we get the first vertex again.
		if (TFE_Polygon::vtxEqual(&newVtx[newVtxCount - 1], &s_geoEdit.shape[0]))
		{
			s_geoEdit.shape.pop_back();
		}
	}

	f32 cro(Vec2f a, Vec2f b) { return a.x*b.z - a.z*b.x; }
	// Computes cos(acos(x)/3), see https://www.shadertoy.com/view/WltSD7
	f32 cos_acos_3(f32 x)
	{
		x = sqrtf(0.5f + 0.5f*x);
		return x * (x*(x*(x*-0.008972f + 0.039071f) - 0.107074f) + 0.576975f) + 0.5f;
	}

	// Analytical signed distance from a point to Quadratic Bezier curve,
	// which requires solving a cubic.
	f32 signedDistQuadraticBezier(const Vec2f& p0, const Vec2f& p1, const Vec2f& pc, const Vec2f& pos, f32& t)
	{
		const Vec2f a = { pc.x - p0.x, pc.z - p0.z };
		const Vec2f b = { p0.x - 2.0f*pc.x + p1.x, p0.z - 2.0f*pc.z + p1.z };
		const Vec2f c = { a.x * 2.0f, a.z * 2.0f };
		const Vec2f d = { p0.x - pos.x, p0.z - pos.z };

		// cubic to be solved.
		const f32 kk = 1.0f / (b.x*b.x + b.z*b.z);
		const f32 kx = kk * (a.x*b.x + a.z*b.z);
		const f32 ky = kk * (2.0f*(a.x*a.x + a.z*a.z) + (d.x*b.x + d.z*b.z)) / 3.0f;
		const f32 kz = kk * (d.x*a.x + d.z*a.z);
				
		const f32 p = ky - kx * kx;
		const f32 q = kx * (2.0f*kx*kx - 3.0f*ky) + kz;
		const f32 p3 = p * p * p;
		const f32 q2 = q * q;

		f32 h = q2 + 4.0f*p3;
		f32 res = 0.0f, sgn = 0.0f;

		if (h >= 0.0f)
		{
			// 1 root.
			h = sqrtf(h);
			Vec2f x = { (h - q) * 0.5f, (-h - q) * 0.5f };
			const f32 pw = 1.0f / 3.0f;
			t = (f32)sign(x.x) * powf(fabsf(x.x), pw) + (f32)sign(x.z) * powf(fabsf(x.z), pw);
			// from NinjaKoala - single newton iteration to account for cancellation
			t -= (t*(t*t + 3.0f*p) + q) / (3.0f*t*t + 3.0f*p);
			t = clamp(t - kx, 0.0f, 1.0f);

			Vec2f w = { d.x + (c.x + b.x * t)*t, d.z + (c.z + b.z * t)*t };
			res = w.x*w.x + w.z*w.z;
			sgn = cro({ c.x + 2.0f*b.x*t, c.z + 2.0f*b.z*t }, w);
		}
		else
		{
			// 3 roots.
			const f32 z = sqrtf(-p);
			const f32 m = cos_acos_3(q / (p*z*2.0f));
			const f32 n = sqrtf(1.0f - m * m) * sqrtf(3.0f);
			const f32 t0 = clamp((m + m)*z - kx, 0.0f, 1.0f);
			const f32 t1 = clamp((-n - m)*z - kx, 0.0f, 1.0f);
			// t0
			const Vec2f qx = { d.x + (c.x + b.x*t0)*t0, d.z + (c.z + b.z*t0)*t0 };
			const f32 dx = qx.x*qx.x + qx.z*qx.z;
			const f32 sx = cro({ a.x + b.x*t0, a.z + b.z*t0 }, qx);
			// t1
			const Vec2f qz = { d.x + (c.x + b.x*t1)*t1, d.z + (c.z + b.z*t1)*t1 };
			const f32 dz = qz.x*qz.x + qz.z*qz.z;
			const f32 sz = cro({ a.x + b.x*t1, a.z + b.z*t1 }, qz);
			// Which one?
			if (dx < dz)
			{
				res = dx;
				sgn = sx;
				t = t0;
			}
			else
			{
				res = dz;
				sgn = sz;
				t = t1;
			}
		}
		return sqrtf(res) * sign(sgn);
	}

	// Evaluate the Quadratic Bezier curve with end-points at (a, b) and center (c) at (t).
	// pos is required, normal is optional.
	void evaluateQuadraticBezier(const Vec2f& a, const Vec2f& b, const Vec2f& c, f32 t, Vec2f* pos, Vec2f* nrm)
	{
		if (!pos) { return; }

		const f32 t0 = t, t1 = 1.0f - t;
		const Vec2f p0 = { c.x + t1 * (a.x - c.x), c.z + t1 * (a.z - c.z) };
		const Vec2f p1 = { c.x + t0 * (b.x - c.x), c.z + t0 * (b.z - c.z) };
		// Tangent line is (p0, p1), Normal is perpendicular ("perp product").
		if (nrm)
		{
			const Vec2f n = { -(p1.z - p0.z), (p1.x - p0.x) };
			*nrm = TFE_Math::normalize(&n);
		}
		*pos = { p0.x + t*(p1.x - p0.x), p0.z + t*(p1.z - p0.z) };
	}

	// Evaluate the Quadratic Bezier curve with end-points at (a, b) and center (c) at (t)
	// where ac = a - c; bc = b - c.
	// Returns the position.
	Vec2f evaluateQuadraticBezier(const Vec2f& ac, const Vec2f& bc, const Vec2f& c, f32 t)
	{
		const f32 t0 = t, t1 = 1.0f - t;
		const Vec2f p0 = { c.x + t1 * ac.x, c.z + t1 * ac.z };
		const Vec2f p1 = { c.x + t0 * bc.x, c.z + t0 * bc.z };
		return { p0.x + t * (p1.x - p0.x), p0.z + t * (p1.z - p0.z) };
	}
	   
	f32 getQuadraticBezierArcLength(const Vec2f& a, const Vec2f& b, const Vec2f& c, f32 t, s32 maxIterationCount, Vec2f* table)
	{
		// Approximate the total length to the current position.
		// The stepCount controls the accuracy, higher values = more accurate,
		// but slower.
		const s32 stepCount = maxIterationCount;
		const f32 ds = 1.0f / f32(stepCount);

		Vec2f p0 = a;
		f32 s = ds;
		f32 len = 0.0f;

		Vec2f p1 = { 0 };
		for (s32 i = 0; i < stepCount && s < t; i++, s += ds)
		{
			evaluateQuadraticBezier(a, b, c, s, &p1);
			len += TFE_Math::distance(&p0, &p1);
			p0 = p1;
			if (table)
			{
				table[i].x = s;
				table[i].z = len;
			}
		}
		// Take the remainder to get to the current position on the curve.
		evaluateQuadraticBezier(a, b, c, t, &p1);
		len += TFE_Math::distance(&p0, &p1);
		if (table)
		{
			table[stepCount - 1].x = t;
			table[stepCount - 1].z = len;
		}

		return len;
	}
}

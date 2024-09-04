#include "editTransforms.h"
#include "editCommon.h"
#include "hotkeys.h"
#include "levelEditor.h"
#include "editVertex.h"
#include "sharedState.h"
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_Editor/LevelEditor/Rendering/gizmo.h>
#include <TFE_System/math.h>
#include <TFE_Input/input.h>
#include <algorithm>

using namespace TFE_Editor;

namespace LevelEditor
{
	enum RotationAction
	{
		RACTION_NONE = 0,
		RACTION_ROTATE,
		RACTION_ROTATE_SNAP,
		RACTION_MOVE,
		RACTION_COUNT
	};

	static TransformMode s_transformMode = TRANS_MOVE;
	static WallMoveMode s_wallMoveMode = WMM_NORMAL;
	static RotationGizmoPart s_rotHover = RGP_NONE;
	static Vec3f s_center = { 0 };
	static Vec3f s_rotation = { 0 };
	static Vec2f s_rotationStartDir = { 0 };
	static std::vector<Vec2f> s_vertexData;
	static std::vector<s32> s_effectedSectors;
	static f32 s_rotationSide = 0.0f;
	static RotationAction s_rotAction = RACTION_NONE;
	static bool s_enableMoveTransform = false;
	static bool s_transformChange = false;
	static bool s_transformToolActive = false;
	static bool s_dataCaptured = false;

	void moveFeatures(const Vec3f& newPos);
	void moveFlat();
	void moveWall(Vec3f worldPos);
	void moveSector(Vec3f worldPos);
	void captureRotationData();
	void applyRotationToData(f32 angle);

	void edit_setTransformMode(TransformMode mode)
	{
		if (mode != s_transformMode)
		{
			s_transformMode = mode;
			edit_setTransformChange();
		}
	}

	TransformMode edit_getTransformMode()
	{
		return s_transformMode;
	}

	void edit_enableMoveTransform(bool enable)
	{
		s_enableMoveTransform = enable;
	}

	void edit_setWallMoveMode(WallMoveMode mode)
	{
		s_wallMoveMode = mode;
	}

	// Call when the selection changes so that the transform can be reset.
	void edit_setTransformChange()
	{
		s_transformChange = true;
	}
		
	bool edit_isTransformToolActive()
	{
		return s_transformToolActive;
	}

	Vec3f edit_getTransformCenter()
	{
		return s_center;
	}

	Vec3f edit_getTransformRotation()
	{
		return s_rotation;
	}

	u32 edit_getTransformRotationHover()
	{
		return (u32)s_rotHover;
	}

	bool edit_interactingWithGizmo()
	{
		bool interacting = false;
		if (s_transformToolActive)
		{
			// Rotation
			if (s_transformMode == TRANS_ROTATE && s_rotHover != RGP_NONE) { interacting = true; }
			// TODO: Other tools.
		}
		return interacting;
	}

	bool edit_gizmoChangedGeo()
	{
		return edit_interactingWithGizmo() && s_dataCaptured;
	}

	void edit_resetGizmo()
	{
		s_rotHover = RGP_NONE;
		s_dataCaptured = false;
	}

	void edit_applyTransformChange()
	{
		if (!s_transformChange) { return; }
		edit_endTransform();

		s_transformChange = false;
		s_transformToolActive = false;

		if (s_transformMode == TRANS_ROTATE)
		{
			s_center = { 0 };
			s_rotation = { 0 };
			s_rotAction = RACTION_NONE;
			s_rotHover = RGP_NONE;
			s_dataCaptured = false;

			// Find the center of rotation.
			if (s_editMode == LEDIT_VERTEX || s_editMode == LEDIT_WALL || s_editMode == LEDIT_SECTOR)
			{
				const u32 vertexCount = selection_getCount(SEL_VERTEX);
				if (vertexCount)
				{
					s_transformToolActive = true;

					// Get bounds based on vertex set.
					s32 vertexIndex = -1;
					EditorSector* sector = nullptr;
					Vec3f boundsMin = { FLT_MAX, FLT_MAX, FLT_MAX };
					Vec3f boundsMax = { -FLT_MAX,-FLT_MAX,-FLT_MAX };

					for (u32 v = 0; v < vertexCount; v++)
					{
						selection_getVertex(v, sector, vertexIndex);
						const Vec2f& vtx = sector->vtx[vertexIndex];
						boundsMin.x = std::min(boundsMin.x, vtx.x);
						boundsMin.z = std::min(boundsMin.z, vtx.z);
						boundsMax.x = std::max(boundsMax.x, vtx.x);
						boundsMax.z = std::max(boundsMax.z, vtx.z);

						boundsMin.y = std::min(boundsMin.y, sector->floorHeight);
					}
					s_center.x = (boundsMin.x + boundsMax.x) * 0.5f;
					s_center.z = (boundsMin.z + boundsMax.z) * 0.5f;
					s_center.y = boundsMin.y;
				}
			}
			else if (s_editMode == LEDIT_GUIDELINES)
			{
			}
			else if (s_editMode == LEDIT_NOTES)
			{
			}
		}
		else if (s_transformMode == TRANS_SCALE)
		{
		}
	}

	f32 getAngleDifference(f32 a0, f32 a1)
	{
		f32 diff = a1 - a0;
		if (diff > 180.0f) { diff -= 180.0f; }
		else if (diff < -180.0f) { diff += 180.0f; }
		return diff;
	}
		
	f32 edit_getRotationGizmoAngle()
	{
		f32 angleDelta = 0.0f;
		const f32 diff = s_rotation.y - s_rotation.x;
		if (s_rotation.z > 0.0f)
		{
			if (diff < 0.0f)
			{
				angleDelta = 2.0f*PI + (s_rotation.y - s_rotation.x) * PI/180.0f;
			}
			else
			{
				angleDelta = diff * PI / 180.0f;
			}

			if (angleDelta > 2.0f * PI)
			{
				angleDelta -= (2.0f * PI);
			}
		}
		else if (s_rotation.z < 0.0f)
		{
			if (diff > 0.0f)
			{
				angleDelta = 2.0f*PI + (s_rotation.y - s_rotation.x) * PI/180.0f;
			}
			else
			{
				angleDelta = diff * PI / 180.0f;
			}

			if (angleDelta > 2.0f * PI)
			{
				angleDelta -= (4.0f * PI);
			}
		}
		return angleDelta;
	}

	f32 edit_getRotationGizmoAngleDegrees()
	{
		f32 angleDelta = 0.0f;
		const f32 diff = s_rotation.y - s_rotation.x;
		if (s_rotation.z > 0.0f)
		{
			angleDelta = diff;
			if (diff < 0.0f)
			{
				angleDelta = 360.0f + (s_rotation.y - s_rotation.x);
			}

			if (angleDelta > 360.0f)
			{
				angleDelta -= 360.0f;
			}
		}
		else if (s_rotation.z < 0.0f)
		{
			angleDelta = diff;
			if (diff > 0.0f)
			{
				angleDelta = 360.0f + (s_rotation.y - s_rotation.x);
			}

			if (angleDelta > 360.0f)
			{
				angleDelta -= 720.0f;
			}
		}
		return angleDelta;
	}

	void edit_transform(s32 mx, s32 my)
	{
		edit_applyTransformChange();

		if (s_transformMode == TRANS_MOVE && s_editMove)
		{
			if (s_view == EDIT_VIEW_2D)
			{
				const Vec2f worldPos2d = mouseCoordToWorldPos2d(mx, my);
				moveFeatures({ worldPos2d.x, 0.0f, worldPos2d.z });
			}
			else if (s_view == EDIT_VIEW_3D)
			{
				Vec3f worldPos;
				if (s_editMode == LEDIT_VERTEX)
				{
					const f32 u = (s_curVtxPos.y - s_camera.pos.y) / (s_cursor3d.y - s_camera.pos.y);
					worldPos =
					{
						s_camera.pos.x + u * (s_cursor3d.x - s_camera.pos.x),
						0.0f,
						s_camera.pos.z + u * (s_cursor3d.z - s_camera.pos.z)
					};
				}
				else // if (s_editMode == LEDIT_WALL)
				{
					// When wall editing is enabled, then what we really want is a plane that passes through the cursor and is oriented along
					// the direction of movement (aka, the normal is perpendicular to this movement).
					worldPos = s_cursor3d;
				}
				moveFeatures(worldPos);
			}
		}
		else if (s_transformMode == TRANS_ROTATE)
		{
			f32 r0, r1, r2, r3;
			Vec3f* center = (s_view == EDIT_VIEW_2D) ? nullptr : &s_center;
			r0 = gizmo_getRotationRadiusWS(RGP_CIRCLE_OUTER, center);
			r1 = gizmo_getRotationRadiusWS(RGP_CIRCLE_CENTER, center);
			r2 = gizmo_getRotationRadiusWS(RGP_CIRCLE_INNER, center);
			r3 = gizmo_getRotationRadiusWS(RGP_CENTER, center);
			// Squared tests.
			r0 *= r0;
			r1 *= r1;
			r2 *= r2;
			r3 *= r3;
			// Hover.
			Vec2f offset = { s_cursor3d.x - s_center.x, s_cursor3d.z - s_center.z };
			f32 rSq = offset.x*offset.x + offset.z*offset.z;
								
			f32 scale = rSq > FLT_EPSILON ? 1.0f / sqrtf(rSq) : 1.0f;
			Vec2f dir = { offset.x * scale, offset.z * scale };

			// Account of the height difference in 3D.
			if (s_view == EDIT_VIEW_3D)
			{
				f32 dy = s_cursor3d.y - s_center.y;
				rSq += dy * dy;
			}

			// Result is in the -PI to +PI range, need to convert to 0, 2PI
			f32 angleInDeg = atan2f(-offset.z, offset.x) * 180.0f / PI;
			angleInDeg = fmodf(angleInDeg + 360.0f, 360.0f);

			if (s_rotAction == RACTION_NONE)
			{
				s_rotHover = RGP_NONE;
				if (rSq > r1 && rSq < r0)
				{
					s_rotHover = RGP_CIRCLE_OUTER;
				}
				else if (rSq > r2 && rSq <= r1)
				{
					s_rotHover = RGP_CIRCLE_INNER;
				}
				else if (rSq <= r3)
				{
					s_rotHover = RGP_CENTER;
				}

				s_dataCaptured = false;
				if (s_singleClick)
				{
					s_rotation = { 0 };
					s_rotationSide = 0.0f;
					if (s_rotHover == RGP_CIRCLE_OUTER)
					{
						s_rotAction = RACTION_ROTATE;
						const f32 s = angleInDeg < 0.0f ? -1.0f : 1.0f;
						s_rotation.x = s * floorf(fabsf(angleInDeg) / 0.1f + 0.5f) * 0.1f;
						s_rotation.y = angleInDeg;
						s_rotationStartDir = dir;
					}
					else if (s_rotHover == RGP_CIRCLE_INNER)
					{
						s_rotAction = RACTION_ROTATE_SNAP;
						const f32 s = angleInDeg < 0.0f ? -1.0f : 1.0f;
						s_rotation.x = s * floorf(fabsf(angleInDeg)/5.0f + 0.5f) * 5.0f;
						s_rotation.y = s_rotation.x;
						s_rotationStartDir = dir;
					}
					else if (s_rotHover == RGP_CENTER)
					{
						s_rotAction = RACTION_MOVE;
					}
				}
			}
			else if (s_rotAction != RACTION_NONE && s_leftMouseDown)
			{
				f32 side = dir.x*s_rotationStartDir.z - dir.z*s_rotationStartDir.x;
				side = (side < 0.0f) ? -1.0f : 1.0f;

				if (s_rotAction == RACTION_MOVE)
				{
					s_center.x = s_cursor3d.x;
					s_center.z = s_cursor3d.z;
					snapToGrid(&s_center);
				}
				else if (s_rotAction == RACTION_ROTATE || s_rotAction == RACTION_ROTATE_SNAP)
				{
					f32 curAngle;
					const f32 s = angleInDeg < 0.0f ? -1.0f : 1.0f;
					if (s_rotAction == RACTION_ROTATE)
					{
						curAngle = s * floorf(fabsf(angleInDeg) / 0.1f + 0.5f) * 0.1f;
					}
					else // RACTION_ROTATE_SNAP
					{
						curAngle = s * floorf(fabsf(angleInDeg) / 5.0f + 0.5f) * 5.0f;
					}
					const f32 diff = getAngleDifference(s_rotation.x, curAngle);

					// Set the direction of rotation; 0 = none, positive = clockwise (+1), negative = counter-clockwise (-1).
					if (diff == 0.0f)
					{
						s_rotation.z = 0.0f;
					}
					else if (fabsf(diff) < 45.0f && (s_rotation.z == 0.0f || s_rotationSide != side))
					{
						s_rotation.z = diff > 0.0f ? 1.0f : -1.0f;
					}
					s_rotationSide = side;
					s_rotation.y = curAngle;

					const f32 rotAngle = edit_getRotationGizmoAngle();
					if (rotAngle != 0.0f)
					{
						applyRotationToData(rotAngle);
					}
				}
			}
			else
			{
				edit_endTransform();
				s_rotAction = RACTION_NONE;
				s_rotation = { 0 };
			}
		}
	}

	void moveFeatures(const Vec3f& newPos)
	{
		const bool hasSelection = selection_getCount() > 0;
		if (s_editMode == LEDIT_VERTEX)
		{
			if (hasSelection && TFE_Input::mouseDown(MBUTTON_LEFT) && !TFE_Input::keyModDown(KEYMOD_CTRL) && !TFE_Input::keyModDown(KEYMOD_SHIFT))
			{
				if (s_enableMoveTransform)
				{
					edit_moveVertices({ newPos.x, newPos.z });
				}
			}
			else
			{
				edit_endTransform();
			}
		}
		else if (s_editMode == LEDIT_WALL)
		{
			EditorSector* sector = nullptr;
			s32 featureIndex = -1;
			HitPart part = HP_NONE;
			selection_getSurface(0, sector, featureIndex, &part);

			if (hasSelection && TFE_Input::mouseDown(MBUTTON_LEFT) && !TFE_Input::keyModDown(KEYMOD_CTRL) && !TFE_Input::keyModDown(KEYMOD_SHIFT))
			{
				if (s_enableMoveTransform)
				{
					if (part == HP_FLOOR || part == HP_CEIL) { moveFlat(); }
					else { moveWall(newPos); }
				}
			}
			else
			{
				edit_endTransform();
			}
		}
		else if (s_editMode == LEDIT_SECTOR)
		{
			if (hasSelection && TFE_Input::mouseDown(MBUTTON_LEFT) && !TFE_Input::keyModDown(KEYMOD_CTRL) && !TFE_Input::keyModDown(KEYMOD_SHIFT))
			{
				if (s_enableMoveTransform)
				{
					moveSector(newPos);
				}
			}
			else
			{
				edit_endTransform();
			}
		}
	}

	void moveFlat()
	{
		EditorSector* sector = nullptr;
		s32 wallIndex = -1;
		HitPart part = HP_NONE;
		selection_getSurface(0, sector, wallIndex, &part);
		if (!sector) { return; }

		if (!s_moveStarted)
		{
			s_moveStarted = true;
			s_moveStartPos.x = part == HP_FLOOR ? sector->floorHeight : sector->ceilHeight;
			s_moveStartPos.z = 0.0f;
			s_prevPos = s_curVtxPos;
		}

		Vec3f worldPos = moveAlongRail({ 0.0f, 1.0f, 0.0f });
		f32 y = worldPos.y;

		snapToGridY(&y);
		f32 heightDelta;
		if (part == HP_FLOOR)
		{
			heightDelta = y - sector->floorHeight;
		}
		else
		{
			heightDelta = y - sector->ceilHeight;
		}

		edit_moveSelectedFlats(heightDelta);
	}

	void moveWall(Vec3f worldPos)
	{
		EditorSector* sector = nullptr;
		s32 wallIndex = -1;
		HitPart part = HP_NONE;
		selection_getSurface(0, sector, wallIndex, &part);
		if (!sector) { return; }

		const EditorWall* wall = &sector->walls[wallIndex];
		const Vec2f& v0 = sector->vtx[wall->idx[0]];
		const Vec2f& v1 = sector->vtx[wall->idx[1]];
		if (!s_moveStarted)
		{
			s_moveStarted = true;
			s_moveStartPos = v0;
			s_moveStartPos1 = v1;

			s_wallNrm = { -(v1.z - v0.z), v1.x - v0.x };
			s_wallNrm = TFE_Math::normalize(&s_wallNrm);

			s_wallTan = { v1.x - v0.x, v1.z - v0.z };
			s_wallTan = TFE_Math::normalize(&s_wallTan);

			if (s_view == EDIT_VIEW_3D)
			{
				s_prevPos = s_curVtxPos;
			}
		}

		Vec2f moveDir = s_wallNrm;
		Vec2f startPos = s_moveStartPos;
		if (s_view == EDIT_VIEW_3D)
		{
			if (s_wallMoveMode == WMM_NORMAL)
			{
				worldPos = moveAlongRail({ s_wallNrm.x, 0.0f, s_wallNrm.z });
			}
			else
			{
				worldPos = moveAlongXZPlane(s_curVtxPos.y);
			}
		}

		if (s_wallMoveMode == WMM_FREE)
		{
			Vec2f delta = { worldPos.x - s_curVtxPos.x, worldPos.z - s_curVtxPos.z };

			if (!TFE_Input::keyModDown(KEYMOD_ALT))
			{
				// Smallest snap distance.
				Vec2f p0 = { s_moveStartPos.x + delta.x, s_moveStartPos.z + delta.z };
				Vec2f p1 = { s_moveStartPos1.x + delta.x, s_moveStartPos1.z + delta.z };

				Vec2f s0 = p0, s1 = p1;
				snapToGrid(&s0);
				snapToGrid(&s1);

				Vec2f d0 = { p0.x - s0.x, p0.z - s0.z };
				Vec2f d1 = { p1.x - s1.x, p1.z - s1.z };
				if (TFE_Math::dot(&d0, &d0) <= TFE_Math::dot(&d1, &d1))
				{
					delta = { s0.x - v0.x, s0.z - v0.z };
				}
				else
				{
					delta = { s1.x - v1.x, s1.z - v1.z };
				}
			}
			else
			{
				delta = { s_moveStartPos.x + delta.x - v0.x, s_moveStartPos.z + delta.z - v0.z };
			}

			// Move all of the vertices by the offset.
			edit_moveSelectedVertices(delta);
		}
		else
		{
			// Compute the movement.
			Vec2f offset = { worldPos.x - startPos.x, worldPos.z - startPos.z };
			f32 nrmOffset = TFE_Math::dot(&offset, &moveDir);

			if (!TFE_Input::keyModDown(KEYMOD_ALT))
			{
				// Find the snap point along the path from vertex 0 and vertex 1.
				f32 v0Offset = snapAlongPath(s_moveStartPos, moveDir, s_moveStartPos, nrmOffset);
				f32 v1Offset = snapAlongPath(s_moveStartPos1, moveDir, s_moveStartPos, nrmOffset);

				// Finally determine which snap to take (whichever is the smallest).
				if (v0Offset < FLT_MAX && fabsf(v0Offset - nrmOffset) < fabsf(v1Offset - nrmOffset))
				{
					nrmOffset = v0Offset;
				}
				else if (v1Offset < FLT_MAX)
				{
					nrmOffset = v1Offset;
				}
			}

			// Move all of the vertices by the offset.
			Vec2f delta = { s_moveStartPos.x + moveDir.x * nrmOffset - v0.x, s_moveStartPos.z + moveDir.z * nrmOffset - v0.z };
			edit_moveSelectedVertices(delta);
		}
	}

	void moveSector(Vec3f worldPos)
	{
		EditorSector* sector = nullptr;
		selection_getSector(0, sector);
		if (!sector) { return; }

		if (!s_moveStarted)
		{
			s_moveStarted = true;
			s_moveStartPos = sector->vtx[0];
			if (s_view == EDIT_VIEW_3D)
			{
				s_prevPos = s_curVtxPos;
			}
		}

		worldPos = moveAlongXZPlane(s_curVtxPos.y);
		Vec2f delta = { worldPos.x - s_curVtxPos.x, worldPos.z - s_curVtxPos.z };

		if (!TFE_Input::keyModDown(KEYMOD_ALT))
		{
			// Smallest snap distance.
			Vec2f p0 = { s_moveStartPos.x + delta.x, s_moveStartPos.z + delta.z };

			Vec2f s0 = p0;
			snapToGrid(&s0);
			delta = { s0.x - sector->vtx[0].x, s0.z - sector->vtx[0].z };
		}
		else
		{
			delta = { s_moveStartPos.x + delta.x - sector->vtx[0].x, s_moveStartPos.z + delta.z - sector->vtx[0].z };
		}

		// Move all of the vertices by the offset.
		edit_moveSelectedVertices(delta);
	}
		
	void captureRotationData()
	{
		s_dataCaptured = true;
		if (s_transformMode == TRANS_ROTATE)
		{
			EditorSector* sector = nullptr;
			s32 index = -1;

			const u32 vertexCount = selection_getCount(SEL_VERTEX);
			s_vertexData.resize(vertexCount);
			Vec2f* vtxData = s_vertexData.data();
			for (u32 v = 0; v < vertexCount; v++)
			{
				selection_getVertex(v, sector, index);
				vtxData[v] = sector->vtx[index];
			}
		}
	}

	void applyRotationToData(f32 angle)
	{
		const u32 vertexCount = selection_getCount(SEL_VERTEX);
		if (!vertexCount) { return; }
		if (!s_dataCaptured)
		{
			captureRotationData();
		}

		const f32 ca = cosf(-angle), sa = sinf(-angle);
		const Vec3f mtx[] =
		{
			{ ca, -sa, s_center.x },
			{ sa,  ca, s_center.z }
		};

		s_searchKey++;
		s_sectorChangeList.clear();

		EditorSector* sector;
		s32 index;
		const Vec2f* srcVtx = s_vertexData.data();
		for (u32 v = 0; v < vertexCount; v++)
		{
			selection_getVertex(v, sector, index);
			sector->vtx[index].x = (srcVtx[v].x - s_center.x) * mtx[0].x + (srcVtx[v].z - s_center.z) * mtx[0].y + mtx[0].z;
			sector->vtx[index].z = (srcVtx[v].x - s_center.x) * mtx[1].x + (srcVtx[v].z - s_center.z) * mtx[1].y + mtx[1].z;

			if (sector->searchKey != s_searchKey)
			{
				s_sectorChangeList.push_back(sector);
				sector->searchKey = s_searchKey;
			}
		}

		// Update sectors after changes.
		const size_t changeCount = s_sectorChangeList.size();
		EditorSector** list = s_sectorChangeList.data();
		for (size_t i = 0; i < changeCount; i++)
		{
			EditorSector* sector = list[i];
			sectorToPolygon(sector);
		}
	}
}

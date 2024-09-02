#include "editTransforms.h"
#include "editCommon.h"
#include "levelEditor.h"
#include "editVertex.h"
#include "sharedState.h"
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_System/math.h>
#include <TFE_Input/input.h>

using namespace TFE_Editor;

namespace LevelEditor
{
	static TransformMode s_transformMode = TRANS_MOVE;
	static bool s_enableMoveTransform = false;
	WallMoveMode s_wallMoveMode = WMM_NORMAL;

	void moveFeatures(const Vec3f& newPos);
	void moveFlat();
	void moveWall(Vec3f worldPos);

	void edit_setTransformMode(TransformMode mode)
	{
		s_transformMode = mode;
	}

	TransformMode edit_getTransformMode()
	{
		return s_transformMode;
	}

	void edit_enableMoveTransform(bool enable)
	{
		s_enableMoveTransform = enable;
	}

	void edit_transform(s32 mx, s32 my)
	{
		if (!s_editMove) { return; }

		if (s_transformMode == TRANS_MOVE)
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
				else if (s_editMode == LEDIT_WALL)
				{
					// When wall editing is enabled, then what we really want is a plane that passes through the cursor and is oriented along
					// the direction of movement (aka, the normal is perpendicular to this movement).
					worldPos = s_cursor3d;
				}
				moveFeatures(worldPos);
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
				edit_endMove();
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
				edit_endMove();
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

		// Overall movement since mousedown, for the history.
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
}

#include "editTransforms.h"
#include "editCommon.h"
#include "editEntity.h"
#include "editSector.h"
#include "hotkeys.h"
#include "levelEditor.h"
#include "levelEditorHistory.h"
#include "editVertex.h"
#include "sharedState.h"
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
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
	static EditorSector* s_moveSector = nullptr;
	static HitPart s_movePart;
	static Vec3f s_transformPos = { 0 };
	static Vec3f s_center = { 0 };
	static Vec3f s_rotation = { 0 };
	static Vec2f s_rotationStartDir = { 0 };
	static std::vector<Vec2f> s_vertexData;
	static std::vector<Vec3f> s_objData;
	static std::vector<s32> s_effectedSectors;
	static f32 s_rotationSide = 0.0f;
	static RotationAction s_rotAction = RACTION_NONE;
	static u32 s_moveAxis = AXIS_XYZ;
	static bool s_enableMoveTransform = false;
	static bool s_transformChange = false;
	static bool s_transformToolActive = false;
	static bool s_dataCaptured = false;

	void moveFeatures(const Vec3f& newPos);
	void moveFlat();
	void moveWall(Vec3f worldPos);
	void moveSector(Vec3f worldPos);
	void moveEntity(Vec3f worldPos);
	void captureRotationData();
	void applyRotationToData(f32 angle);
	void addObjectToNewSector(EditorObject* obj, EditorSector* sector, s32 featureIndex, EditorSector* newSector);
	void endTransformInternal();

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

	// Call when the selection changes so that the transform can be reset.
	void edit_setTransformChange()
	{
		s_transformChange = true;
	}
		
	bool edit_isTransformToolActive()
	{
		return s_transformToolActive || (s_transformMode == TRANS_MOVE && s_moveStarted);
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
		if (edit_isTransformToolActive())
		{
			// Rotation
			if (s_transformMode == TRANS_ROTATE && s_rotHover != RGP_NONE) { interacting = true; }
			else if (s_transformMode == TRANS_MOVE && s_moveStarted) { interacting = true; }
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
			else if (s_editMode == LEDIT_ENTITY)
			{
				const u32 objCount = selection_getCount();
				if (objCount)
				{
					s_transformToolActive = true;

					// Get bounds based on vertex set.
					s32 objIndex = -1;
					EditorSector* sector = nullptr;
					Vec3f boundsMin = { FLT_MAX, FLT_MAX, FLT_MAX };
					Vec3f boundsMax = { -FLT_MAX,-FLT_MAX,-FLT_MAX };

					for (u32 v = 0; v < objCount; v++)
					{
						selection_getEntity(v, sector, objIndex);
						const EditorObject* obj = &sector->obj[objIndex];
						boundsMin.x = std::min(boundsMin.x, obj->pos.x);
						boundsMin.z = std::min(boundsMin.z, obj->pos.z);
						boundsMax.x = std::max(boundsMax.x, obj->pos.x);
						boundsMax.z = std::max(boundsMax.z, obj->pos.z);

						const Entity* entity = &s_level.entities[obj->entityId];
						const f32 base = (entity->type == ETYPE_3D) ? obj->pos.y + entity->obj3d->bounds[0].y : obj->pos.y;
						boundsMin.y = std::min(boundsMin.y, obj->pos.y);
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

	bool rayPlaneIntersection(const Vec3f& origin, const Vec3f& dir, const Vec4f& plane, f32& dist)
	{
		const f32 d = plane.x*dir.x + plane.y*dir.y + plane.z*dir.z;
		if (fabsf(d) < FLT_MIN) { return false; }

		dist = -(plane.x*origin.x + plane.y*origin.y + plane.z*origin.z + plane.w) / d;
		return dist >= FLT_MIN;
	}

	bool rayBoundingPlaneIntersection(const Vec4f* planes, Vec3f* it)
	{
		f32 closestHit = FLT_MAX;
		for (s32 i = 0; i < 6; i++)
		{
			// Skip backfacing planes.
			if (planes[i].x*s_rayDir.x + planes[i].y*s_rayDir.y + planes[i].z*s_rayDir.z > 0.0f)
			{
				continue;
			}

			f32 dist;
			if (rayPlaneIntersection(s_camera.pos, s_rayDir, planes[i], dist))
			{
				if (dist < closestHit)
				{
					closestHit = dist;
				}
			}
		}
		if (closestHit <= 0.0f || closestHit == FLT_MAX) { return false; }

		it->x = s_camera.pos.x + s_rayDir.x * closestHit;
		it->y = s_camera.pos.y + s_rayDir.y * closestHit;
		it->z = s_camera.pos.z + s_rayDir.z * closestHit;
		return true;
	}

	Vec3f edit_gizmoCursor3d()
	{
		f32 height = s_grid.height;
		s_grid.height = s_center.y;
		Vec3f cursor = rayGridPlaneHit(s_camera.pos, s_rayDir);
		s_grid.height = height;

		return cursor;
	}
		
	Vec3f edit_getTransformAnchor()
	{
		return s_center;
	}
	void edit_setTransformAnchor(Vec3f anchor)
	{
		s_center = anchor;
		s_transformPos = anchor;
	}
		
	Vec3f edit_getTransformPos()
	{
		return s_transformPos;
	}
	void edit_setTransformPos(Vec3f pos)
	{
		s_transformPos = pos;
	}

	u32 edit_getMoveAxis()
	{
		return s_moveAxis;
	}

	void edit_transform(s32 mx, s32 my)
	{
		edit_applyTransformChange();

		if (s_transformMode == TRANS_MOVE && s_editMove)
		{
			const u32 moveAxis = s_moveAxis;
			const WallMoveMode moveMode = s_wallMoveMode;
			// Get the new move axis.
			s_moveAxis = AXIS_XZ;
			s_wallMoveMode = WMM_FREE;
			if (isShortcutHeld(SHORTCUT_MOVE_X))
			{
				s_moveAxis = AXIS_X;
			}
			// Y-axis movement only works in 3D view.
			else if (isShortcutHeld(SHORTCUT_MOVE_Y) && s_view != EDIT_VIEW_2D)
			{
				s_moveAxis = AXIS_Y;
			}
			else if (isShortcutHeld(SHORTCUT_MOVE_Z))
			{
				s_moveAxis = AXIS_Z;
			}
			// Normal 
			if (isShortcutHeld(SHORTCUT_MOVE_NORMAL))
			{
				s_wallMoveMode = WMM_NORMAL;
			}
			// If the moveAxis has changed, we need to reset the movement and anchor at the current position.
			if ((moveAxis != s_moveAxis || moveMode != s_wallMoveMode) && s_moveStarted)
			{
				// Reset the move.
				s_moveStarted = false;
				// Reset the center height if moved in Y and recompute the cursor if needed.
				if (moveAxis & AXIS_Y)
				{
					// Reset the center to the new height.
					s_center.y = s_transformPos.y;
					// Recalculate the cursor, since the plane height has changed.
					s_cursor3d = edit_gizmoCursor3d();
				}
			}

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
				else
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

			// Handle cursor.
			Vec3f cursor = s_cursor3d;
			if (s_view == EDIT_VIEW_3D)
			{
				cursor = edit_gizmoCursor3d();
			}

			// Hover.
			Vec2f offset = { cursor.x - s_center.x, cursor.z - s_center.z };
			f32 rSq = offset.x*offset.x + offset.z*offset.z;

			f32 scale = rSq > FLT_EPSILON ? 1.0f / sqrtf(rSq) : 1.0f;
			Vec2f dir = { offset.x * scale, offset.z * scale };

			// Account of the height difference in 3D.
			if (s_view == EDIT_VIEW_3D)
			{
				f32 dy = cursor.y - s_center.y;
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
					s_center.x = cursor.x;
					s_center.z = cursor.z;
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
					applyRotationToData(rotAngle);
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
		
	void edit_endTransform()
	{
		const u32 count = selection_getCount();
		if (s_editMove && s_moveStarted && count > 0)
		{
			s_searchKey++;
			s_idList.clear();
			u32 name = 0;
			switch (s_editMode)
			{
				case LEDIT_VERTEX:
				{
					EditorSector* sector = nullptr;
					s32 featureIndex = -1;
					for (u32 i = 0; i < count; i++)
					{
						selection_getVertex(i, sector, featureIndex);
						if (sector->searchKey != s_searchKey)
						{
							sector->searchKey = s_searchKey;
							s_idList.push_back(sector->id);
						}
					}
					name = LName_MoveVertex;
				} break;
				case LEDIT_WALL:
				{
					EditorSector* sector = nullptr;
					s32 featureIndex = -1;
					HitPart part;
					for (u32 i = 0; i < count; i++)
					{
						selection_getSurface(i, sector, featureIndex, &part);
						if (sector->searchKey != s_searchKey)
						{
							sector->searchKey = s_searchKey;
							s_idList.push_back(sector->id);
						}
					}

					// Handle vertices connected to other sectors.
					u32 vertexCount = (u32)selection_getCount(SEL_VERTEX);
					for (u32 v = 0; v < vertexCount; v++)
					{
						selection_getVertex(v, sector, featureIndex);
						if (sector->searchKey != s_searchKey)
						{
							sector->searchKey = s_searchKey;
							s_idList.push_back(sector->id);
						}
					}
					name = LName_MoveWall;
				} break;
				case LEDIT_SECTOR:
				{
					EditorSector* sector = nullptr;
					for (u32 i = 0; i < count; i++)
					{
						selection_getSector(i, sector);
						if (sector->searchKey != s_searchKey)
						{
							sector->searchKey = s_searchKey;
							s_idList.push_back(sector->id);
						}
					}

					// Handle vertices connected to other sectors.
					u32 vertexCount = (u32)selection_getCount(SEL_VERTEX);
					s32 featureIndex = -1;
					for (u32 v = 0; v < vertexCount; v++)
					{
						selection_getVertex(v, sector, featureIndex);
						if (sector->searchKey != s_searchKey)
						{
							sector->searchKey = s_searchKey;
							s_idList.push_back(sector->id);
						}
					}
					name = LName_MoveSector;
				} break;
				case LEDIT_ENTITY:
				{
					EditorSector* sector = nullptr;
					s32 featureIndex = -1;

					u32 entityCount = (u32)selection_getCount(SEL_ENTITY);
					for (u32 v = 0; v < entityCount; v++)
					{
						selection_getEntity(v, sector, featureIndex);
						if (sector->searchKey != s_searchKey)
						{
							sector->searchKey = s_searchKey;
							s_idList.push_back(sector->id);
						}

						// Now check to see if the entity is still in the same sector.
						EditorObject* obj = &sector->obj[featureIndex];
						if (!isPointInsideSector3d(sector, obj->pos))
						{
							// Try to find a different sector.
							EditorSector* newSector = findSector3d(obj->pos);
							if (newSector)
							{
								addObjectToNewSector(obj, sector, featureIndex, newSector);
							}
							else if (!isPointInsideSector2d(sector, { obj->pos.x, obj->pos.z }))
							{
								// No longer in a sector, so try to find a 2D sector (maybe underneath).
								newSector = findSector2d_closestHeight({ obj->pos.x, obj->pos.z }, obj->pos.y);
								if (newSector)
								{
									addObjectToNewSector(obj, sector, featureIndex, newSector);
								}

								// Otherwise just leave it alone for now.
							}
						}
					}
					name = LName_MoveObject;
				} break;
			};
			// Take a snapshot of changed sectors.
			// TODO: Vertex snapshot of a set of sectors?
			cmd_sectorSnapshot(name, s_idList);
		}
		else
		{
			endTransformInternal();
		}

		s_editMove = false;
		s_moveStarted = false;
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
			selection_getSurface(selection_hasHovered() ? SEL_INDEX_HOVERED : 0, sector, featureIndex, &part);

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
		else if (s_editMode == LEDIT_ENTITY)
		{
			if (hasSelection && TFE_Input::mouseDown(MBUTTON_LEFT) && !TFE_Input::keyModDown(KEYMOD_CTRL) && !TFE_Input::keyModDown(KEYMOD_SHIFT))
			{
				if (s_enableMoveTransform)
				{
					moveEntity(newPos);
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
		EditorSector* sector = s_moveSector;
		if (!s_moveStarted)
		{
			s32 wallIndex = -1;
			s_movePart = HP_NONE;
			sector = nullptr;
			selection_getSurface(selection_hasHovered() ? SEL_INDEX_HOVERED : 0, sector, wallIndex, &s_movePart);
			if (!sector) { return; }

			s_moveStarted = true;
			s_moveStartPos.x = s_movePart == HP_FLOOR ? sector->floorHeight : sector->ceilHeight;
			s_moveStartPos.z = 0.0f;
			s_prevPos = s_curVtxPos;
			s_moveSector = sector;

			edit_setTransformAnchor({ s_curVtxPos.x, s_moveStartPos.x, s_curVtxPos.z });
		}

		Vec3f worldPos = moveAlongRail({ 0.0f, 1.0f, 0.0f }, false);
		f32 y = worldPos.y;

		const Vec3f cameraDelta = { worldPos.x - s_camera.pos.x, worldPos.y - s_camera.pos.y, worldPos.z - s_camera.pos.z };
		if (cameraDelta.x*s_rayDir.x + cameraDelta.z*s_rayDir.z < 0.0f)
		{
			// Do not allow the object to be moved behind the camera.
			return;
		}

		snapToGridY(&y);
		f32 heightDelta;
		if (s_movePart == HP_FLOOR)
		{
			heightDelta = y - sector->floorHeight;
		}
		else
		{
			heightDelta = y - sector->ceilHeight;
		}

		Vec3f pos = edit_getTransformPos();
		edit_setTransformPos({ pos.x, pos.y + heightDelta, pos.z });
		edit_moveSelectedFlats(heightDelta);
	}

	void moveWall(Vec3f worldPos)
	{
		if (!s_moveStarted)
		{
			// Prefer hovered sector, but we must keep the sector and wall index since hovered may change later.
			s_transformWallSector = nullptr;
			s_transformWallIndex = -1;
			selection_getSurface(selection_hasHovered() ? SEL_INDEX_HOVERED : 0, s_transformWallSector, s_transformWallIndex);
			if (!s_transformWallSector) { return; }

			const EditorSector* sector = s_transformWallSector;
			const EditorWall* wall = &sector->walls[s_transformWallIndex];
			const Vec2f& v0 = sector->vtx[wall->idx[0]];
			const Vec2f& v1 = sector->vtx[wall->idx[1]];

			s_moveStarted   = true;
			s_moveStartPos  = v0;
			s_moveStartPos1 = v1;

			s_wallNrm = { -(v1.z - v0.z), v1.x - v0.x };
			s_wallNrm = TFE_Math::normalize(&s_wallNrm);

			s_wallTan = { v1.x - v0.x, v1.z - v0.z };
			s_wallTan = TFE_Math::normalize(&s_wallTan);

			if (s_view == EDIT_VIEW_3D)
			{ 
				s_prevPos = s_curVtxPos;
			}
			s_curVtxPos = { worldPos.x, s_cursor3d.y, worldPos.z };

			edit_setTransformAnchor({ worldPos.x, s_cursor3d.y, worldPos.z });
		}
				
		const EditorSector* sector = s_transformWallSector;
		const EditorWall* wall = &sector->walls[s_transformWallIndex];
		const Vec2f& v0 = sector->vtx[wall->idx[0]];
		const Vec2f& v1 = sector->vtx[wall->idx[1]];

		Vec2f moveDir = s_wallNrm;
		Vec2f startPos = s_moveStartPos;
		Vec3f pos = edit_getTransformPos();
		const u32 moveAxis = edit_getMoveAxis();
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

			const Vec3f cameraDelta = { worldPos.x - s_camera.pos.x, worldPos.y - s_camera.pos.y, worldPos.z - s_camera.pos.z };
			if (cameraDelta.x*s_rayDir.x + cameraDelta.z*s_rayDir.z < 0.0f)
			{
				// Do not allow the object to be moved behind the camera.
				return;
			}
		}

		Vec3f transformPos = edit_getTransformPos();
		if (s_wallMoveMode == WMM_FREE)
		{
			Vec2f delta = { worldPos.x - s_curVtxPos.x, worldPos.z - s_curVtxPos.z };
			if (!(moveAxis & AXIS_X))
			{
				delta.x = 0.0f;
			}
			if (!(moveAxis & AXIS_Z))
			{
				delta.z = 0.0f;
			}
					
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
			edit_setTransformPos({ transformPos.x + delta.x, transformPos.y, transformPos.z + delta.z });
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
			edit_setTransformPos({ transformPos.x + delta.x, transformPos.y, transformPos.z + delta.z });
			edit_moveSelectedVertices(delta);
		}
	}

	void moveSector(Vec3f worldPos)
	{
		const u32 moveAxis = edit_getMoveAxis();
		const bool moveOnYAxis = (moveAxis & AXIS_Y) != 0u;

		snapToGrid(&worldPos);
		if (!s_moveStarted)
		{
			s_moveStarted = true;
			s_moveStartPos = { worldPos.x, worldPos.z };

			s_moveStarted = true;
			if (s_view == EDIT_VIEW_3D)
			{
				s_prevPos = s_curVtxPos;
			}

			EditorSector* sector = nullptr;
			selection_getSector(selection_hasHovered() ? SEL_INDEX_HOVERED : 0, sector);

			if (moveOnYAxis && sector)
			{
				const f32 dF = fabsf(worldPos.y - sector->floorHeight);
				const f32 dC = fabsf(worldPos.y - sector->ceilHeight);
				edit_setTransformAnchor({ s_moveStartPos.x, dF <= dC ? sector->floorHeight : sector->ceilHeight, s_moveStartPos.z });
			}
			else
			{
				if (sector) { s_grid.height = sector->floorHeight; worldPos.y = s_grid.height; }
				edit_setTransformAnchor({ s_moveStartPos.x, worldPos.y, s_moveStartPos.z });
			}

			if (moveOnYAxis)
			{
				s_curVtxPos = s_cursor3d;
			}
		}

		if (moveOnYAxis)
		{
			worldPos = moveAlongRail({ 0.0f, 1.0f, 0.0f });
			snapToGridY(&worldPos.y);
		}

		if (s_view == EDIT_VIEW_3D)
		{
			const Vec3f cameraDelta = { worldPos.x - s_camera.pos.x, worldPos.y - s_camera.pos.y, worldPos.z - s_camera.pos.z };
			if (cameraDelta.x*s_rayDir.x + cameraDelta.z*s_rayDir.z < 0.0f || (s_cursor3d.x == 0.0f && s_cursor3d.z == 0.0f))
			{
				// Do not allow the object to be moved behind the camera.
				return;
			}
		}

		// Current movement.
		const Vec3f pos = edit_getTransformPos();
		Vec3f delta = { worldPos.x - s_moveStartPos.x, worldPos.y - pos.y, worldPos.z - s_moveStartPos.z };
		Vec3f transPos = worldPos;
		if (moveOnYAxis)
		{
			// Movement along Y axis.
			delta.x = 0.0f;
			delta.z = 0.0f;
			transPos.x = pos.x;
			transPos.z = pos.z;
		}
		else
		{
			// Movement along XZ axis.
			delta.y = 0.0f;
			transPos.y = pos.y;

			if (!(moveAxis & AXIS_X))
			{
				delta.x = 0.0f;
				delta.y = 0.0f;
				transPos.x = pos.x;
				transPos.y = pos.y;
			}
			else if (!(moveAxis & AXIS_Z))
			{
				delta.z = 0.0f;
				delta.y = 0.0f;
				transPos.z = pos.z;
				transPos.y = pos.y;
			}
		}

		s_moveStartPos = { transPos.x, transPos.z };
		edit_setTransformPos(transPos);
		edit_moveSector(delta);
	}

	void moveEntity(Vec3f worldPos)
	{
		const u32 moveAxis = edit_getMoveAxis();
		const bool moveOnYAxis = (moveAxis & AXIS_Y) != 0u;

		snapToGrid(&worldPos);
		if (!s_moveStarted)
		{
			s_moveStarted = true;
			
			EditorSector* sector = nullptr;
			EditorObject* obj = nullptr;
			Entity* entity = nullptr;
			s32 index = -1;
			selection_getEntity(selection_hasHovered() ? SEL_INDEX_HOVERED : 0, sector, index);
			if (sector && index >= 0)
			{
				obj = &sector->obj[index];
				entity = &s_level.entities[obj->entityId];
			}

			f32 base, top;
			if (entity->type == ETYPE_3D)
			{
				base = obj->pos.y + entity->obj3d->bounds[0].y;
				top = obj->pos.y + entity->obj3d->bounds[1].y;
			}
			else
			{
				base = obj->pos.y;
				top = obj->pos.y + entity->size.z;
			}

			if (s_view == EDIT_VIEW_3D)
			{
				Vec4f planes[6];
				Vec3f it;
				viewport_computeEntityBoundingPlanes(sector, obj, planes);
				if (rayBoundingPlaneIntersection(planes, &it))
				{
					s_cursor3d = it;
				}
				else
				{
					s_cursor3d = edit_gizmoCursor3d();
				}
				
				// Snap to the grid (XZ).
				Vec2f snapPos = { s_cursor3d.x, s_cursor3d.z };
				snapToGrid(&snapPos);
				s_cursor3d = { snapPos.x, s_cursor3d.y, snapPos.z };

				// Set the world position.
				worldPos = s_cursor3d;
				s_curVtxPos = s_cursor3d;
				s_prevPos = s_cursor3d;
			}
			s_moveStartPos = { worldPos.x, worldPos.z };

			if (moveOnYAxis && obj)
			{
				edit_setTransformAnchor(worldPos);
			}
			else
			{
				edit_setTransformAnchor({ s_moveStartPos.x, worldPos.y, s_moveStartPos.z });
			}

			if (moveOnYAxis)
			{
				s_curVtxPos = s_cursor3d;
			}
		}

		if (moveOnYAxis)
		{
			worldPos = moveAlongRail({ 0.0f, 1.0f, 0.0f }, false);
			snapToGridY(&worldPos.y);
		}

		// Current movement.
		const Vec3f pos = edit_getTransformPos();
		Vec3f delta = { worldPos.x - s_moveStartPos.x, worldPos.y - pos.y, worldPos.z - s_moveStartPos.z };
		Vec3f transPos = worldPos;

		if (s_view == EDIT_VIEW_3D)
		{
			const Vec3f cameraDelta = { worldPos.x - s_camera.pos.x, worldPos.y - s_camera.pos.y, worldPos.z - s_camera.pos.z };
			if (cameraDelta.x*s_rayDir.x + cameraDelta.z*s_rayDir.z < 0.0f && !moveOnYAxis)
			{
				// Do not allow the object to be moved behind the camera.
				return;
			}
		}

		if (moveOnYAxis)
		{
			// Movement along Y axis.
			delta.x = 0.0f;
			delta.z = 0.0f;
			transPos.x = pos.x;
			transPos.z = pos.z;
		}
		else
		{
			// Movement along XZ axis.
			delta.y = 0.0f;
			transPos.y = pos.y;

			if (!(moveAxis & AXIS_X))
			{
				delta.x = 0.0f;
				delta.y = 0.0f;
				transPos.x = pos.x;
				transPos.y = pos.y;
			}
			else if (!(moveAxis & AXIS_Z))
			{
				delta.z = 0.0f;
				delta.y = 0.0f;
				transPos.z = pos.z;
				transPos.y = pos.y;
			}
		}

		s_moveStartPos = { transPos.x, transPos.z };
		edit_setTransformPos(transPos);
		edit_moveEntity(delta);
	}

	u32 captureGetTotalObjectCount()
	{
		u32 totalObjCount = 0;
		const u32 sectorCount = selection_getCount(SEL_SECTOR);
		EditorSector* sector;
		for (u32 s = 0; s < sectorCount; s++)
		{
			selection_getSector(s, sector);
			totalObjCount += (u32)sector->obj.size();
		}
		return totalObjCount;
	}

	void captureObjectRotationData()
	{
		if (s_editMode != LEDIT_SECTOR) { return; }
		u32 totalObjectCount = captureGetTotalObjectCount();
		if (!totalObjectCount) { return; }

		const u32 sectorCount = selection_getCount(SEL_SECTOR);
		EditorSector* sector;

		s_objData.resize(totalObjectCount);
		Vec3f* objData = s_objData.data();
		for (u32 s = 0; s < sectorCount; s++)
		{
			selection_getSector(s, sector);
			const u32 objCount = (u32)sector->obj.size();
			EditorObject* obj = sector->obj.data();
			for (u32 o = 0; o < objCount; o++, obj++)
			{
				objData[o] = { obj->pos.x, obj->pos.z, obj->angle };
			}
			objData += objCount;
		}
	}

	void captureRotationData()
	{
		s_dataCaptured = true;
		if (s_transformMode == TRANS_ROTATE)
		{
			EditorSector* sector = nullptr;
			s32 index = -1;

			if (s_editMode == LEDIT_ENTITY)
			{
				const u32 objCount = selection_getCount(SEL_ENTITY);
				s_objData.resize(objCount);
				Vec3f* objData = s_objData.data();
				for (u32 s = 0; s < objCount; s++)
				{
					selection_getEntity(s, sector, index);
					EditorObject* obj = &sector->obj[index];
					objData[s] = { obj->pos.x, obj->pos.z, obj->angle };
				}
			}
			else
			{
				const u32 vertexCount = selection_getCount(SEL_VERTEX);
				const u32 totalObjCount = captureGetTotalObjectCount();
				s_vertexData.resize(vertexCount);
				Vec2f* vtxData = s_vertexData.data();
				for (u32 v = 0; v < vertexCount; v++)
				{
					selection_getVertex(v, sector, index);
					vtxData[v] = sector->vtx[index];
				}
				captureObjectRotationData();
			}
		}
	}

	void applyRotationToObjects(const Vec3f* mtx, f32 angle)
	{
		if (s_editMode != LEDIT_SECTOR) { return; }

		const u32 sectorCount = selection_getCount(SEL_SECTOR);
		EditorSector* sector;

		Vec3f* srcData = s_objData.data();
		for (u32 s = 0; s < sectorCount; s++)
		{
			selection_getSector(s, sector);
			const u32 objCount = (u32)sector->obj.size();
			EditorObject* obj = sector->obj.data();
			for (u32 v = 0; v < objCount; v++, obj++)
			{
				obj->pos.x = (srcData[v].x - s_center.x) * mtx[0].x + (srcData[v].y - s_center.z) * mtx[0].y + mtx[0].z;
				obj->pos.z = (srcData[v].x - s_center.x) * mtx[1].x + (srcData[v].y - s_center.z) * mtx[1].y + mtx[1].z;
				obj->angle = srcData[v].z + angle;

				if (s_level.entities[obj->entityId].type == ETYPE_3D)
				{
					compute3x3Rotation(&obj->transform, obj->angle, obj->pitch, obj->roll);
				}
			}
			srcData += objCount;
		}
	}

	void applyRotationToData(f32 angle)
	{
		if ((s_editMode == LEDIT_ENTITY && !selection_getCount()) ||
			(s_editMode != LEDIT_ENTITY && !selection_getCount(SEL_VERTEX)))
		{
			return;
		}

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
		if (s_editMode == LEDIT_ENTITY)
		{
			const u32 objCount = selection_getCount();
			const Vec3f* srcObjData = s_objData.data();
			if (fabsf(angle) < 0.0001f)
			{
				for (u32 o = 0; o < objCount; o++)
				{
					selection_getEntity(o, sector, index);
					EditorObject* obj = &sector->obj[index];
					obj->pos.x = srcObjData[o].x;
					obj->pos.z = srcObjData[o].y;
					obj->angle = srcObjData[o].z;

					if (s_level.entities[obj->entityId].type == ETYPE_3D)
					{
						compute3x3Rotation(&obj->transform, obj->angle, obj->pitch, obj->roll);
					}

					if (sector->searchKey != s_searchKey)
					{
						s_sectorChangeList.push_back(sector);
						sector->searchKey = s_searchKey;
					}
				}
			}
			else
			{
				for (u32 o = 0; o < objCount; o++)
				{
					selection_getEntity(o, sector, index);
					EditorObject* obj = &sector->obj[index];
					obj->pos.x = (srcObjData[o].x - s_center.x) * mtx[0].x + (srcObjData[o].y - s_center.z) * mtx[0].y + mtx[0].z;
					obj->pos.z = (srcObjData[o].x - s_center.x) * mtx[1].x + (srcObjData[o].y - s_center.z) * mtx[1].y + mtx[1].z;
					obj->angle = srcObjData[o].z + angle;

					if (s_level.entities[obj->entityId].type == ETYPE_3D)
					{
						compute3x3Rotation(&obj->transform, obj->angle, obj->pitch, obj->roll);
					}

					if (sector->searchKey != s_searchKey)
					{
						s_sectorChangeList.push_back(sector);
						sector->searchKey = s_searchKey;
					}
				}
			}
		}
		else
		{
			const u32 vertexCount = selection_getCount(SEL_VERTEX);
			const Vec2f* srcVtx = s_vertexData.data();
			if (fabsf(angle) < 0.0001f)
			{
				// If the angle is very close to zero, then just copy the original data.
				for (u32 v = 0; v < vertexCount; v++)
				{
					selection_getVertex(v, sector, index);
					sector->vtx[index] = srcVtx[v];
					if (sector->searchKey != s_searchKey)
					{
						s_sectorChangeList.push_back(sector);
						sector->searchKey = s_searchKey;
					}
				}
			}
			else
			{
				// Otherwise apply the rotation.
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
			}
			applyRotationToObjects(mtx, angle);
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

	void addObjectToNewSector(EditorObject* obj, EditorSector* sector, s32 featureIndex, EditorSector* newSector)
	{
		EditorObject objCopy = *obj;
		edit_deleteObject(sector, featureIndex);
		selection_clearHovered();
		selection_entity(SA_REMOVE, sector, featureIndex);

		s32 newIndex = (s32)newSector->obj.size();
		selection_entity(SA_ADD, newSector, newIndex);
		newSector->obj.push_back(objCopy);

		if (newSector->searchKey != s_searchKey)
		{
			newSector->searchKey = s_searchKey;
			s_idList.push_back(newSector->id);
		}
	}

	void endTransformInternal()
	{
		if (edit_gizmoChangedGeo() && selection_getCount() > 0)
		{
			s_searchKey++;
			s_idList.clear();

			// Handle vertices connected to other sectors.
			if (s_editMode != LEDIT_ENTITY)
			{
				u32 vertexCount = (u32)selection_getCount(SEL_VERTEX);
				EditorSector* sector = nullptr;
				s32 featureIndex = -1;
				for (u32 v = 0; v < vertexCount; v++)
				{
					selection_getVertex(v, sector, featureIndex);
					if (sector->searchKey != s_searchKey)
					{
						sector->searchKey = s_searchKey;
						s_idList.push_back(sector->id);
					}
				}
			}
			else // LEDIT_ENTITY
			{
				EditorSector* sector = nullptr;
				s32 featureIndex = -1;

				u32 entityCount = (u32)selection_getCount(SEL_ENTITY);
				for (u32 v = 0; v < entityCount; v++)
				{
					selection_getEntity(v, sector, featureIndex);
					if (sector->searchKey != s_searchKey)
					{
						sector->searchKey = s_searchKey;
						s_idList.push_back(sector->id);
					}
				}
			}

			u32 name = 0;
			if (s_editMode == LEDIT_SECTOR)
			{
				name = LName_RotateSector;
			}
			else if (s_editMode == LEDIT_WALL)
			{
				name = LName_RotateWall;
			}
			else if (s_editMode == LEDIT_VERTEX)
			{
				name = LName_RotateVertex;
			}
			else if (s_editMode == LEDIT_ENTITY)
			{
				name = LName_RotateEntity;
			}
			cmd_sectorSnapshot(name, s_idList);
			edit_resetGizmo();
		}
	}
}

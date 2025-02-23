#include "editEntity.h"
#include "editCommon.h"
#include "hotkeys.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "levelEditorHistory.h"
#include "sharedState.h"
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_System/math.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editor.h>

#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;
using namespace TFE_Jedi;

namespace LevelEditor
{
	static Vec3f s_moveBasePos3d = { 0 };
	static Vec3f s_moveStartPos3d = { 0 };

	bool pointInsideOBB2d(const Vec2f pt, const Vec3f* bounds, const Vec3f* pos, const Mat3* mtx)
	{
		// Transform the origin, relative to the position.
		Vec3f relOrigin = { pt.x - pos->x, 0.0f, pt.z - pos->z };
		Vec3f localPt;
		localPt.x = relOrigin.x * mtx->m0.x + relOrigin.y * mtx->m1.x + relOrigin.z * mtx->m2.x;
		localPt.z = relOrigin.x * mtx->m0.z + relOrigin.y * mtx->m1.z + relOrigin.z * mtx->m2.z;

		return localPt.x >= bounds[0].x && localPt.x <= bounds[1].x && localPt.z >= bounds[0].z && localPt.z <= bounds[1].z;
	}

	void addEntityToSector(EditorSector* sector, Entity* entity, Vec3f* pos)
	{
		EditorObject obj;
		obj.entityId = addEntityToLevel(entity);
		obj.angle = 0.0f;
		obj.pitch = 0.0f;
		obj.roll = 0.0f;
		obj.pos = *pos;
		obj.diff = 1;	// default
		obj.transform = { 0 };
		obj.transform.m0.x = 1.0f;
		obj.transform.m1.y = 1.0f;
		obj.transform.m2.z = 1.0f;
		sector->obj.push_back(obj);
	}
	
	void findHoveredEntity2d(Vec2f worldPos)
	{
		// TODO: This isn't very efficient, but for now loop through all of the sectors.
		const s32 sectorCount = (s32)s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		EditorSector* objSector = nullptr;

		f32 maxY = -FLT_MAX;
		s32 maxObjId = -1;

		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if (!sector_isInteractable(sector) || !sector_onActiveLayer(sector)) { continue; }

			// Check for objects.
			// Must be inside the range and pick the highest top
			// in order to duplicate the 3D behavior.
			const s32 objCount = (s32)sector->obj.size();
			const EditorObject* obj = sector->obj.data();
			for (s32 i = 0; i < objCount; i++, obj++)
			{
				const Entity* entity = &s_level.entities[obj->entityId];

				bool cursorInside = false;
				f32 yTop = 0.0f;
				if (entity->type == ETYPE_3D)
				{
					cursorInside = pointInsideOBB2d({ worldPos.x, worldPos.z }, entity->obj3d->bounds, &obj->pos, &obj->transform);
					yTop = obj->pos.y + entity->obj3d->bounds[1].y;
				}
				else
				{
					const f32 dx = fabsf(obj->pos.x - worldPos.x);
					const f32 dz = fabsf(obj->pos.z - worldPos.z);
					const f32 w = entity->size.x * 0.5f;

					cursorInside = dx <= w && dz <= w;
					yTop = obj->pos.y + entity->size.z;
				}

				if (cursorInside && yTop > maxY)
				{
					maxObjId = i;
					maxY = yTop;
					objSector = sector;
				}
			}
		}
		if (maxObjId >= 0)
		{
			selection_entity(SA_SET_HOVERED, objSector, maxObjId);
		}
	}

	void handleEntityInsert(Vec3f worldPos, RayHitInfo* info/*=nullptr*/)
	{
		const bool insertEntity = getEditAction(ACTION_PLACE);
		if (!insertEntity) { return; }

		EditorSector* hoverSector = nullptr;
		if (s_view == EDIT_VIEW_2D)
		{
			hoverSector = edit_getHoverSector2dAtCursor();
		}
		else if (info)
		{
			hoverSector = info->hitSectorId >= 0 ? &s_level.sectors[info->hitSectorId] : nullptr;
		}
		if (!hoverSector) { return; }

		snapToGrid(&worldPos);
		if (s_view == EDIT_VIEW_2D)
		{
			// Place on the floor.
			worldPos.y = hoverSector->floorHeight;
		}
		else
		{
			// Clamp to the sector floor/ceiling.
			worldPos.y = max(hoverSector->floorHeight, worldPos.y);
			worldPos.y = min(hoverSector->ceilHeight, worldPos.y);
		}
		if (hoverSector && s_selectedEntity >= 0)
		{
			addEntityToSector(hoverSector, &s_entityDefList[s_selectedEntity], &worldPos);
			cmd_objectListSnapshot(LName_AddObject, hoverSector->id);
		}
	}

	void entityComputeDragSelect()
	{
		if (s_dragSelect.curPos.x == s_dragSelect.startPos.x && s_dragSelect.curPos.z == s_dragSelect.startPos.z)
		{
			return;
		}
		if (s_dragSelect.mode == DSEL_SET)
		{
			selection_clear(SEL_ENTITY_BIT, false);
		}

		if (s_view == EDIT_VIEW_2D)
		{
			// Convert drag select coordinates to world space.
			// World position from viewport position, relative mouse position and zoom.
			Vec2f worldPos[2];
			worldPos[0].x = s_viewportPos.x + f32(s_dragSelect.curPos.x) * s_zoom2d;
			worldPos[0].z = -(s_viewportPos.z + f32(s_dragSelect.curPos.z) * s_zoom2d);
			worldPos[1].x = s_viewportPos.x + f32(s_dragSelect.startPos.x) * s_zoom2d;
			worldPos[1].z = -(s_viewportPos.z + f32(s_dragSelect.startPos.z) * s_zoom2d);

			Vec3f aabb[] =
			{
				{ std::min(worldPos[0].x, worldPos[1].x), 0.0f, std::min(worldPos[0].z, worldPos[1].z) },
				{ std::max(worldPos[0].x, worldPos[1].x), 0.0f, std::max(worldPos[0].z, worldPos[1].z) }
			};

			const size_t sectorCount = s_level.sectors.size();
			EditorSector* sector = s_level.sectors.data();
			for (size_t s = 0; s < sectorCount; s++, sector++)
			{
				if (!sector_isInteractable(sector) || !sector_onActiveLayer(sector)) { continue; }
				if (!aabbOverlap2d(sector->bounds, aabb)) { continue; }

				const size_t objCount = sector->obj.size();
				const EditorObject* obj = sector->obj.data();
				for (size_t v = 0; v < objCount; v++, obj++)
				{
					if (obj->pos.x >= aabb[0].x && obj->pos.x < aabb[1].x && obj->pos.z >= aabb[0].z && obj->pos.z < aabb[1].z)
					{
						selection_entity(s_dragSelect.mode == DSEL_REM ? SA_REMOVE : SA_ADD, sector, (s32)v);
					}
				}
			}
		}
		else if (s_view == EDIT_VIEW_3D)
		{
			// In 3D, build a frustum and select point inside the frustum.
			// The near plane corners of the frustum are derived from the screenspace rectangle.
			const s32 x0 = std::min(s_dragSelect.startPos.x, s_dragSelect.curPos.x);
			const s32 x1 = std::max(s_dragSelect.startPos.x, s_dragSelect.curPos.x);
			const s32 z0 = std::min(s_dragSelect.startPos.z, s_dragSelect.curPos.z);
			const s32 z1 = std::max(s_dragSelect.startPos.z, s_dragSelect.curPos.z);
			// Directions to the near-plane frustum corners.
			const Vec3f d0 = edit_viewportCoordToWorldDir3d({ x0, z0 });
			const Vec3f d1 = edit_viewportCoordToWorldDir3d({ x1, z0 });
			const Vec3f d2 = edit_viewportCoordToWorldDir3d({ x1, z1 });
			const Vec3f d3 = edit_viewportCoordToWorldDir3d({ x0, z1 });
			// Camera forward vector.
			const Vec3f fwd = { -s_camera.viewMtx.m2.x, -s_camera.viewMtx.m2.y, -s_camera.viewMtx.m2.z };
			// Trace forward at the screen center to get the likely focus sector.
			Vec3f centerViewDir = edit_viewportCoordToWorldDir3d({ s_viewportSize.x / 2, s_viewportSize.z / 2 });
			RayHitInfo hitInfo;
			Ray ray = { s_camera.pos, centerViewDir, 1000.0f };

			f32 nearDist = 1.0f;
			f32 farDist = 100.0f;
			if (traceRay(&ray, &hitInfo, false, false) && hitInfo.hitSectorId >= 0)
			{
				EditorSector* hitSector = &s_level.sectors[hitInfo.hitSectorId];
				Vec3f bbox[] =
				{
					{hitSector->bounds[0].x, hitSector->floorHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->floorHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->floorHeight, hitSector->bounds[1].z},
					{hitSector->bounds[0].x, hitSector->floorHeight, hitSector->bounds[1].z},

					{hitSector->bounds[0].x, hitSector->ceilHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->ceilHeight, hitSector->bounds[0].z},
					{hitSector->bounds[1].x, hitSector->ceilHeight, hitSector->bounds[1].z},
					{hitSector->bounds[0].x, hitSector->ceilHeight, hitSector->bounds[1].z},
				};
				f32 maxDist = 0.0f;
				for (s32 i = 0; i < 8; i++)
				{
					Vec3f offset = { bbox[i].x - s_camera.pos.x, bbox[i].y - s_camera.pos.y, bbox[i].z - s_camera.pos.z };
					f32 dist = TFE_Math::dot(&offset, &fwd);
					if (dist > maxDist)
					{
						maxDist = dist;
					}
				}
				if (maxDist > 0.0f) { farDist = maxDist; }
			}
			// Build Planes.
			Vec3f near = { s_camera.pos.x + nearDist * fwd.x, s_camera.pos.y + nearDist * fwd.y, s_camera.pos.z + nearDist * fwd.z };
			Vec3f far = { s_camera.pos.x + farDist * fwd.x, s_camera.pos.y + farDist * fwd.y, s_camera.pos.z + farDist * fwd.z };

			Vec3f left = TFE_Math::cross(&d3, &d0);
			Vec3f right = TFE_Math::cross(&d1, &d2);
			Vec3f top = TFE_Math::cross(&d0, &d1);
			Vec3f bot = TFE_Math::cross(&d2, &d3);

			left = TFE_Math::normalize(&left);
			right = TFE_Math::normalize(&right);
			top = TFE_Math::normalize(&top);
			bot = TFE_Math::normalize(&bot);

			const Vec4f plane[] =
			{
				{ -fwd.x, -fwd.y, -fwd.z,  TFE_Math::dot(&fwd, &near) },
				{  fwd.x,  fwd.y,  fwd.z, -TFE_Math::dot(&fwd, &far) },
				{ left.x, left.y, left.z, -TFE_Math::dot(&left, &s_camera.pos) },
				{ right.x, right.y, right.z, -TFE_Math::dot(&right, &s_camera.pos) },
				{ top.x, top.y, top.z, -TFE_Math::dot(&top, &s_camera.pos) },
				{ bot.x, bot.y, bot.z, -TFE_Math::dot(&bot, &s_camera.pos) }
			};

			const size_t sectorCount = s_level.sectors.size();
			EditorSector* sector = s_level.sectors.data();
			for (size_t s = 0; s < sectorCount; s++, sector++)
			{
				if (!sector_isInteractable(sector) || !sector_onActiveLayer(sector)) { continue; }

				// For now, do them all...
				const size_t objCount = sector->obj.size();
				const EditorObject* obj = sector->obj.data();
				for (size_t v = 0; v < objCount; v++, obj++)
				{
					bool inside = true;
					for (s32 p = 0; p < 6; p++)
					{
						if (plane[p].x*obj->pos.x + plane[p].y*obj->pos.y + plane[p].z*obj->pos.z + plane[p].w > 0.0f)
						{
							inside = false;
							break;
						}
					}

					if (inside)
					{
						selection_entity(s_dragSelect.mode == DSEL_REM ? SA_REMOVE : SA_ADD, sector, (s32)v);
					}
				}
			}
		}
	}

	void edit_moveEntity(Vec3f delta)
	{
		const u32 entityCount = selection_getCount(SEL_ENTITY);
		for (u32 i = 0; i < entityCount; i++)
		{
			EditorSector* sector = nullptr;
			s32 index = -1;
			selection_getEntity(i, sector, index);
			if (!sector || index < 0) { continue; }

			EditorObject* obj = &sector->obj[index];
			obj->pos.x += delta.x;
			obj->pos.y += delta.y;
			obj->pos.z += delta.z;
		}
	}

	void edit_entityMoveTo(MoveTo moveTo)
	{
		s_searchKey++;
		s_idList.clear();

		u32 name = 0;
		const u32 entityCount = selection_getCount(SEL_ENTITY);
		for (u32 i = 0; i < entityCount; i++)
		{
			EditorSector* sector = nullptr;
			s32 index = -1;
			selection_getEntity(i, sector, index);
			if (!sector || index < 0) { continue; }

			EditorObject* obj = &sector->obj[index];
			const Entity* entity = &s_level.entities[obj->entityId];
			if (moveTo == MOVETO_FLOOR)
			{
				name = LName_MoveObjectToFloor;
				f32 base = obj->pos.y;
				if (entity->type == ETYPE_3D)
				{
					base = obj->pos.y + entity->obj3d->bounds[0].y;
				}

				if (base != sector->floorHeight)
				{
					if (entity->type == ETYPE_3D)
					{
						obj->pos.y = sector->floorHeight - entity->obj3d->bounds[0].y;
					}
					else
					{
						obj->pos.y = sector->floorHeight;
					}
					if (sector->searchKey != s_searchKey)
					{
						s_idList.push_back(sector->id);
					}
				}
			}
			else if (moveTo == MOVETO_CEIL)
			{
				name = LName_MoveObjectToCeil;
				f32 top;
				if (entity->type == ETYPE_3D)
				{
					top = obj->pos.y + entity->obj3d->bounds[1].y;
				}
				else
				{
					// Assume the pivot is at the ceiling.
					top = obj->pos.y;
				}

				if (top != sector->ceilHeight)
				{
					if (entity->type == ETYPE_3D)
					{
						obj->pos.y = sector->ceilHeight - entity->obj3d->bounds[1].y;
					}
					else
					{
						obj->pos.y = sector->ceilHeight;
					}
					if (sector->searchKey != s_searchKey)
					{
						s_idList.push_back(sector->id);
					}
				}
			}
		}
		if (!s_idList.empty())
		{
			cmd_sectorSnapshot(name, s_idList);
		}
	}
	
	// TODO: Delete.
#if 0
	void handleEntityEdit(RayHitInfo* hitInfo, const Vec3f& rayDir)
	{
		selection_clearHovered();
		EditorSector* hoverSector = nullptr;
		if (s_view == EDIT_VIEW_2D) // Get the hover sector in 2D.
		{
			// Get the hovered sector as well for insertion later.
			hoverSector = edit_getHoverSector2dAtCursor();
		}
		else // Or the hit sector in 3D.
		{
			hoverSector = hitInfo->hitSectorId < 0 ? nullptr : &s_level.sectors[hitInfo->hitSectorId];

			// See if we can select an object.
			RayHitInfo hitInfoObj;
			const Ray ray = { s_camera.pos, rayDir, 1000.0f };
			const bool rayHit = traceRay(&ray, &hitInfoObj, false, false, true/*canHitObjects*/);
			if (rayHit && hitInfoObj.dist < hitInfo->dist && hitInfoObj.hitObjId >= 0)
			{
				s_cursor3d = hitInfoObj.hitPos;
				selection_entity(SA_SET_HOVERED, &s_level.sectors[hitInfoObj.hitSectorId], hitInfoObj.hitObjId);
			}
		}

		// Get the hovered object, if there is one.
		EditorSector* hoveredObjSector = nullptr;
		s32 hoveredObjIndex = -1;
		selection_getEntity(SEL_INDEX_HOVERED, hoveredObjSector, hoveredObjIndex);

		if (getEditAction(ACTION_PLACE) && hoverSector && s_selectedEntity >= 0 && s_selectedEntity < (s32)s_entityDefList.size())
		{
			Vec3f pos = s_cursor3d;
			snapToGrid(&pos);

			if (s_view == EDIT_VIEW_2D)
			{
				// Place on the floor.
				pos.y = hoverSector->floorHeight;
			}
			else
			{
				// Clamp to the sector floor/ceiling.
				pos.y = max(hoverSector->floorHeight, pos.y);
				pos.y = min(hoverSector->ceilHeight, pos.y);
			}
			addEntityToSector(hoverSector, &s_entityDefList[s_selectedEntity], &pos);
			if (hoverSector)
			{
				cmd_objectListSnapshot(LName_AddObject, hoverSector->id);
			}
		}
		else if (s_singleClick && hoveredObjSector)
		{
			selection_entity(SA_SET, hoveredObjSector, hoveredObjIndex);

			EditorObject* obj = &hoveredObjSector->obj[hoveredObjIndex];
			s_editMove = true;
			s_moveBasePos3d = obj->pos;

			if (s_view == EDIT_VIEW_2D)
			{
				s_cursor3d.y = obj->pos.y;
			}
			else
			{
				f32 gridHeight = s_grid.height;
				s_grid.height = s_cursor3d.y;

				s_cursor3d = rayGridPlaneHit(s_camera.pos, rayDir);
				s_grid.height = gridHeight;
			}
			s_moveStartPos3d = s_cursor3d;
		}
		else if (s_leftMouseDown)
		{
			// TODO: Proper selection movement.
			// Get the "current" object, if there is one.
			EditorSector* curObjSector = nullptr;
			s32 curObjIndex = -1;
			selection_getEntity(0, curObjSector, curObjIndex);

			if (s_editMove && curObjSector)
			{
				EditorObject* obj = &curObjSector->obj[curObjIndex];
				if (s_view == EDIT_VIEW_2D)
				{
					s_cursor3d.y = obj->pos.y;
				}
				else
				{
					f32 gridHeight = s_grid.height;
					s_grid.height = s_moveStartPos3d.y;

					s_cursor3d = rayGridPlaneHit(s_camera.pos, rayDir);
					s_grid.height = gridHeight;
				}

				if (getEditAction(ACTION_MOVE_Y))
				{
					s_curVtxPos = s_moveStartPos3d;
					f32 yNew = moveAlongRail({ 0.0f, 1.0f, 0.0f }).y;

					f32 yPrev = obj->pos.y;
					obj->pos.y = s_moveBasePos3d.y + (yNew - s_moveStartPos3d.y);
					f32 yDelta = obj->pos.y - yPrev;
					snapToGridY(&obj->pos.y);

					if (s_view == EDIT_VIEW_3D)
					{
						Vec3f rail[] = { obj->pos, { obj->pos.x, obj->pos.y + 1.0f, obj->pos.z},
							{ obj->pos.x, obj->pos.y - 1.0f, obj->pos.z } };
						Vec3f moveDir = { 0.0f, yDelta, 0.0f };
						moveDir = TFE_Math::normalize(&moveDir);
						viewport_setRail(rail, 2, &moveDir);

						s_moveStartPos3d.x = s_curVtxPos.x;
						s_moveStartPos3d.z = s_curVtxPos.z;
					}
				}
				else if (s_moveStartPos3d.x != s_cursor3d.x || s_moveStartPos3d.z != s_cursor3d.z)
				{
					Vec3f prevPos = obj->pos;
					EditorSector* prevSector = curObjSector;

					if (getEditAction(ACTION_MOVE_X))
					{
						obj->pos.x = (s_cursor3d.x - s_moveStartPos3d.x) + s_moveBasePos3d.x;
					}
					else if (getEditAction(ACTION_MOVE_Z))
					{
						obj->pos.z = (s_cursor3d.z - s_moveStartPos3d.z) + s_moveBasePos3d.z;
					}
					else
					{
						obj->pos.x = (s_cursor3d.x - s_moveStartPos3d.x) + s_moveBasePos3d.x;
						obj->pos.z = (s_cursor3d.z - s_moveStartPos3d.z) + s_moveBasePos3d.z;
					}
					Vec3f moveDir = { obj->pos.x - prevPos.x, 0.0f, obj->pos.z - prevPos.z };
					snapToGrid(&obj->pos);

					if (s_view == EDIT_VIEW_3D)
					{
						Vec3f boxPos = { obj->pos.x, s_cursor3d.y, obj->pos.z };
						Vec3f rail[] = { boxPos, { boxPos.x + 1.0f, boxPos.y, boxPos.z}, { boxPos.x, boxPos.y, boxPos.z + 1.0f },
							{ boxPos.x - 1.0f, boxPos.y, boxPos.z}, { boxPos.x, boxPos.y, boxPos.z - 1.0f } };
						moveDir = TFE_Math::normalize(&moveDir);
						viewport_setRail(rail, 4, &moveDir);
					}

					if (!isPointInsideSector3d(curObjSector, obj->pos))
					{
						EditorObject objCopy = *obj;
						edit_deleteObject(curObjSector, curObjIndex);
						selection_clearHovered();

						EditorSector* newSector = findSector3d(objCopy.pos);
						if (newSector)
						{
							curObjSector = newSector;
							curObjIndex = (s32)newSector->obj.size();
							selection_entity(SA_SET, curObjSector, curObjIndex);
							newSector->obj.push_back(objCopy);
						}
						else
						{
							// Try the floor.
							Vec3f testPos = { objCopy.pos.x, objCopy.pos.y + 4.0f, objCopy.pos.z };
							newSector = findSector3d(testPos);
							if (newSector)
							{
								curObjSector = newSector;
								curObjIndex = (s32)newSector->obj.size();
								objCopy.pos.y = newSector->floorHeight;

								selection_entity(SA_SET, curObjSector, curObjIndex);
								newSector->obj.push_back(objCopy);
							}
							// Try the ceiling.
							if (!newSector)
							{
								testPos.y = objCopy.pos.y - 4.0f;
								newSector = findSector3d(testPos);
								if (newSector)
								{
									curObjSector = newSector;
									curObjIndex = (s32)newSector->obj.size();
									objCopy.pos.y = newSector->ceilHeight;

									selection_entity(SA_SET, curObjSector, curObjIndex);
									newSector->obj.push_back(objCopy);
								}
							}
							// Otherwise just abort.
							if (!newSector)
							{
								objCopy.pos = prevPos;
								curObjIndex = (s32)prevSector->obj.size();
								curObjSector = prevSector;

								selection_entity(SA_SET, curObjSector, curObjIndex);
								prevSector->obj.push_back(objCopy);
							}
						}
					}
				}

				if (curObjSector && curObjIndex >= 0)
				{
					obj = &curObjSector->obj[curObjIndex];
					obj->pos.y = max(curObjSector->floorHeight, obj->pos.y);
					obj->pos.y = min(curObjSector->ceilHeight, obj->pos.y);
				}
			}
			else
			{
				s_editMove = false;
			}
		}
		else
		{
			s_editMove = false;
		}

		// Get the "current" object, if there is one.
		EditorSector* curObjSector = nullptr;
		s32 curObjIndex = -1;
		selection_getEntity(0, curObjSector, curObjIndex);
	
		if (s_rotationDelta != 0 && getEditAction(ACTION_ROTATE))
		{
			EditorObject* obj = nullptr;
			if (curObjSector && curObjIndex >= 0)
			{
				obj = &curObjSector->obj[curObjIndex];
			}
			else if (hoveredObjSector && hoveredObjIndex >= 0)
			{
				obj = &hoveredObjSector->obj[hoveredObjIndex];
			}
			if (obj)
			{
				obj->angle += f32(s_rotationDelta) * 2.0f*PI / 32.0f;
				if (s_level.entities[obj->entityId].obj3d)
				{
					compute3x3Rotation(&obj->transform, obj->angle, obj->pitch, obj->roll);
				}
			}
		}

		if (getEditAction(ACTION_DELETE))
		{
			bool deleted = false;
			s32 sectorId = -1;
			if (curObjSector && curObjIndex >= 0)
			{
				edit_deleteObject(curObjSector, curObjIndex);
				sectorId = curObjSector->id;
				deleted = true;
			}
			else if (hoveredObjSector && hoveredObjIndex >= 0)
			{
				edit_deleteObject(hoveredObjSector, hoveredObjIndex);
				sectorId = hoveredObjSector->id;
				deleted = true;
			}
			if (deleted)
			{
				selection_clearHovered();
				selection_clear(SEL_ENTITY_BIT);
			}
			if (sectorId >= 0)
			{
				cmd_objectListSnapshot(LName_DeleteObject, sectorId);
			}
		}
	}
#endif
}

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
		const bool insertEntity = isShortcutPressed(SHORTCUT_PLACE);
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
}

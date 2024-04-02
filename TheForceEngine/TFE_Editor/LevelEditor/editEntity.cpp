#include "editEntity.h"
#include "hotkeys.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "sharedState.h"
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_System/math.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editor.h>
#include <TFE_Input/input.h>

#include <TFE_Ui/imGUI/imgui.h>
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

	void handleEntityEdit(RayHitInfo* hitInfo, const Vec3f& rayDir)
	{
		// TODO: Hotkeys.
		bool placeEntity = TFE_Input::keyPressed(KEY_INSERT);
		bool moveAlongX = TFE_Input::keyDown(KEY_X);
		bool moveAlongY = TFE_Input::keyDown(KEY_Y) && s_view != EDIT_VIEW_2D;
		bool moveAlongZ = TFE_Input::keyDown(KEY_Z);
		const s32 layer = (s_editFlags & LEF_SHOW_ALL_LAYERS) ? LAYER_ANY : s_curLayer;

		EditorSector* hoverSector = nullptr;
		if (s_view == EDIT_VIEW_2D) // Get the hover sector in 2D.
		{
			hoverSector = s_featureHovered.sector;
			s_featureHovered = {};

			f32 maxY = -FLT_MAX;
			s32 maxObjId = -1;

			// TODO: This isn't very efficient, but for now loop through all of the sectors.
			const s32 sectorCount = (s32)s_level.sectors.size();
			EditorSector* sector = s_level.sectors.data();
			EditorSector* objSector = nullptr;
			for (s32 s = 0; s < sectorCount; s++, sector++)
			{
				if (!sector_isInteractable(sector)) { continue; }

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
						cursorInside = pointInsideOBB2d({ s_cursor3d.x, s_cursor3d.z }, entity->obj3d->bounds, &obj->pos, &obj->transform);
						yTop = obj->pos.y + entity->obj3d->bounds[1].y;
					}
					else
					{
						const f32 dx = fabsf(obj->pos.x - s_cursor3d.x);
						const f32 dz = fabsf(obj->pos.z - s_cursor3d.z);
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

				if (maxObjId >= 0)
				{
					s_featureHovered.featureIndex = maxObjId;
					s_featureHovered.sector = objSector;
					s_featureHovered.prevSector = nullptr;
					s_featureHovered.isObject = true;
				}
			}
		}
		else // Or the hit sector in 3D.
		{
			hoverSector = hitInfo->hitSectorId < 0 ? nullptr : &s_level.sectors[hitInfo->hitSectorId];

			// See if we can select an object.
			RayHitInfo hitInfoObj;
			const Ray ray = { s_camera.pos, rayDir, 1000.0f, layer };
			const bool rayHit = traceRay(&ray, &hitInfoObj, false, false, true/*canHitObjects*/);
			if (rayHit && hitInfoObj.dist < hitInfo->dist && hitInfoObj.hitObjId >= 0)
			{
				s_cursor3d = hitInfoObj.hitPos;

				s_featureHovered.featureIndex = hitInfoObj.hitObjId;
				s_featureHovered.sector = &s_level.sectors[hitInfoObj.hitSectorId];
				s_featureHovered.prevSector = nullptr;
				s_featureHovered.isObject = true;
			}
		}

		if (placeEntity && hoverSector && s_selectedEntity >= 0 && s_selectedEntity < (s32)s_entityDefList.size())
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
		}
		else if (s_singleClick && s_featureHovered.isObject && s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
		{
			s_featureCur = s_featureHovered;

			EditorObject* obj = &s_featureCur.sector->obj[s_featureCur.featureIndex];
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
		else if (TFE_Input::mouseDown(MBUTTON_LEFT))
		{
			if (s_editMove && s_featureCur.isObject && s_featureCur.sector && s_featureCur.featureIndex >= 0)
			{
				EditorObject* obj = &s_featureCur.sector->obj[s_featureCur.featureIndex];
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

				if (moveAlongY)
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
					EditorSector* prevSector = s_featureCur.sector;

					if (moveAlongX)
					{
						obj->pos.x = (s_cursor3d.x - s_moveStartPos3d.x) + s_moveBasePos3d.x;
					}
					else if (moveAlongZ)
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

					if (!isPointInsideSector3d(s_featureCur.sector, obj->pos, layer))
					{
						EditorObject objCopy = *obj;
						edit_deleteObject(s_featureCur.sector, s_featureCur.featureIndex);
						s_featureHovered = {};

						EditorSector* newSector = findSector3d(objCopy.pos, layer);
						if (newSector)
						{
							s_featureCur.featureIndex = (s32)newSector->obj.size();
							s_featureCur.sector = newSector;
							newSector->obj.push_back(objCopy);
						}
						else
						{
							// Try the floor.
							Vec3f testPos = { objCopy.pos.x, objCopy.pos.y + 4.0f, objCopy.pos.z };
							newSector = findSector3d(testPos, layer);
							if (newSector)
							{
								s_featureCur.featureIndex = (s32)newSector->obj.size();
								s_featureCur.sector = newSector;
								objCopy.pos.y = newSector->floorHeight;
								newSector->obj.push_back(objCopy);
							}
							// Try the ceiling.
							if (!newSector)
							{
								testPos.y = objCopy.pos.y - 4.0f;
								newSector = findSector3d(testPos, layer);
								if (newSector)
								{
									s_featureCur.featureIndex = (s32)newSector->obj.size();
									s_featureCur.sector = newSector;
									objCopy.pos.y = newSector->ceilHeight;
									newSector->obj.push_back(objCopy);
								}
							}
							// Otherwise just abort.
							if (!newSector)
							{
								objCopy.pos = prevPos;
								s_featureCur.featureIndex = (s32)prevSector->obj.size();
								s_featureCur.sector = prevSector;
								prevSector->obj.push_back(objCopy);
							}
						}
					}
				}

				if (s_featureCur.isObject && s_featureCur.sector && s_featureCur.featureIndex >= 0)
				{
					obj = &s_featureCur.sector->obj[s_featureCur.featureIndex];
					obj->pos.y = max(s_featureCur.sector->floorHeight, obj->pos.y);
					obj->pos.y = min(s_featureCur.sector->ceilHeight, obj->pos.y);
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

		s32 dx, dy;
		TFE_Input::getMouseWheel(&dx, &dy);
		bool del = TFE_Input::keyPressed(KEY_DELETE);

		if (dy && TFE_Input::keyModDown(KEYMOD_CTRL))
		{
			EditorObject* obj = nullptr;
			if (dy && s_featureCur.isObject && s_featureCur.sector && s_featureCur.featureIndex >= 0)
			{
				obj = &s_featureCur.sector->obj[s_featureCur.featureIndex];
			}
			else if (dy && s_featureHovered.isObject && s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
			{
				obj = &s_featureHovered.sector->obj[s_featureHovered.featureIndex];
			}
			if (obj)
			{
				obj->angle += f32(dy) * 2.0f*PI / 32.0f;
				if (s_level.entities[obj->entityId].obj3d)
				{
					compute3x3Rotation(&obj->transform, obj->angle, obj->pitch, obj->roll);
				}
			}
		}

		if (del)
		{
			bool deleted = false;
			if (s_featureCur.isObject && s_featureCur.sector && s_featureCur.featureIndex >= 0)
			{
				edit_deleteObject(s_featureCur.sector, s_featureCur.featureIndex);
				deleted = true;
			}
			else if (s_featureHovered.isObject && s_featureHovered.sector && s_featureHovered.featureIndex >= 0)
			{
				edit_deleteObject(s_featureHovered.sector, s_featureHovered.featureIndex);
				deleted = true;
			}
			if (deleted)
			{
				s_featureHovered = {};
				s_featureCur = {};
			}
		}
	}
}

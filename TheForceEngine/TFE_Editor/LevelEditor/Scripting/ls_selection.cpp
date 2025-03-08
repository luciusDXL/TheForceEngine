#include "ls_selection.h"
#include "levelEditorScripts.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>
#include <TFE_Editor/LevelEditor/selection.h>
#include <TFE_ForceScript/scriptInterface.h>
#include <TFE_ForceScript/float2.h>
#include <algorithm>
#include <angelscript.h>

namespace LevelEditor
{
	// TODO: Finish Selection API once selection editor code is refactored.

	// Get the position or center of an item in the current selection.
	TFE_ForceScript::float2 LS_Selection::getPositionXZ(s32 index)
	{
		TFE_ForceScript::float2 pos(0.0f, 0.0f);
		switch (s_editMode)
		{
			case LEDIT_VERTEX:
			{
				EditorSector* sector = nullptr;
				s32 vertexIndex = -1;
				if (selection_getVertex(index, sector, vertexIndex))
				{
					const Vec2f& vtx = sector->vtx[vertexIndex];
					pos.x = vtx.x;
					pos.y = vtx.z;
				}
			} break;
			case LEDIT_WALL:
			{
				EditorSector* sector = nullptr;
				s32 wallIndex = -1;
				HitPart part = HP_NONE;
				if (selection_getSurface(index, sector, wallIndex, &part))
				{
					if (part == HP_FLOOR || part == HP_CEIL)
					{
						// Center of the sector.
						pos.x = (sector->bounds[0].x + sector->bounds[1].x) * 0.5f;
						pos.y = (sector->bounds[0].z + sector->bounds[1].z) * 0.5f;
					}
					else if (part == HP_SIGN)
					{
						// TODO
					}
					else
					{
						// Center of the wall.
						const EditorWall* wall = &sector->walls[wallIndex];
						const Vec2f v0 = sector->vtx[wall->idx[0]];
						const Vec2f v1 = sector->vtx[wall->idx[1]];
						pos.x = (v0.x + v1.x) * 0.5f;
						pos.y = (v0.z + v1.z) * 0.5f;
					}
				}
			} break;
			case LEDIT_SECTOR:
			{
				EditorSector* sector = nullptr;
				if (selection_getSector(index, sector))
				{
					// Center of the sector.
					pos.x = (sector->bounds[0].x + sector->bounds[1].x) * 0.5f;
					pos.y = (sector->bounds[0].z + sector->bounds[1].z) * 0.5f;
				}
			} break;
			case LEDIT_ENTITY:
			{
				EditorSector* sector = nullptr;
				s32 objIndex = -1;
				if (selection_getEntity(index, sector, objIndex))
				{
					const EditorObject* obj = &sector->obj[objIndex];
					pos.x = obj->pos.x;
					pos.y = obj->pos.z;
				}
			} break;
			case LEDIT_GUIDELINES:
			{
				Guideline* guideline = nullptr;
				if (selection_getGuideline(index, guideline))
				{
					// Compute the bounds, then get the center.
					const s32 vtxCount = (s32)guideline->vtx.size();
					const Vec2f* vtx = guideline->vtx.data();
					Vec2f minPos = vtx[0];
					Vec2f maxPos = minPos;
					for (s32 v = 1; v < vtxCount; v++)
					{
						minPos.x = std::min(minPos.x, vtx[v].x);
						minPos.z = std::min(minPos.z, vtx[v].z);

						maxPos.x = std::max(maxPos.x, vtx[v].x);
						maxPos.z = std::max(maxPos.z, vtx[v].z);
					}
					pos.x = (minPos.x + maxPos.x) * 0.5f;
					pos.y = (minPos.z + maxPos.z) * 0.5f;
				}
			} break;
			case LEDIT_NOTES:
			{
				LevelNote* note = nullptr;
				if (selection_getLevelNote(index, note))
				{
					pos.x = note->pos.x;
					pos.y = note->pos.z;
				}
			} break;
		}

		return pos;
	}

	bool LS_Selection::scriptRegister(ScriptAPI api)
	{
		ScriptClassBegin("Selection", "selection", api);
		{
			ScriptObjMethod("float2 getPositionXZ(int = 0)", getPositionXZ);
		}
		ScriptClassEnd();
	}
}

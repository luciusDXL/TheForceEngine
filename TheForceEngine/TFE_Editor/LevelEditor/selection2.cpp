#include "levelEditor.h"
#include "levelEditorData.h"
#include "selection2.h"
#include "sharedState.h"

using namespace TFE_Editor;

namespace LevelEditor
{
	SelectionList s_selectionList2[SEL_COUNT];
	SelectionListId s_currentSelection = SEL_VERTEX;
	FeatureId s_hovered = FEATUREID_NULL;

	bool selection_insertVertex(FeatureId id, Vec2f value);
	bool selection_removeVertex(FeatureId id, Vec2f value);
	bool selection_isVertexInSet(FeatureId id);
	bool selection_insertFeatureId(SelectionList& list, FeatureId id);
	bool selection_removeFeatureId(SelectionList& list, FeatureId id);
	bool selection_isFeatureIdInSet(SelectionList& list, FeatureId id, s32* index = nullptr);
	bool selection_featureId(SelectAction action, FeatureId id);
	void selection_buildDerived();
	FeatureId selection_getFeatureId(s32 index, SelectionListId listId);

	// General Interface
	void selection_clear(u32 selections, bool clearDragSelect)
	{
		for (s32 i = 0; i < SEL_COUNT; i++)
		{
			if ((1 << i) & selections)
			{
				s_selectionList2[i].clear();
			}
		}
		if (clearDragSelect)
		{
			s_dragSelect.active = false;
		}
	}

	void selection_setCurrent(SelectionListId id)
	{
		assert(id < SEL_CURRENT);
		selection_clearHovered();
		s_currentSelection = id;
	}

	void selection_clearHovered()
	{
		s_hovered = FEATUREID_NULL;
	}

	bool selection_hasHovered()
	{
		return s_hovered != FEATUREID_NULL;
	}

	bool selection_hasSelection(SelectionListId id)
	{
		return !s_selectionList2[id >= SEL_CURRENT ? s_currentSelection : id].empty();
	}
		
	// Selection interface.
	bool selection_vertex(SelectAction action, EditorSector* sector, s32 index, u32 flags)
	{
		if (!sector || index < 0 || index >= (s32)sector->vtx.size()) { return false; }

		bool buildDerived = false;
		bool actionDone = false;
		FeatureId id = createFeatureId(sector, index);
		switch (action)
		{
			case SA_SET:
			{
				s_selectionList2[SEL_VERTEX].clear();
				actionDone = selection_insertVertex(id, sector->vtx[index]);
				buildDerived = actionDone;
			} break;
			case SA_ADD:
			{
				actionDone = selection_insertVertex(id, sector->vtx[index]);
				buildDerived = actionDone;
			} break;
			case SA_REMOVE:
			{
				actionDone = selection_removeVertex(id, sector->vtx[index]);
				buildDerived = actionDone;
			} break;
			case SA_TOGGLE:
			{
				if (selection_isVertexInSet(id))
				{
					actionDone = selection_removeVertex(id, sector->vtx[index]);
				}
				else
				{
					actionDone = selection_insertVertex(id, sector->vtx[index]);
				}
				buildDerived = actionDone;
			} break;
			case SA_CHECK_INCLUSION:
			{
				actionDone = selection_isVertexInSet(id);
			} break;
			case SA_SET_HOVERED:
			{
				s_hovered = id;
				actionDone = true;
			} break;
		}

		if (buildDerived)
		{
			selection_buildDerived();
		}
		return actionDone;
	}
				
	bool selection_surface(SelectAction action, EditorSector* sector, s32 index, HitPart part, u32 flags)
	{
		if (!sector) { return false; }
		if (part != HP_FLOOR && part != HP_CEIL && index < 0 || index >= (s32)sector->walls.size()) { return false; }

		const FeatureId id = createFeatureId(sector, index, (s32)part);
		return selection_featureId(action, id);
	}

	bool selection_entity(SelectAction action, EditorSector* sector, s32 index, u32 flags)
	{
		// TODO: Handle entity's outside of sectors.
		if (!sector) { return false; }
		const FeatureId id = createFeatureId(sector, index);
		return selection_featureId(action, id);
	}

	bool selection_sector(SelectAction action, EditorSector* sector, u32 flags)
	{
		if (!sector) { return false; }
		const FeatureId id = createFeatureId(sector);
		return selection_featureId(action, id);
	}

	bool selection_guideline(SelectAction action, Guideline* guideline, u32 flags)
	{
		const FeatureId id = createFeatureId(nullptr, guideline->id);
		return selection_featureId(action, id);
	}

	bool selection_levelNote(SelectAction action, LevelNote* note, u32 flags)
	{
		const FeatureId id = createFeatureId(nullptr, note->id);
		return selection_featureId(action, id);
	}

	// Read interface.
	// Pass in SEL_GET_HOVERED for the index to get the hovered feature (or false is none).
	u32 selection_getCount(SelectionListId id)
	{
		return (u32)s_selectionList2[id >= SEL_CURRENT ? s_currentSelection : id].size();
	}
		
	bool selection_getVertex(s32 index, EditorSector*& sector, s32& featureIndex, bool* isOverlapped)
	{
		const FeatureId id = selection_getFeatureId(index, SEL_VERTEX);
		if (id == FEATUREID_NULL) { return false; }
		// Unpack the feature ID based on the type.
		sector = unpackFeatureId(id, &featureIndex, nullptr, isOverlapped);
		return true;
	}

	bool selection_getSurface(s32 index, EditorSector*& sector, s32& featureIndex, HitPart* part, bool* isOverlapped)
	{
		const FeatureId id = selection_getFeatureId(index, SEL_SURFACE);
		if (id == FEATUREID_NULL) { return false; }
		// Unpack the feature ID based on the type.
		sector = unpackFeatureId(id, &featureIndex, (s32*)part, isOverlapped);
		return true;
	}

	bool selection_getEntity(s32 index, EditorSector*& sector, s32& featureIndex)
	{
		const FeatureId id = selection_getFeatureId(index, SEL_ENTITY);
		if (id == FEATUREID_NULL) { return false; }
		// Unpack the feature ID based on the type.
		sector = unpackFeatureId(id, &featureIndex);
		return true;
	}

	bool selection_getSector(s32 index, EditorSector*& sector)
	{
		const FeatureId id = selection_getFeatureId(index, SEL_SECTOR);
		if (id == FEATUREID_NULL) { return false; }
		// Unpack the feature ID based on the type.
		sector = unpackFeatureId(id);
		return true;
	}

	bool selection_getGuideline(s32 index, Guideline*& guideline)
	{
		const FeatureId id = selection_getFeatureId(index, SEL_GUIDELINE);
		if (id == FEATUREID_NULL) { return false; }
		// Unpack the feature ID based on the type.
		s32 featureIndex = -1;
		unpackFeatureId(id, &featureIndex);
		if (featureIndex >= 0 && featureIndex < (s32)s_level.guidelines.size())
		{
			guideline = &s_level.guidelines[featureIndex];
			return true;
		}
		return false;
	}

	bool selection_getLevelNote(s32 index, LevelNote*& levelNote)
	{
		const FeatureId id = selection_getFeatureId(index, SEL_NOTE);
		if (id == FEATUREID_NULL) { return false; }
		// Unpack the feature ID based on the type.
		s32 featureIndex = -1;
		unpackFeatureId(id, &featureIndex);
		if (featureIndex >= 0 && featureIndex < (s32)s_level.notes.size())
		{
			levelNote = &s_level.notes[featureIndex];
			return true;
		}
		return false;
	}

	////////////////////////////////////////////////////////////
	// Internal
	////////////////////////////////////////////////////////////
	bool selection_insertVertex(FeatureId id, Vec2f value)
	{
		// Insert the vertex if not found.
		const s32 count = (s32)s_selectionList2[SEL_VERTEX].size();
		const FeatureId* featureId = s_selectionList2[SEL_VERTEX].data();
		s32 matchId = -1;
		for (s32 i = 0; i < count; i++)
		{
			if (featuresEqual(featureId[i], id)) { return false; }

			s32 index;
			EditorSector* sector = unpackFeatureId(featureId[i], &index);
			const Vec2f& srcValue = sector->vtx[index];

			if (TFE_Polygon::vtxEqual(&value, &srcValue))
			{
				// Keep looking after this, the feature may still be in here...
				matchId = i;
			}
		}

		id = setIsOverlapped(id, matchId >= 0);
		s_selectionList2[SEL_VERTEX].push_back(id);

		return true;
	}

	// Remove all vertices in the list at 'value'.
	bool selection_removeVertex(FeatureId id, Vec2f value)
	{
		const s32 count = (s32)s_selectionList2[SEL_VERTEX].size();
		const FeatureId* featureId = s_selectionList2[SEL_VERTEX].data();
		std::vector<s32> idxToRemove;
		for (s32 i = 0; i < count; i++)
		{
			if (featuresEqual(featureId[i], id))
			{
				idxToRemove.push_back(i);
			}
			else
			{
				s32 index;
				EditorSector* sector = unpackFeatureId(featureId[i], &index);
				const Vec2f& srcValue = sector->vtx[index];

				if (TFE_Polygon::vtxEqual(&value, &srcValue))
				{
					idxToRemove.push_back(i);
				}
			}
		}
		if (idxToRemove.empty()) { return false; }
		const s32 delCount = (s32)idxToRemove.size();
		const s32* idxList = idxToRemove.data();
		for (s32 i = delCount - 1; i >= 0; i--)
		{
			s_selectionList2[SEL_VERTEX].erase(s_selectionList2[SEL_VERTEX].begin() + idxList[i]);
		}
		return true;
	}

	bool selection_isVertexInSet(FeatureId id)
	{
		// Insert the vertex if not found.
		const s32 count = (s32)s_selectionList2[SEL_VERTEX].size();
		const FeatureId* featureId = s_selectionList2[SEL_VERTEX].data();
		for (s32 i = 0; i < count; i++)
		{
			if (featuresEqual(featureId[i], id)) { return true; }
		}
		return false;
	}
		
	bool selection_insertFeatureId(SelectionList& list, FeatureId id)
	{
		if (selection_isFeatureIdInSet(list, id)) { return false; }
		list.push_back(id);
		return true;
	}

	bool selection_removeFeatureId(SelectionList& list, FeatureId id)
	{
		s32 index;
		selection_isFeatureIdInSet(list, id, &index);
		if (index < 0) { return false; }

		list.erase(list.begin() + index);
		return true;
	}

	bool selection_isFeatureIdInSet(SelectionList& list, FeatureId id, s32* index)
	{
		const s32 count = (s32)list.size();
		const FeatureId* listId = list.data();
		if (index) { *index = -1; }
		for (s32 i = 0; i < count; i++)
		{
			if (featuresEqual(listId[i], id))
			{
				if (index) { *index = i; }
				return true;
			}
		}
		return false;
	}

	bool selection_featureId(SelectAction action, FeatureId id)
	{
		bool buildDerived = false;
		bool actionDone = false;
		switch (action)
		{
			case SA_SET:
			{
				s_selectionList2[SEL_SURFACE].clear();
				actionDone = selection_insertFeatureId(s_selectionList2[SEL_SURFACE], id);
				buildDerived = actionDone;
			} break;
			case SA_ADD:
			{
				actionDone = selection_insertFeatureId(s_selectionList2[SEL_SURFACE], id);
				buildDerived = actionDone;
			} break;
			case SA_REMOVE:
			{
				actionDone = selection_removeFeatureId(s_selectionList2[SEL_SURFACE], id);
				buildDerived = actionDone;
			} break;
			case SA_TOGGLE:
			{
				if (selection_isFeatureIdInSet(s_selectionList2[SEL_SURFACE], id))
				{
					actionDone = selection_removeFeatureId(s_selectionList2[SEL_SURFACE], id);
				}
				else
				{
					actionDone = selection_insertFeatureId(s_selectionList2[SEL_SURFACE], id);
				}
				buildDerived = actionDone;
			} break;
			case SA_CHECK_INCLUSION:
			{
				actionDone = selection_isFeatureIdInSet(s_selectionList2[SEL_SURFACE], id);
			} break;
			case SA_SET_HOVERED:
			{
				s_hovered = id;
				actionDone = true;
			} break;
		}

		if (buildDerived)
		{
			selection_buildDerived();
		}
		return actionDone;
	}

	FeatureId selection_getFeatureId(s32 index, SelectionListId listId)
	{
		FeatureId id = FEATUREID_NULL;
		if (index == SEL_INDEX_HOVERED && s_currentSelection == listId)
		{
			id = s_hovered;
		}
		else if (index >= 0 && index < (s32)s_selectionList2[listId].size())
		{
			id = s_selectionList2[listId][index];
		}
		return id;
	}

	void selection_insertWallVertices(const EditorSector* sector, const EditorWall* wall)
	{
		// Add front face.
		FeatureId v0 = createFeatureId(sector, wall->idx[0]);
		FeatureId v1 = createFeatureId(sector, wall->idx[1]);
		selection_insertVertex(v0, sector->vtx[wall->idx[0]]);
		selection_insertVertex(v1, sector->vtx[wall->idx[1]]);

		// Add mirror (if it exists).
		if (wall->adjoinId >= 0 && wall->mirrorId >= 0)
		{
			EditorSector* nextSector = &s_level.sectors[wall->adjoinId];
			EditorWall* mirror = wall->mirrorId < (s32)nextSector->walls.size() ? &nextSector->walls[wall->mirrorId] : nullptr;

			if (mirror)
			{
				FeatureId v2 = createFeatureId(nextSector, mirror->idx[0]);
				FeatureId v3 = createFeatureId(nextSector, mirror->idx[1]);
				selection_insertVertex(v2, nextSector->vtx[mirror->idx[0]]);
				selection_insertVertex(v3, nextSector->vtx[mirror->idx[1]]);
			}
		}
	}

	void selection_buildDerived()
	{
		if (s_currentSelection != SEL_VERTEX && s_currentSelection != SEL_SURFACE && s_currentSelection != SEL_SECTOR)
		{
			return;
		}
		if (s_currentSelection != SEL_VERTEX)
		{
			s_selectionList2[SEL_VERTEX].clear();
		}
		if (s_currentSelection != SEL_SURFACE)
		{
			s_selectionList2[SEL_SURFACE].clear();
		}
		if (s_currentSelection != SEL_SECTOR)
		{
			s_selectionList2[SEL_SECTOR].clear();
		}
		// There is no derived selection from vertices (for now).
		if (s_currentSelection == SEL_VERTEX)
		{
			// For now, vertices don't select anything.
			// This may change.
		}
		else if (s_currentSelection == SEL_SURFACE)
		{
			// Surfaces select vertices.
			const s32 count = (s32)s_selectionList2[SEL_SURFACE].size();
			const FeatureId* list = s_selectionList2[SEL_SURFACE].data();
			for (s32 i = 0; i < count; i++)
			{
				s32 featureIndex;
				HitPart part;
				EditorSector* sector = unpackFeatureId(list[i], &featureIndex, (s32*)&part);
				// Skip flats.
				if (part == HP_FLOOR || part == HP_CEIL) { continue; }

				EditorWall* wall = &sector->walls[featureIndex];
				selection_insertWallVertices(sector, wall);
			}

			// Surfaces may also select sectors, if floor and ceiling surfaces are selected.
			for (s32 i = 0; i < count; i++)
			{
				s32 featureIndex;
				HitPart part;
				EditorSector* sector = unpackFeatureId(list[i], &featureIndex, (s32*)&part);
				if (part != HP_FLOOR && part != HP_CEIL) { continue; }

				FeatureId id = createFeatureId(sector);
				selection_insertFeatureId(s_selectionList2[SEL_SECTOR], id);
			}
		}
		else if (s_currentSelection == SEL_SECTOR)
		{
			// Sectors select surfaces and vertices.
			const s32 count = (s32)s_selectionList2[SEL_SECTOR].size();
			const FeatureId* list = s_selectionList2[SEL_SECTOR].data();
			for (s32 i = 0; i < count; i++)
			{
				EditorSector* sector = unpackFeatureId(list[i]);
				if (!sector) { continue; }

				const s32 wallCount = (s32)sector->walls.size();
				const EditorWall* wall = sector->walls.data();
				for (s32 w = 0; w < wallCount; w++)
				{
					FeatureId id = createFeatureId(sector, w, HP_MID);
					selection_insertFeatureId(s_selectionList2[SEL_SURFACE], id);
					selection_insertWallVertices(sector, wall);
				}
			}
		}
	}
}

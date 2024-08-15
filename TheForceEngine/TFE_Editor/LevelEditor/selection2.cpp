#include "levelEditor.h"
#include "levelEditorData.h"
#include "selection2.h"
#include "sharedState.h"

using namespace TFE_Editor;

namespace LevelEditor
{
	SelectionList s_selectionList2[SEL_COUNT];
	FeatureId s_hovered = FEATUREID_NULL;
	SelectionListId s_currentSelection = SEL_VERTEX;

	bool selection_insertVertex(FeatureId id, Vec2f value);
	bool selection_removeVertex(FeatureId id, Vec2f value);
	bool selection_isVertexInSet(FeatureId id);
	void selection_buildDerived();

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
		return false;
	}

	bool selection_entity(SelectAction action, EditorSector* sector, s32 index, u32 flags)
	{
		return false;
	}

	bool selection_sector(SelectAction action, EditorSector* sector, u32 flags)
	{
		return false;
	}

	bool selection_guideline(SelectAction action, Guideline* guideline, u32 flags)
	{
		return false;
	}

	bool selection_levelNote(SelectAction action, LevelNote* note, u32 flags)
	{
		return false;
	}

	// Read interface.
	// Pass in SEL_GET_HOVERED for the index to get the hovered feature (or false is none).
	u32 selection_getCount(SelectionListId id)
	{
		return (u32)s_selectionList2[id >= SEL_CURRENT ? s_currentSelection : id].size();
	}

	bool selection_getVertex(s32 index, EditorSector*& sector, s32& featureIndex, bool* isOverlapped)
	{
		FeatureId id = FEATUREID_NULL;
		if (index == SEL_INDEX_HOVERED && s_currentSelection == SEL_VERTEX)
		{
			id = s_hovered;
		}
		else if (index >= 0 && index < (s32)s_selectionList2[SEL_VERTEX].size())
		{ 
			id = s_selectionList2[SEL_VERTEX][index];
		}
		if (id == FEATUREID_NULL) { return false; }
		// Unpack the feature ID based on the type.
		sector = unpackFeatureId(id, &featureIndex, nullptr, isOverlapped);
		return true;
	}

	// TODO: Hovered (see above).
	bool selection_getSurface(s32 index, EditorSector*& sector, s32& featureIndex, HitPart* part, bool* isOverlapped)
	{
		if (index < 0 || index >= (s32)s_selectionList2[SEL_SURFACE].size()) { return false; }

		const FeatureId id = s_selectionList2[SEL_SURFACE][index];
		if (id == FEATUREID_NULL) { return false; }

		sector = unpackFeatureId(id, &featureIndex, (s32*)part, isOverlapped);
		return true;
	}

	bool selection_getEntity(s32 index, EditorSector*& sector, s32& featureIndex)
	{
		if (index < 0 || index >= (s32)s_selectionList2[SEL_ENTITY].size()) { return false; }

		const FeatureId id = s_selectionList2[SEL_ENTITY][index];
		if (id == FEATUREID_NULL) { return false; }

		sector = unpackFeatureId(id, &featureIndex);
		return true;
	}

	bool selection_getSector(s32 index, EditorSector*& sector)
	{
		if (index < 0 || index >= (s32)s_selectionList2[SEL_SECTOR].size()) { return false; }

		const FeatureId id = s_selectionList2[SEL_SECTOR][index];
		if (id == FEATUREID_NULL) { return false; }

		sector = unpackFeatureId(id);
		return true;
	}

	bool selection_getGuideline(s32 index, Guideline*& guideline)
	{
		if (index < 0 || index >= (s32)s_selectionList2[SEL_GUIDELINE].size()) { return false; }

		const FeatureId id = s_selectionList2[SEL_GUIDELINE][index];
		if (id == FEATUREID_NULL) { return false; }

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
		if (index < 0 || index >= (s32)s_selectionList2[SEL_NOTE].size()) { return false; }

		const FeatureId id = s_selectionList2[SEL_NOTE][index];
		if (id == FEATUREID_NULL) { return false; }

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
		return false;
	}

	bool selection_removeVertex(FeatureId id, Vec2f value)
	{
		return false;
	}

	bool selection_isVertexInSet(FeatureId id)
	{
		return false;
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
			// Surfaces may also select sectors, if floor and ceiling surfaces are selected.
		}
		else if (s_currentSelection == SEL_SECTOR)
		{
			// Sectors select surfaces and vertices.
		}
	}
}

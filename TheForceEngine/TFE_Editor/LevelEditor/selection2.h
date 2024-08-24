#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "levelEditorData.h"
#include "featureId.h"
#include "dragSelect.h"
#include <vector>

namespace LevelEditor
{
	struct EditorSector;
	
	enum SelectionListId
	{
		SEL_VERTEX = 0,
		SEL_SURFACE,  // Walls in 2D; Walls, Floors and Ceilings in 3D.
		SEL_ENTITY,
		SEL_SECTOR,
		SEL_GUIDELINE,
		SEL_NOTE,
		SEL_COUNT,
		SEL_CURRENT = SEL_COUNT
	};
	enum SelectionListBit
	{
		SEL_VERTEX_BIT = FLAG_BIT(SEL_VERTEX),
		SEL_SURFACE_BIT = FLAG_BIT(SEL_SURFACE),
		SEL_ENTITY_BIT = FLAG_BIT(SEL_ENTITY),
		SEL_SECTOR_BIT = FLAG_BIT(SEL_SECTOR),
		SEL_GUIDELINE_BIT = FLAG_BIT(SEL_GUIDELINE),
		SEL_NOTE_BIT = FLAG_BIT(SEL_NOTE),
		SEL_NONE = 0,
		SEL_GEO = SEL_VERTEX_BIT | SEL_SURFACE_BIT | SEL_SECTOR_BIT,
		SEL_ALL = SEL_VERTEX_BIT | SEL_SURFACE_BIT | SEL_ENTITY_BIT | SEL_SECTOR_BIT | SEL_GUIDELINE_BIT | SEL_NOTE_BIT
	};

	enum SelectAction
	{
		SA_SET,     // Set as index 0
		SA_ADD,   // Add to the set, if not already contained.
		SA_REMOVE,  // Remove from the set if it is contained.
		SA_TOGGLE,   // Toggle
		SA_CHECK_INCLUSION,  // Return true if part of the set.
		SA_SET_HOVERED,  // Set as hovered if the type == current.
	};
	enum SelectFlags
	{
		SEL_FLAG_NONE = 0,
		SEL_FLAG_INCLUDE_OVERLAPPING = 1,
	};
	enum SelectIndex
	{
		SEL_INDEX_HOVERED = -1,
	};

	// General Interface
	void selection_clear(u32 selections = SEL_ALL, bool clearDragSelect = true); // see SelectionListBit
	void selection_setCurrent(SelectionListId id);
	void selection_clearHovered();
	bool selection_hasHovered();
	bool selection_hasSelection(SelectionListId id = SEL_CURRENT);

	// Selection interface.
	bool selection_vertex(SelectAction action, EditorSector* sector, s32 index, u32 flags = SEL_FLAG_INCLUDE_OVERLAPPING);
	bool selection_surface(SelectAction action, EditorSector* sector, s32 index, HitPart part = HP_MID, u32 flags = SEL_FLAG_INCLUDE_OVERLAPPING);
	bool selection_entity(SelectAction action, EditorSector* sector, s32 index, u32 flags = SEL_FLAG_NONE);
	bool selection_sector(SelectAction action, EditorSector* sector, u32 flags = SEL_FLAG_NONE);
	bool selection_guideline(SelectAction action, Guideline* guideline, u32 flags = SEL_FLAG_NONE);
	bool selection_levelNote(SelectAction action, LevelNote* note, u32 flags = SEL_FLAG_NONE);

	// Read interface.
	// Pass in SEL_GET_HOVERED for the index to get the hovered feature (or false if nothing hovered).
	u32  selection_getCount(SelectionListId id = SEL_CURRENT);
	bool selection_getVertex(s32 index, EditorSector*& sector, s32& featureIndex, bool* isOverlapped = 0);
	bool selection_getSurface(s32 index, EditorSector*& sector, s32& featureIndex, HitPart* part = nullptr, bool* isOverlapped = nullptr);
	bool selection_getEntity(s32 index, EditorSector*& sector, s32& featureIndex);
	bool selection_getSector(s32 index, EditorSector*& sector);
	bool selection_getGuideline(s32 index, Guideline*& guideline);
	bool selection_getLevelNote(s32 index, LevelNote*& levelNote);
}

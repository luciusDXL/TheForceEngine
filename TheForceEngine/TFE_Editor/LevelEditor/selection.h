#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <vector>

namespace LevelEditor
{
	struct EditorSector;

	// Feature ID
	typedef u64 FeatureId;
	typedef std::vector<FeatureId> SelectionList;
	typedef std::vector<EditorSector*> SectorList;

	extern SelectionList s_selectionList;
	extern SectorList s_sectorChangeList;

	FeatureId createFeatureId(EditorSector* sector, s32 featureIndex, s32 featureData = 0, bool isOverlapped = false);
	EditorSector* unpackFeatureId(FeatureId id, s32* featureIndex, s32* featureData, bool* isOverlapped);

	// Selection
	void selection_clear();
	bool selection_add(FeatureId id);
	void selection_remove(FeatureId id);
	void selection_toggle(FeatureId id);
	bool selection_doesFeatureExist(FeatureId id);
}

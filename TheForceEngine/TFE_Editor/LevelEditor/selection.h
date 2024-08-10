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
	
	typedef std::vector<FeatureId> SelectionList;

	extern SelectionList s_selectionList;
	extern SelectionList s_vertexList;
	extern SectorList s_sectorChangeList;
	
	// Selection
	void selection_clear(bool clearDragSelect = true);
	bool selection_add(FeatureId id);
	void selection_remove(FeatureId id);
	void selection_toggle(FeatureId id);
	bool selection_doesFeatureExist(FeatureId id);

	void vtxSelection_clear();
	bool vtxSelection_doesFeatureExist(FeatureId id);
	bool vtxSelection_add(FeatureId id);
	void vtxSelection_remove(FeatureId id);
}

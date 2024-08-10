#include "levelEditor.h"
#include "levelEditorData.h"
#include "selection.h"
#include "sharedState.h"

using namespace TFE_Editor;

namespace LevelEditor
{
	SelectionList s_selectionList;
	SelectionList s_vertexList;
	SectorList s_sectorChangeList;
	
	// Selection
	void selection_clear(bool clearDragSelect)
	{
		s_selectionList.clear();
		if (clearDragSelect) { s_dragSelect.active = false; }
	}
		
	bool selection_doesFeatureExist(FeatureId id)
	{
		const u64 compareValue = u64(id) & FID_COMPARE_MASK;
		const size_t count = s_selectionList.size();
		const FeatureId* feature = s_selectionList.data();
		for (size_t i = 0; i < count; i++, feature++)
		{
			if ((u64(*feature) & FID_COMPARE_MASK) == compareValue)
			{
				return true;
			}
		}
		return false;
	}

	bool selection_add(FeatureId id)
	{
		if (selection_doesFeatureExist(id)) { return false; }
		s_selectionList.push_back(id);
		return true;
	}

	void selection_remove(FeatureId id)
	{
		const u64 compareValue = u64(id) & FID_COMPARE_MASK;
		const size_t count = s_selectionList.size();
		const FeatureId* feature = s_selectionList.data();
		for (size_t i = 0; i < count; i++, feature++)
		{
			if ((u64(*feature) & FID_COMPARE_MASK) == compareValue)
			{
				s_selectionList.erase(s_selectionList.begin() + i);
				return;
			}
		}
	}

	void selection_toggle(FeatureId id)
	{
		if (selection_doesFeatureExist(id))
		{ 
			selection_remove(id);
			return;
		}
		selection_add(id);
	}
		
	// Vertex Selection
	void vtxSelection_clear()
	{
		s_vertexList.clear();
	}

	bool vtxSelection_doesFeatureExist(FeatureId id)
	{
		const u64 compareValue = u64(id) & FID_COMPARE_MASK;
		const size_t count = s_vertexList.size();
		const FeatureId* feature = s_vertexList.data();
		for (size_t i = 0; i < count; i++, feature++)
		{
			if ((u64(*feature) & FID_COMPARE_MASK) == compareValue)
			{
				return true;
			}
		}
		return false;
	}

	bool vtxSelection_add(FeatureId id)
	{
		if (vtxSelection_doesFeatureExist(id)) { return false; }
		s_vertexList.push_back(id);
		return true;
	}

	void vtxSelection_remove(FeatureId id)
	{
		const u64 compareValue = u64(id) & FID_COMPARE_MASK;
		const size_t count = s_vertexList.size();
		const FeatureId* feature = s_selectionList.data();
		for (size_t i = 0; i < count; i++, feature++)
		{
			if ((u64(*feature) & FID_COMPARE_MASK) == compareValue)
			{
				s_vertexList.erase(s_vertexList.begin() + i);
				return;
			}
		}
	}
}

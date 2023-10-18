#include "levelEditor.h"
#include "levelEditorData.h"
#include "selection.h"
#include "camera.h"
#include "sharedState.h"
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/editorLevel.h>
#include <TFE_Editor/editorProject.h>
#include <TFE_Editor/editorResources.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_Editor/LevelEditor/Rendering/grid2d.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderShared/lineDraw2d.h>
#include <TFE_RenderShared/lineDraw3d.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Polygon/polygon.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	SelectionList s_selectionList;
	SectorList s_sectorChangeList;
	DragSelect s_dragSelect = { 0 };

	enum FeatureIdPacking : u64
	{
		FID_SECTOR = 0ull,
		FID_FEATURE_INDEX = 32ull,
		FID_FEATURE_DATA  = 55ull,
		FID_OVERLAPPED    = 63ull,

		FID_SECTOR_MASK        = (1ull << 32ull) - 1ull,
		FID_FEATURE_INDEX_MASK = (1ull << 23ull) - 1ull,
		FID_FEATURE_DATA_MASK  = (1ull << 8ull) - 1ull,
		FID_COMPARE_MASK = (1ull << 55ull) - 1ull,
	};

	FeatureId createFeatureId(EditorSector* sector, s32 featureIndex, s32 featureData, bool isOverlapped)
	{
		return FeatureId(u64(sector->id) |
			            (u64(featureIndex) << FID_FEATURE_INDEX) |
			            (u64(featureData) << FID_FEATURE_DATA) |
			            (u64(isOverlapped ? 1 : 0) << FID_OVERLAPPED));
	}

	EditorSector* unpackFeatureId(FeatureId id, s32* featureIndex, s32* featureData, bool* isOverlapped)
	{
		u32 sectorId  = u32(id & FID_SECTOR_MASK);
		*featureIndex = s32((id >> FID_FEATURE_INDEX) & FID_FEATURE_INDEX_MASK);
		*featureData  = s32((id >> FID_FEATURE_DATA) & FID_FEATURE_DATA_MASK);
		*isOverlapped = (id & (1ull << FID_OVERLAPPED)) != 0;

		return &s_level.sectors[sectorId];
	}

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
}

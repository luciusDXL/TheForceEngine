#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>

namespace AssetBrowser
{
	void init();
	void destroy();

	void update();
	void render();

	void selectAll();
	void selectNone();
	void invertSelection();

	bool showOnlyModLevels();
	void rebuildAssets();

	TFE_Editor::Asset* findAsset(const char* name, TFE_Editor::AssetType type);
	TFE_Editor::AssetHandle loadAssetData(const TFE_Editor::Asset* asset);
}

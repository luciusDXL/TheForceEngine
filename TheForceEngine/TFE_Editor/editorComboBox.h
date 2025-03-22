#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <string>

namespace TFE_Editor
{
	struct ItemInfo
	{
		std::string name;	// Name of the item.
		s32 id;				// ID.
	};

	void editor_comboBoxInit();
	bool editor_itemEditComboBox(s32 id, char* inputBuffer, size_t inputBufferSize, s32 listCount, const ItemInfo* itemList);
	bool editor_assetEditComboBox(s32 id, char* inputBuffer, size_t inputBufferSize, s32 listCount, const Asset* assetList);
	void editor_handleOverlayList();

	bool editor_beginList();
	void editor_endList();
}

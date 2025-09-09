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
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>

namespace LevelEditor
{
	enum LeMsgType
	{
		LE_MSG_INFO = 0,
		LE_MSG_WARNING,
		LE_MSG_ERROR,
		LE_MSG_SCRIPT,
		LE_MSG_COUNT
	};
	enum LeMsgFilter
	{
		LFILTER_INFO    = (1 << LE_MSG_INFO),
		LFILTER_WARNING = (1 << LE_MSG_WARNING),
		LFILTER_ERROR   = (1 << LE_MSG_ERROR),
		LFILTER_SCRIPT  = (1 << LE_MSG_SCRIPT),
		LFILTER_DEFAULT = LFILTER_INFO | LFILTER_WARNING | LFILTER_ERROR
	};

	void infoToolBegin(s32 height);
	bool drawInfoPanel(EditorView view);
	void infoToolEnd();

	bool categoryPopupUI();

	// Output messages
	void infoPanelClearMessages();
	void infoPanelAddMsg(LeMsgType type, const char* msg, ...);
	void infoPanelSetMsgFilter(u32 filter=LFILTER_DEFAULT);
	s32 infoPanelOutput(s32 width);

	void infoPanelClearFeatures();
	s32 infoPanelGetHeight();

	void clearEntityChanges();
	void commitCurEntityChanges();

	void selectAndScrollToSector(EditorSector* sector);
	void scrollToSector(EditorSector* sector);

	void infoPanel_clearSelection();

	void infoPanelOpenGroup(s32 groupId);
}

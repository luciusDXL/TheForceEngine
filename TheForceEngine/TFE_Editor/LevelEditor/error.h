#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>

#define LE_ERROR(...) LevelEditor::infoPanelAddMsg(LevelEditor::LE_MSG_ERROR, __VA_ARGS__)
#define LE_WARNING(...) LevelEditor::infoPanelAddMsg(LevelEditor::LE_MSG_WARNING, __VA_ARGS__)
#define LE_INFO(...) LevelEditor::infoPanelAddMsg(LevelEditor::LE_MSG_INFO, __VA_ARGS__)

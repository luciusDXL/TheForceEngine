#pragma once
#include <TFE_System/types.h>
#include <TFE_Editor/history.h>
#include <TFE_Editor/LevelEditor/selection.h>

namespace LevelEditor
{
	void levHistory_init();
	void levHistory_destroy();

	void levHistory_clear();
	void levHistory_createSnapshot(const char* name);

	void levHistory_undo();
	void levHistory_redo();

	// Commands
	void cmd_addMoveVertices(s32 count, const FeatureId* vertices, Vec2f delta);
	void cmd_addSetVertex(FeatureId vertex, Vec2f pos);
}

#include "levelEditorHistory.h"
#include "sharedState.h"
#include <TFE_Editor/LevelEditor/levelEditor.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>
#include <assert.h>
#include <algorithm>

using namespace TFE_Editor;

namespace LevelEditor
{
	enum LevCommand
	{
		LCmd_MoveVertex = CMD_START,
		LCmd_SetVertex,
		LCmd_Count
	};

	enum LevCommandName
	{
		LName_MoveVertex = 0,
		LName_SetVertex,
		LName_Count
	};

	// Command Functions
	void cmd_applyMoveVertices();
	void cmd_applySetVertex();

	void levHistory_init()
	{
		history_init(level_unpackSnapshot, level_createSnapshot);

		history_registerCommand(LCmd_MoveVertex, cmd_applyMoveVertices);
		history_registerCommand(LCmd_SetVertex, cmd_applySetVertex);
		history_registerName(LName_MoveVertex, "Move Vertices");
		history_registerName(LName_SetVertex, "Set Vertex Position");
	}

	void levHistory_destroy()
	{
		history_destroy();
	}

	void levHistory_clear()
	{
		history_clear();
	}

	void levHistory_createSnapshot(const char* name)
	{
		history_createSnapshot(name);
	}
		
	void levHistory_undo()
	{
		// TODO: Figure out how to track sector changes properly.
		history_step(-1);
	}

	void levHistory_redo()
	{
		// TODO: Figure out how to track sector changes properly.
		history_step(1);
	}

	////////////////////////////////
	// Commands
	////////////////////////////////
	// cmd_add*   - called from the level editor to add the command to the history.
	// cmd_apply* - called from the history/undo/redo system to apply the command from the history.
	
	/////////////////////////////////
	// Move Vertices
	void cmd_addMoveVertices(s32 count, const FeatureId* vertices, Vec2f delta)
	{
		// Try to create a command, if it returns false - then a snapshot was created
		// instead.
		if (!history_createCommand(LCmd_MoveVertex, LName_MoveVertex)) { return; }
		// Add the command data.
		hBuffer_addS32(count);
		hBuffer_addArrayU64(count, vertices);
		hBuffer_addVec2f(delta);
	}

	void cmd_applyMoveVertices()
	{
		// Extract the command data.
		const s32 count = hBuffer_getS32();
		const FeatureId* vertices = (FeatureId*)hBuffer_getArrayU64(count);
		const Vec2f delta = hBuffer_getVec2f();
		// Call the editor command.
		edit_moveVertices(count, vertices, delta);
	}

	/////////////////////////////////
	// Set Vertex
	void cmd_addSetVertex(FeatureId vertex, Vec2f pos)
	{
		// Try to create a command, if it returns false - then a snapshot was created
		// instead.
		if (!history_createCommand(LCmd_SetVertex, LName_SetVertex)) { return; }
		// Add the command data.
		hBuffer_addU64(vertex);
		hBuffer_addVec2f(pos);
	}

	void cmd_applySetVertex()
	{
		// Extract the command data.
		const FeatureId id = (FeatureId)hBuffer_getU64();
		const Vec2f pos = hBuffer_getVec2f();
		// Call the editor command.
		edit_setVertexPos(id, pos);
	}
}

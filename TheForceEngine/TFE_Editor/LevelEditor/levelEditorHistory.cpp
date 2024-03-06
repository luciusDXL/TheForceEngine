#include "levelEditorHistory.h"
#include "sharedState.h"
#include <TFE_Editor/LevelEditor/levelEditor.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>
#include <TFE_System/system.h>
#include <assert.h>
#include <algorithm>
#include <cstring>

using namespace TFE_Editor;

// TODO: Capture edit mode and selection data when creating a delta/snapshot.

namespace LevelEditor
{
	enum LevCommand
	{
		LCmd_MoveVertex = CMD_START,
		LCmd_SetVertex,
		LCmd_MoveFlat,
		LCmd_InsertVertex,
		LCmd_DeleteVertex,
		LCmd_DeleteSector,
		LCmd_CreateSectorFromRect,
		LCmd_CreateSectorFromShape,
		LCmd_MoveTexture,
		LCmd_SetTexture,
		LCmd_CopyTexture,
		LCmd_ClearTexture,
		LCmd_Autoalign,
		LCmd_Count
	};

	// Used to merge certain commands.
	static f64 c_mergeThreshold = 1.0;
	static f64 s_lastMoveTex = 0.0;
				
	// Command Functions
	void cmd_applyMoveVertices();
	void cmd_applySetVertex();
	void cmd_applyMoveFlats();
	void cmd_applyInsertVertex();
	void cmd_applyDeleteVertex();
	void cmd_applyDeleteSector();
	void cmd_applyCreateSectorFromRect();
	void cmd_applyCreateSectorFromShape();
	void cmd_applyMoveTexture();
	void cmd_applySetTexture();
	void cmd_applySetTextureWithOffset();
	void cmd_applySetTexture();
	void cmd_applyClearTexture();
	void cmd_applyAutoalign();

	// Command Merging Helpers
	bool mergeMoveTextureCmd(s32 count, FeatureId* features, Vec2f delta);

	///////////////////////////////////
	// API
	///////////////////////////////////
	void levHistory_init()
	{
		history_init(level_unpackSnapshot, level_createSnapshot);

		history_registerCommand(LCmd_MoveVertex, cmd_applyMoveVertices);
		history_registerCommand(LCmd_SetVertex, cmd_applySetVertex);
		history_registerCommand(LCmd_MoveFlat, cmd_applyMoveFlats);
		history_registerCommand(LCmd_InsertVertex, cmd_applyInsertVertex);
		history_registerCommand(LCmd_DeleteVertex, cmd_applyDeleteVertex);
		history_registerCommand(LCmd_DeleteSector, cmd_applyDeleteSector);
		history_registerCommand(LCmd_CreateSectorFromRect, cmd_applyCreateSectorFromRect);
		history_registerCommand(LCmd_CreateSectorFromShape, cmd_applyCreateSectorFromShape);
		history_registerCommand(LCmd_MoveTexture, cmd_applyMoveTexture);
		history_registerCommand(LCmd_SetTexture, cmd_applySetTexture);
		history_registerCommand(LCmd_CopyTexture, cmd_applySetTextureWithOffset);
		history_registerCommand(LCmd_ClearTexture, cmd_applyClearTexture);
		history_registerCommand(LCmd_Autoalign, cmd_applyAutoalign);
		history_registerName(LName_MoveVertex, "Move Vertice(s)");
		history_registerName(LName_SetVertex, "Set Vertex Position");
		history_registerName(LName_MoveWall, "Move Wall(s)");
		history_registerName(LName_MoveFlat, "Move Flat(s)");
		history_registerName(LName_InsertVertex, "Insert Vertex");
		history_registerName(LName_DeleteVertex, "Delete Vertex");
		history_registerName(LName_DeleteWall, "Delete Wall");
		history_registerName(LName_DeleteSector, "Delete Sector");
		history_registerName(LName_CreateSectorFromRect, "Create Sector (Rect)");
		history_registerName(LName_CreateSectorFromShape, "Create Sector (Shape)");
		history_registerName(LName_MoveTexture, "Move Texture");
		history_registerName(LName_SetTexture, "Set Texture");
		history_registerName(LName_CopyTexture, "Copy Texture");
		history_registerName(LName_ClearTexture, "Clear Texture");
		history_registerName(LName_Autoalign, "Autoalign Textures");

		s_lastMoveTex = 0.0;
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
		history_step(-1);
	}

	void levHistory_redo()
	{
		history_step(1);
	}
		
	////////////////////////////////
	// Command Helpers
	////////////////////////////////
	// Requires either 4 or 8 bytes.
	void captureFeature(Feature feature)
	{
		hBuffer_addS16((s16)feature.featureIndex);
		hBuffer_addS16((s16)feature.part);
		// Save 4 bytes if there is no feature.
		if (feature.featureIndex >= 0)
		{
			hBuffer_addS32(feature.sector ? feature.sector->id : -1);
		}
		// Previous sector is not captured since it is optional.
	}

	void restoreFeature(Feature& feature)
	{
		// Previous sector is not captured, so clear.
		// This is only a convenience for 2D editing and is not required.

		feature.featureIndex = (s32)hBuffer_getS16();
		feature.part = (HitPart)hBuffer_getS16();
		feature.sector = nullptr;
		feature.prevSector = nullptr;
		if (feature.featureIndex >= 0)
		{
			s32 sectorId = hBuffer_getS32();
			feature.sector = sectorId < 0 ? nullptr : &s_level.sectors[sectorId];
		}
	}
		
	// First number is single instance, second is 65536 states with the same settings.
	// Min (no features): 16 bytes, 1.0Mb
	//        (features): 24 bytes, 1.5Mb
	// Assuming 10 items selected: 104 bytes, 6.5Mb
	// Most common case: 24 bytes + 1 selected = 32 bytes (2.0Mb @ 64k).
	void captureEditState()
	{
		hBuffer_addU32((u32)s_editMode);
		hBuffer_addU32((u32)s_selectionList.size());
		if (!s_selectionList.empty())
		{
			hBuffer_addArrayU64((u32)s_selectionList.size(), s_selectionList.data());
		}
		captureFeature(s_featureHovered);
		captureFeature(s_featureCur);
	}

	void restoreEditState()
	{
		s_editMode = (LevelEditMode)hBuffer_getU32();
		u32 selectionCount = hBuffer_getU32();
		s_selectionList.resize(selectionCount);
		if (selectionCount)
		{
			memcpy(s_selectionList.data(), hBuffer_getArrayU64(selectionCount), sizeof(u64) * selectionCount);
		}
		restoreFeature(s_featureHovered);
		restoreFeature(s_featureCur);
		// Clear the current wall, it will be reset when needed automatically.
		s_featureCurWall = {};
	}

	#define CMD_APPLY_BEGIN()
	#define CMD_APPLY_END() restoreEditState()

	#define CMD_BEGIN(cmdId, cmdName) if (!history_createCommand(cmdId, cmdName)) { return; }
	#define CMD_END() captureEditState()

	////////////////////////////////
	// Commands
	////////////////////////////////
	// cmd_add*   - called from the level editor to add the command to the history.
	// cmd_apply* - called from the history/undo/redo system to apply the command from the history.
	
	/////////////////////////////////
	// Move Vertices
	void cmd_addMoveVertices(s32 count, const FeatureId* vertices, Vec2f delta, LevCommandName name)
	{
		CMD_BEGIN(LCmd_MoveVertex, name);
		// Add the command data.
		hBuffer_addS32(count);
		hBuffer_addArrayU64(count, vertices);
		hBuffer_addVec2f(delta);
		CMD_END();
	}

	void cmd_applyMoveVertices()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		const s32 count = hBuffer_getS32();
		const FeatureId* vertices = (FeatureId*)hBuffer_getArrayU64(count);
		const Vec2f delta = hBuffer_getVec2f();
		// Call the editor command.
		edit_moveVertices(count, vertices, delta);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Set Vertex
	void cmd_addSetVertex(FeatureId vertex, Vec2f pos)
	{
		CMD_BEGIN(LCmd_SetVertex, LName_SetVertex);
		// Add the command data.
		hBuffer_addU64(vertex);
		hBuffer_addVec2f(pos);
		CMD_END();
	}

	void cmd_applySetVertex()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		const FeatureId id = (FeatureId)hBuffer_getU64();
		const Vec2f pos = hBuffer_getVec2f();
		// Call the editor command.
		edit_setVertexPos(id, pos);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Move Flats
	void cmd_addMoveFlats(s32 count, const FeatureId* flats, f32 delta)
	{
		CMD_BEGIN(LCmd_MoveFlat, LName_MoveFlat);
		// Add the command data.
		hBuffer_addS32(count);
		hBuffer_addArrayU64(count, flats);
		hBuffer_addF32(delta);
		CMD_END();
	}

	void cmd_applyMoveFlats()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		const s32 count = hBuffer_getS32();
		const FeatureId* flats = (FeatureId*)hBuffer_getArrayU64(count);
		const f32 delta = hBuffer_getF32();
		// Call the editor command.
		edit_moveFlats(count, flats, delta);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Insert Vertex
	void cmd_addInsertVertex(s32 sectorIndex, s32 wallIndex, Vec2f newPos)
	{
		CMD_BEGIN(LCmd_InsertVertex, LName_InsertVertex);
		// Add the command data.
		hBuffer_addS32(sectorIndex);
		hBuffer_addS32(wallIndex);
		hBuffer_addVec2f(newPos);
		CMD_END();
	}

	void cmd_applyInsertVertex()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		const s32 sectorIndex = hBuffer_getS32();
		const s32 wallIndex = hBuffer_getS32();
		const Vec2f newPos = hBuffer_getVec2f();
		// Call the editor command.
		edit_splitWall(sectorIndex, wallIndex, newPos);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Delete Vertex
	void cmd_addDeleteVertex(s32 sectorIndex, s32 vertexIndex, LevCommandName name)
	{
		CMD_BEGIN(LCmd_DeleteVertex, name);
		// Add the command data.
		hBuffer_addS32(sectorIndex);
		hBuffer_addS32(vertexIndex);
		CMD_END();
	}

	void cmd_applyDeleteVertex()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		const s32 sectorIndex = hBuffer_getS32();
		const s32 vertexIndex = hBuffer_getS32();
		// Call the editor command.
		edit_deleteVertex(sectorIndex, vertexIndex);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Delete Sector
	void cmd_addDeleteSector(s32 sectorIndex)
	{
		CMD_BEGIN(LCmd_DeleteSector, LName_DeleteSector);
		// Add the command data.
		hBuffer_addS32(sectorIndex);
		CMD_END();
	}

	void cmd_applyDeleteSector()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		const s32 sectorIndex = hBuffer_getS32();
		// Call the editor command.
		edit_deleteSector(sectorIndex);
		CMD_APPLY_END();
	}
		
	/////////////////////////////////
	// Create Sector From Rect
	void cmd_addCreateSectorFromRect(const f32* heights, const Vec2f* corners)
	{
		CMD_BEGIN(LCmd_CreateSectorFromRect, LName_CreateSectorFromRect);
		// Add the command data.
		hBuffer_addArrayF32(2, heights);
		hBuffer_addArrayVec2f(2, corners);
		CMD_END();
	}

	void cmd_applyCreateSectorFromRect()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		const f32* heights   = hBuffer_getArrayF32(2);
		const Vec2f* corners = hBuffer_getArrayVec2f(2);
		// Call the editor command.
		edit_createSectorFromRect(heights, corners);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Create Sector From Shape
	void cmd_addCreateSectorFromShape(const f32* heights, s32 vertexCount, const Vec2f* vtx)
	{
		CMD_BEGIN(LCmd_CreateSectorFromShape, LName_CreateSectorFromShape);
		// Add the command data.
		hBuffer_addS32(vertexCount);
		hBuffer_addArrayF32(2, heights);
		hBuffer_addArrayVec2f(vertexCount, vtx);
		CMD_END();
	}

	void cmd_applyCreateSectorFromShape()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		s32 vertexCount = hBuffer_getS32();
		const f32* heights = hBuffer_getArrayF32(2);
		const Vec2f* vtx = hBuffer_getArrayVec2f(vertexCount);
		// Call the editor command.
		edit_createSectorFromShape(heights, vertexCount, vtx);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Texture Offset
	void cmd_addMoveTexture(s32 count, FeatureId* features, Vec2f delta)
	{
		// Special: try to merge the command to avoid too many tiny movements.
		if (mergeMoveTextureCmd(count, features, delta)) { return; }

		CMD_BEGIN(LCmd_MoveTexture, LName_MoveTexture);
		// Add the command data.
		hBuffer_addS32(count);
		hBuffer_addArrayU64(count, features);
		hBuffer_addVec2f(delta);
		CMD_END();
	}

	void cmd_applyMoveTexture()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		s32 count = hBuffer_getS32();
		FeatureId* features = (FeatureId*)hBuffer_getArrayU64(count);
		Vec2f delta = hBuffer_getVec2f();
		// Call the editor command.
		edit_moveTexture(count, features, delta);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Texture Offset
	void cmd_addSetTexture(s32 count, FeatureId* features, s32 texId, Vec2f* offset)
	{
		CMD_BEGIN(offset ? LCmd_CopyTexture : LCmd_SetTexture, offset ? LName_CopyTexture : LName_SetTexture);
		// Add the command data.
		hBuffer_addS32(count);
		hBuffer_addS32(texId);
		hBuffer_addArrayU64(count, features);
		if (offset)
		{
			hBuffer_addVec2f(*offset);
		}
		CMD_END();
	}

	void cmd_applySetTexture()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		s32 count = hBuffer_getS32();
		s32 texId = hBuffer_getS32();
		FeatureId* features = (FeatureId*)hBuffer_getArrayU64(count);
		// Call the editor command.
		edit_setTexture(count, features, texId);
		CMD_APPLY_END();
	}

	void cmd_applySetTextureWithOffset()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		s32 count = hBuffer_getS32();
		s32 texId = hBuffer_getS32();
		FeatureId* features = (FeatureId*)hBuffer_getArrayU64(count);
		Vec2f offset = hBuffer_getVec2f();
		// Call the editor command.
		edit_setTexture(count, features, texId, &offset);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Clear Texture
	void cmd_addClearTexture(s32 count, FeatureId* features)
	{
		CMD_BEGIN(LCmd_ClearTexture, LName_ClearTexture);
		// Add the command data.
		hBuffer_addS32(count);
		hBuffer_addArrayU64(count, features);
		CMD_END();
	}

	void cmd_applyClearTexture()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		s32 count = hBuffer_getS32();
		FeatureId* features = (FeatureId*)hBuffer_getArrayU64(count);
		// Call the editor command.
		edit_clearTexture(count, features);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Auto Align
	void cmd_addAutoAlign(s32 sectorId, s32 featureIndex, HitPart part)
	{
		CMD_BEGIN(LCmd_Autoalign, LName_Autoalign);
		// Add the command data.
		hBuffer_addS32(sectorId);
		hBuffer_addS32(featureIndex);
		hBuffer_addS32((s32)part);
		CMD_END();
	}

	void cmd_applyAutoalign()
	{
		CMD_APPLY_BEGIN();
		// Extract the command data.
		s32 sectorId = hBuffer_getS32();
		s32 featureIndex = hBuffer_getS32();
		HitPart part = (HitPart)hBuffer_getS32();
		// Call the editor command.
		edit_autoAlign(sectorId, featureIndex, part);
		CMD_APPLY_END();
	}

	/////////////////////////////////
	// Command Merging
	/////////////////////////////////
	bool mergeMoveTextureCmd(s32 count, FeatureId* features, Vec2f delta)
	{
		bool canMerge = false;
		f64 curTime = TFE_System::getTime();
		if (curTime - s_lastMoveTex < c_mergeThreshold)
		{
			// Add the command data.
			u32 testData[3] = { (u32)count, u32(features[0]), u32(features[0] >> u64(32)) };
			canMerge = history_canMergeCommand(LCmd_MoveTexture, LName_MoveTexture, testData, sizeof(u32) * 3);

			if (canMerge)
			{
				s32 dataOffset = sizeof(s32)/*count*/ + sizeof(u64)*count/*feature array*/;
				Vec2f* prevData = (Vec2f*)history_getPrevCmdBufferData(dataOffset);
				prevData->x += delta.x;
				prevData->z += delta.z;
			}
		}
		s_lastMoveTex = curTime;
		return canMerge;
	}
}

#include "levelEditorHistory.h"
#include "sharedState.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/levelEditor.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>
#include <TFE_Archive/zstdCompression.h>
#include <TFE_System/system.h>
#include <assert.h>
#include <algorithm>
#include <cstring>
#include <cstddef>

using namespace TFE_Editor;

// TODO: Capture edit mode and selection data when creating a delta/snapshot.

namespace LevelEditor
{
	#define CMD_BEGIN(cmdId, cmdName) if (!history_createCommand(cmdId, cmdName)) { return; }
	#define CMD_END()

	enum LevCommand
	{
		LCmd_Sector_Snapshot = CMD_START,
		LCmd_Sector_Wall_Snapshot,
		LCmd_Sector_Attrib_Snapshot,
		LCmd_ObjList_Snapshot,
		LCmd_Count
	};

	// Used to merge certain commands.
	static TFE_Editor::SnapshotBuffer s_workBuffer[2];
				
	// Command Functions
	void cmd_applySectorSnapshot();
	void cmd_applySectorWallSnapshot();
	void cmd_applySectorAttribSnapshot();
	void cmd_applyObjListSnapshot();

	///////////////////////////////////
	// API
	///////////////////////////////////
	void levHistory_init()
	{
		history_init(level_unpackSnapshot, level_createSnapshot);

		history_registerCommand(LCmd_Sector_Snapshot, cmd_applySectorSnapshot);
		history_registerCommand(LCmd_Sector_Wall_Snapshot, cmd_applySectorWallSnapshot);
		history_registerCommand(LCmd_Sector_Attrib_Snapshot, cmd_applySectorAttribSnapshot);
		history_registerCommand(LCmd_ObjList_Snapshot, cmd_applyObjListSnapshot);

		history_registerName(LName_MoveVertex, "Move Vertice(s)");
		history_registerName(LName_SetVertex, "Set Vertex Position");
		history_registerName(LName_MoveWall, "Move Wall(s)");
		history_registerName(LName_MoveFlat, "Move Flat(s)");
		history_registerName(LName_MoveSector, "Move Sector(s)");
		history_registerName(LName_MoveObject, "Move Objects(s)");
		history_registerName(LName_MoveObjectToFloor, "Move Object(s) to Floor");
		history_registerName(LName_MoveObjectToCeil, "Move Object(s) to Ceiling");
		history_registerName(LName_InsertVertex, "Insert Vertex");
		history_registerName(LName_DeleteVertex, "Delete Vertex");
		history_registerName(LName_DeleteWall, "Delete Wall");
		history_registerName(LName_DeleteSector, "Delete Sector");
		history_registerName(LName_CreateSectorFromRect, "Create Sector (Rect)");
		history_registerName(LName_CreateSectorFromShape, "Create Sector (Shape)");
		history_registerName(LName_ExtrudeSectorFromWall, "Extrude Sectors from Wall");
		history_registerName(LName_MoveTexture, "Move Texture");
		history_registerName(LName_SetTexture, "Set Texture");
		history_registerName(LName_CopyTexture, "Copy Texture");
		history_registerName(LName_ClearTexture, "Clear Texture");
		history_registerName(LName_Autoalign, "Autoalign Textures");
		history_registerName(LName_DeleteObject, "Delete Object(s)");
		history_registerName(LName_AddObject, "Added Object(s)");
		history_registerName(LName_RotateSector, "Rotate Sector(s)");
		history_registerName(LName_RotateWall, "Rotate Wall(s)");
		history_registerName(LName_RotateVertex, "Rotate Vertices(s)");
		history_registerName(LName_ChangeWallAttrib, "Change Wall Attributes");
		history_registerName(LName_ChangeSectorAttrib, "Change Sector Attributes");
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
		edit_clearSelections(false);
	}

	void levHistory_redo()
	{
		history_step(1);
		edit_clearSelections(false);
	}

	////////////////////////////////
	// Editor API
	////////////////////////////////
	void cmd_sectorSnapshot(u32 name, std::vector<s32>& sectorIds)
	{
		if (sectorIds.empty()) { return; }

		s_workBuffer[0].clear();
		s_workBuffer[1].clear();
		level_createSectorSnapshot(&s_workBuffer[0], sectorIds);
		if (s_workBuffer[0].empty()) { return; }

		const u32 uncompressedSize = (u32)s_workBuffer[0].size();
		u32 compressedSize = 0;
		if (zstd_compress(s_workBuffer[1], s_workBuffer[0].data(), uncompressedSize, 4))
		{
			compressedSize = (u32)s_workBuffer[1].size();
		}
		// ERROR
		if (!compressedSize)
		{
			return;
		}

		CMD_BEGIN(LCmd_Sector_Snapshot, name);
		{
			hBuffer_addU32(uncompressedSize);
			hBuffer_addU32(compressedSize);
			hBuffer_addArrayU8(compressedSize, s_workBuffer[1].data());
		}
		CMD_END();
	}

	void cmd_objectListSnapshot(u32 name, s32 sectorId)
	{
		s_workBuffer[0].clear();
		s_workBuffer[1].clear();
		level_createEntiyListSnapshot(&s_workBuffer[0], sectorId);
		if (s_workBuffer[0].empty()) { return; }

		const u32 uncompressedSize = (u32)s_workBuffer[0].size();
		u32 compressedSize = 0;
		if (zstd_compress(s_workBuffer[1], s_workBuffer[0].data(), uncompressedSize, 4))
		{
			compressedSize = (u32)s_workBuffer[1].size();
		}
		// ERROR
		if (!compressedSize)
		{
			return;
		}

		CMD_BEGIN(LCmd_ObjList_Snapshot, name);
		{
			hBuffer_addU32(uncompressedSize);
			hBuffer_addU32(compressedSize);
			hBuffer_addArrayU8(compressedSize, s_workBuffer[1].data());
		}
		CMD_END();
	}

	void cmd_sectorWallSnapshot(u32 name, std::vector<IndexPair>& sectorWallIds, bool idsChanged)
	{
		if (sectorWallIds.empty()) { return; }

		u16 prevCmd, prevName;
		history_getPrevCmdAndName(prevCmd, prevName);
		if (prevCmd == LCmd_Sector_Wall_Snapshot && prevName == name && !idsChanged)
		{
			history_removeLast();
		}

		s_workBuffer[0].clear();
		s_workBuffer[1].clear();
		level_createSectorWallSnapshot(&s_workBuffer[0], sectorWallIds);
		if (s_workBuffer[0].empty()) { return; }

		const u32 uncompressedSize = (u32)s_workBuffer[0].size();
		u32 compressedSize = 0;
		if (zstd_compress(s_workBuffer[1], s_workBuffer[0].data(), uncompressedSize, 4))
		{
			compressedSize = (u32)s_workBuffer[1].size();
		}
		// ERROR
		if (!compressedSize)
		{
			return;
		}

		CMD_BEGIN(LCmd_Sector_Wall_Snapshot, name);
		{
			hBuffer_addU32(uncompressedSize);
			hBuffer_addU32(compressedSize);
			hBuffer_addArrayU8(compressedSize, s_workBuffer[1].data());
		}
		CMD_END();
	}

	void cmd_sectorAttributeSnapshot(u32 name, std::vector<IndexPair>& sectorIds, bool idsChanged)
	{
		if (sectorIds.empty()) { return; }

		u16 prevCmd, prevName;
		history_getPrevCmdAndName(prevCmd, prevName);
		if (prevCmd == LCmd_Sector_Attrib_Snapshot && prevName == name && !idsChanged)
		{
			history_removeLast();
		}

		s_workBuffer[0].clear();
		s_workBuffer[1].clear();
		level_createSectorAttribSnapshot(&s_workBuffer[0], sectorIds);
		if (s_workBuffer[0].empty()) { return; }

		const u32 uncompressedSize = (u32)s_workBuffer[0].size();
		u32 compressedSize = 0;
		if (zstd_compress(s_workBuffer[1], s_workBuffer[0].data(), uncompressedSize, 4))
		{
			compressedSize = (u32)s_workBuffer[1].size();
		}
		// ERROR
		if (!compressedSize)
		{
			return;
		}

		CMD_BEGIN(LCmd_Sector_Attrib_Snapshot, name);
		{
			hBuffer_addU32(uncompressedSize);
			hBuffer_addU32(compressedSize);
			hBuffer_addArrayU8(compressedSize, s_workBuffer[1].data());
		}
		CMD_END();
	}
		
	////////////////////////////////
	// History Commands
	////////////////////////////////
	void cmd_applySectorSnapshot()
	{
		const u32 uncompressedSize = hBuffer_getU32();
		const u32 compressedSize = hBuffer_getU32();
		const u8* compressedData = hBuffer_getArrayU8(compressedSize);

		s_workBuffer[0].resize(uncompressedSize);
		if (zstd_decompress(s_workBuffer[0].data(), uncompressedSize, compressedData, compressedSize))
		{
			level_unpackSectorSnapshot(uncompressedSize, s_workBuffer[0].data());
		}
	}

	void cmd_applyObjListSnapshot()
	{
		const u32 uncompressedSize = hBuffer_getU32();
		const u32 compressedSize = hBuffer_getU32();
		const u8* compressedData = hBuffer_getArrayU8(compressedSize);

		s_workBuffer[0].resize(uncompressedSize);
		if (zstd_decompress(s_workBuffer[0].data(), uncompressedSize, compressedData, compressedSize))
		{
			level_unpackEntiyListSnapshot(uncompressedSize, s_workBuffer[0].data());
		}
	}

	void cmd_applySectorWallSnapshot()
	{
		const u32 uncompressedSize = hBuffer_getU32();
		const u32 compressedSize = hBuffer_getU32();
		const u8* compressedData = hBuffer_getArrayU8(compressedSize);

		s_workBuffer[0].resize(uncompressedSize);
		if (zstd_decompress(s_workBuffer[0].data(), uncompressedSize, compressedData, compressedSize))
		{
			level_unpackSectorWallSnapshot(uncompressedSize, s_workBuffer[0].data());
		}
	}

	void cmd_applySectorAttribSnapshot()
	{
		const u32 uncompressedSize = hBuffer_getU32();
		const u32 compressedSize = hBuffer_getU32();
		const u8* compressedData = hBuffer_getArrayU8(compressedSize);

		s_workBuffer[0].resize(uncompressedSize);
		if (zstd_decompress(s_workBuffer[0].data(), uncompressedSize, compressedData, compressedSize))
		{
			level_unpackSectorAttribSnapshot(uncompressedSize, s_workBuffer[0].data());
		}
	}
}

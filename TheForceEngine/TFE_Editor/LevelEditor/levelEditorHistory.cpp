#include "levelEditorHistory.h"
#include "sharedState.h"
#include <TFE_Editor/LevelEditor/levelEditor.h>
#include <TFE_Editor/LevelEditor/levelEditorData.h>
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
		LCmd_Entity_Snapshot,
		LCmd_Single_Entity_Snapshot,
		LCmd_Notes_Snapshot,
		LCmd_Guidelines_Snapshot,
		LCmd_Selection_Snapshot,
		LCmd_Attrib_Change,
		LCmd_Attrib_Set,
		LCmd_Count
	};

	// Used to merge certain commands.
	static f64 c_mergeThreshold = 1.0;
	static f64 s_lastMoveTex = 0.0;
				
	// Command Functions
	void cmd_applySectorSnapshot();
	void cmd_applyEntitySnapshot();
	void cmd_applySingleEntitySnapshot();
	void cmd_applyNotesSnapshot();
	void cmd_applyGuidelinesSnapshot();
	void cmd_applySelectionSnapshot();
	void cmd_applyAttribChange();
	void cmd_applyAttribSet();

	///////////////////////////////////
	// API
	///////////////////////////////////
	void levHistory_init()
	{
		history_init(level_unpackSnapshot, level_createSnapshot);

		history_registerCommand(LCmd_Sector_Snapshot, cmd_applySectorSnapshot);
		history_registerCommand(LCmd_Entity_Snapshot, cmd_applyEntitySnapshot);
		history_registerCommand(LCmd_Single_Entity_Snapshot, cmd_applySingleEntitySnapshot);
		history_registerCommand(LCmd_Notes_Snapshot, cmd_applyNotesSnapshot);
		history_registerCommand(LCmd_Guidelines_Snapshot, cmd_applyGuidelinesSnapshot);
		history_registerCommand(LCmd_Selection_Snapshot, cmd_applySelectionSnapshot);
		history_registerCommand(LCmd_Attrib_Change, cmd_applyAttribChange);
		history_registerCommand(LCmd_Attrib_Set, cmd_applyAttribSet);

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
	// Editor API
	////////////////////////////////
	enum HistoryType
	{
		HTYPE_VERTEX = 0,
		HTYPE_WALL,
		HTYPE_SECTOR,
		HTYPE_ENTITY,
		HTYPE_NOTE,
		HTYPE_GUIDELINE,
		HTYPE_COUNT
	};
	static std::vector<u8> s_cmdBuffer;
		
	void cmd_addAttributeChange(s32 count, const FeatureId* list, const u8* data, u32 name, HistoryType type, u32 attribId, u32 attribSize)
	{
		CMD_BEGIN(LCmd_Attrib_Change, name);
		{
			hBuffer_addS32(count);
			hBuffer_addArrayU64(count, list);
			hBuffer_addU8(type);
			hBuffer_addU8(attribId);
			hBuffer_addS16(attribSize);
			hBuffer_addArrayU8(attribSize * count, data);
		}
		CMD_END();
	}

	void cmd_addAttributeSet(s32 count, const FeatureId* list, const u8* data, u32 name, HistoryType type, u32 attribId, u32 attribSize)
	{
		CMD_BEGIN(LCmd_Attrib_Set, name);
		{
			hBuffer_addS32(count);
			hBuffer_addArrayU64(count, list);
			hBuffer_addU8(type);
			hBuffer_addU8(attribId);
			hBuffer_addS16(attribSize);
			hBuffer_addArrayU8(attribSize, data);
		}
		CMD_END();
	}

	////////////////////////////////
	// Sector Attributes
	////////////////////////////////
	void getSectorAttributeInfo(u32 attribId, size_t& offset, size_t& attribSize)
	{
		switch (attribId)
		{
			case SEC_ATTRIB_GROUP_ID:
			{
				offset = offsetof(EditorSector, groupId);
				attribSize = sizeof(EditorSector::groupId);
			} break;
			case SEC_ATTRIB_NAME:
			{
				offset = offsetof(EditorSector, name);
				attribSize = getSectorNameLimit();
			} break;
			case SEC_ATTRIB_FLOOR_TEX:
			{
				offset = offsetof(EditorSector, floorTex);
				attribSize = sizeof(EditorSector::floorTex);
			} break;
			case SEC_ATTRIB_CEIL_TEX:
			{
				offset = offsetof(EditorSector, ceilTex);
				attribSize = sizeof(EditorSector::ceilTex);
			} break;
			case SEC_ATTRIB_FLOOR_HEIGHT:
			{
				offset = offsetof(EditorSector, floorHeight);
				attribSize = sizeof(EditorSector::floorHeight);
			} break;
			case SEC_ATTRIB_CEIL_HEIGHT:
			{
				offset = offsetof(EditorSector, ceilHeight);
				attribSize = sizeof(EditorSector::ceilHeight);
			} break;
			case SEC_ATTRIB_SEC_HEIGHT:
			{
				offset = offsetof(EditorSector, secHeight);
				attribSize = sizeof(EditorSector::secHeight);
			} break;
			case SEC_ATTRIB_FLAGS1:
			{
				offset = offsetof(EditorSector, flags[0]);
				attribSize = sizeof(EditorSector::flags[0]);
			} break;
			case SEC_ATTRIB_FLAGS2:
			{
				offset = offsetof(EditorSector, flags[1]);
				attribSize = sizeof(EditorSector::flags[1]);
			} break;
			case SEC_ATTRIB_FLAGS3:
			{
				offset = offsetof(EditorSector, flags[2]);
				attribSize = sizeof(EditorSector::flags[2]);
			} break;
			case SEC_ATTRIB_LAYER:
			{
				offset = offsetof(EditorSector, layer);
				attribSize = sizeof(EditorSector::layer);
			} break;
		}
	}
		
	// Pair of functions for each HistoryType.
	void cmd_sectorChangeAttribute(u32 name, s32 count, const FeatureId* list, u32 attribId)
	{
		size_t offset, attribSize;
		getSectorAttributeInfo(attribId, offset, attribSize);

		s_cmdBuffer.resize(count * attribSize);
		u8* outbuffer = s_cmdBuffer.data();
		// The name has to get special handling...
		if (attribId == SEC_ATTRIB_NAME)
		{
			for (s32 i = 0; i < count; i++, outbuffer += attribSize)
			{
				EditorSector* sector = unpackFeatureId(list[i]);
				memset(outbuffer, 0, attribSize);
				memcpy(outbuffer, sector->name.c_str(), std::min(attribSize, sector->name.length()));
			}
		}
		// Other attributes can be treated the same.
		else
		{
			for (s32 i = 0; i < count; i++, outbuffer += attribSize)
			{
				EditorSector* sector = unpackFeatureId(list[i]);
				memcpy(outbuffer, (u8*)sector + offset, attribSize);
			}
		}
		cmd_addAttributeChange(count, list, s_cmdBuffer.data(), name, HTYPE_SECTOR, attribId, (u32)attribSize);
	}
	
	// Set to a single value.
	void cmd_sectorSetAttribute(u32 name, s32 count, const FeatureId* list, u32 attribId)
	{
		size_t offset, attribSize;
		getSectorAttributeInfo(attribId, offset, attribSize);

		s_cmdBuffer.resize(attribSize);
		EditorSector* sector = unpackFeatureId(list[0]);
		if (attribId == SEC_ATTRIB_NAME)
		{
			memset(s_cmdBuffer.data(), 0, attribSize);
			memcpy(s_cmdBuffer.data(), sector->name.c_str(), std::min(attribSize, sector->name.length()));
		}
		else
		{
			memcpy(s_cmdBuffer.data(), (u8*)sector + offset, attribSize);
		}
		cmd_addAttributeSet(count, list, s_cmdBuffer.data(), name, HTYPE_SECTOR, attribId, (u32)attribSize);
	}
		
	////////////////////////////////
	// History Commands
	////////////////////////////////
	void cmd_sectorApplyChangeAttribute(s32 count, const FeatureId* list, u32 attribId, u32 attribSize, const u8* data);
	void cmd_sectorApplySetAttribute(s32 count, const FeatureId* list, u32 attribId, u32 attribSize, const u8* data);

	void cmd_applySectorSnapshot()
	{
		// STUB
	}

	void cmd_applyEntitySnapshot()
	{
		// STUB
	}

	void cmd_applySingleEntitySnapshot()
	{
		// STUB
	}

	void cmd_applyNotesSnapshot()
	{
		// STUB
	}

	void cmd_applyGuidelinesSnapshot()
	{
		// STUB
	}

	void cmd_applySelectionSnapshot()
	{
		// STUB
	}

	void cmd_applyAttribChange()
	{
		const s32 count = hBuffer_getS32();
		const FeatureId* list = (FeatureId*)hBuffer_getArrayU64(count);
		const u8 type = hBuffer_getU8();
		const u8 attribId = hBuffer_getU8();
		const s16 attribSize = hBuffer_getS16();
		const u8* data = hBuffer_getArrayU8(attribSize * count);

		switch (type)
		{
			case HTYPE_SECTOR:
			{
				cmd_sectorApplyChangeAttribute(count, list, attribId, attribSize, data);
			} break;
			//...
		}
	}

	void cmd_applyAttribSet()
	{
		const s32 count = hBuffer_getS32();
		const FeatureId* list = (FeatureId*)hBuffer_getArrayU64(count);
		const u8 type = hBuffer_getU8();
		const u8 attribId = hBuffer_getU8();
		const s16 attribSize = hBuffer_getS16();
		const u8* data = hBuffer_getArrayU8(attribSize);

		switch (type)
		{
			case HTYPE_SECTOR:
			{
				cmd_sectorApplySetAttribute(count, list, attribId, attribSize, data);
			} break;
			//...
		}
	}

	////////////////////////////////
	// Sector
	////////////////////////////////
	void cmd_sectorApplyChangeAttribute(s32 count, const FeatureId* list, u32 attribId, u32 attribSize, const u8* data)
	{
		size_t offset, attribSizeCheck;
		getSectorAttributeInfo(attribId, offset, attribSizeCheck);
		assert(attribSizeCheck == attribSize);

		for (s32 i = 0; i < count; i++, data += attribSize)
		{
			EditorSector* sector = unpackFeatureId(list[i]);
			if (attribId == SEC_ATTRIB_NAME)
			{
				sector->name = (const char*)data;
			}
			else
			{
				memcpy((u8*)sector + offset, data, attribSize);
			}
		}
	}

	void cmd_sectorApplySetAttribute(s32 count, const FeatureId* list, u32 attribId, u32 attribSize, const u8* data)
	{
		size_t offset, attribSizeCheck;
		getSectorAttributeInfo(attribId, offset, attribSizeCheck);
		assert(attribSizeCheck == attribSize);

		for (s32 i = 0; i < count; i++)
		{
			EditorSector* sector = unpackFeatureId(list[i]);
			if (attribId == SEC_ATTRIB_NAME)
			{
				sector->name = (const char*)data;
			}
			else
			{
				memcpy((u8*)sector + offset, data, attribSize);
			}
		}
	}
}

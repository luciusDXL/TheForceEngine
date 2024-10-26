#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <vector>

namespace TFE_Editor
{
	typedef std::vector<u8> SnapshotBuffer;

	typedef void(*CmdApplyFunc)(void);
	typedef void(*UnpackSnapshotFunc)(s32 id, u32 size, void* data);
	typedef void(*CreateSnapshotFunc)(SnapshotBuffer* buffer);

	enum
	{
		CMD_SNAPSHOT = 0,
		CMD_START = 1,
	};

	enum HistoryErrorType
	{
		hError_None = 0,
		hError_InvalidCmdId,
		hError_InvalidNameId,
		hError_Count
	};

	// TODO: Add load and save functionality.
	void history_init(UnpackSnapshotFunc snapshotUnpackFunc, CreateSnapshotFunc createSnapshotFunc);
	void history_destroy();
	void history_clear();
	void history_removeLast();
	HistoryErrorType history_validateCommandHeaders(s32& errorIndex, u32& badValue);

	// Register general commands and names.
	void history_registerCommand(u16 id, CmdApplyFunc func);
	void history_registerName(u16 id, const char* name);
	const char* history_getName(u16 id);
		
	// Create new commands and snapshots.
	void history_createSnapshot(const char* name=nullptr);
	bool history_createCommand(u16 cmd, u16 name);
	void history_step(s32 count);
	void history_setPos(s32 pos);
	void history_showBranch(s32 pos);
	s32  history_getPos();
	u32  history_getSize();
	u32  history_getItemCount();
	void history_collapseToPos(s32 pos);
	void history_collapse();
	const char* history_getItemNameAndState(u32 index, u32& parentId, bool& isHidden);

	// Handle merging commands.
	void history_getPrevCmdAndName(u16& cmd, u16& name);

	// Get values from the buffer.
	u8    hBuffer_getU8();
	s16   hBuffer_getS16();
	s32   hBuffer_getS32();
	f32   hBuffer_getF32();
	u32   hBuffer_getU32();
	u64   hBuffer_getU64();
	Vec2f hBuffer_getVec2f();
	const u8*  hBuffer_getArrayU8(s32 count);

	// Add values to the buffer.
	void hBuffer_addU8(u8 value);
	void hBuffer_addS16(s16 value);
	void hBuffer_addS32(s32 value);
	void hBuffer_addF32(f32 value);
	void hBuffer_addU32(u32 value);
	void hBuffer_addU64(u64 value);
	void hBuffer_addVec2f(Vec2f value);
	void hBuffer_addArrayU8(s32 count, const u8* values);
}

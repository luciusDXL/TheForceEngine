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

	// TODO: Add load and save functionality.
	void history_init(UnpackSnapshotFunc snapshotUnpackFunc, CreateSnapshotFunc createSnapshotFunc);
	void history_destroy();
	void history_clear();

	// Register general commands and names.
	void history_registerCommand(u16 id, CmdApplyFunc func);
	void history_registerName(u16 id, const char* name);
	
	// Create new commands and snapshots.
	void history_createSnapshot(const char* name=nullptr);
	bool history_createCommand(u16 cmd, u16 name);
	void history_step(s32 count);
	void history_setPos(s32 pos);
	s32  history_getPos();
	s32  history_getSize();

	// Get values from the buffer.
	s32   hBuffer_getS32();
	u64   hBuffer_getU64();
	Vec2f hBuffer_getVec2f();
	const u16* hBuffer_getArrayU16(s32 count);
	const u32* hBuffer_getArrayU32(s32 count);
	const u64* hBuffer_getArrayU64(s32 count);

	// Add values to the buffer.
	void hBuffer_addS32(s32 value);
	void hBuffer_addU64(u64 value);
	void hBuffer_addVec2f(Vec2f value);
	void hBuffer_addArrayU16(s32 count, const u16* values);
	void hBuffer_addArrayU32(s32 count, const u32* values);
	void hBuffer_addArrayU64(s32 count, const u64* values);
}

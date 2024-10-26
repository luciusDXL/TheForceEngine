#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Editor/history.h>
#include <string>

namespace TFE_Editor
{
	void setSnapshotWriteBuffer(SnapshotBuffer* buffer);
	void setSnapshotReadBuffer(const u8* buffer, u32 size);

	// Write
	void writeU8(u8 value);
	void writeU32(u32 value);
	void writeS32(s32 value);
	void writeF32(f32 value);
	void writeData(const void* srcData, u32 size);
	void writeString(const std::string& str);

	// Read
	u8 readU8();
	u32 readU32();
	s32 readS32();
	f32 readF32();
	void readData(void* dstData, u32 size);
	void readString(std::string& str);
}

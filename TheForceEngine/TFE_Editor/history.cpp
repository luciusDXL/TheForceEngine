#include "history.h"
#include "errorMessages.h"
#include <TFE_Archive/zstdCompression.h>
#include <TFE_System/system.h>
#include <assert.h>
#include <algorithm>
#include <cstring>
#include <string>

namespace TFE_Editor
{
	enum
	{
		CMD_MAX_DEPTH = 64,
	};

	struct Snapshot
	{
		std::string name;
		u32 uncompressedSize;
		u32 compressedSize; // if zero, then uncompressed.
		std::vector<u8> compressedData;
	};

	struct CommandHeader
	{
		u16 cmdId;
		u16 cmdName;
		u16 parentId;
		u8  depth;
		u8  hidden;
	};

	UnpackSnapshotFunc s_snapshotUnpack = nullptr;
	CreateSnapshotFunc s_snapshotCreate = nullptr;
	std::vector<CmdApplyFunc> s_cmdFunc;
	std::vector<std::string> s_cmdName;

	std::vector<Snapshot> s_snapShots;
	std::vector<u32> s_history;
	std::vector<u8> s_historyBuffer;
	std::vector<u8> s_snapshotBuffer;

	u32 s_curPosInHistory = 0;
	u32 s_curBufferAddr = 0;
	u32 s_curSnapshot = 0;

	void history_init(UnpackSnapshotFunc snapshotUnpackFunc, CreateSnapshotFunc createSnapshotFunc)
	{
		s_snapshotUnpack = snapshotUnpackFunc;
		s_snapshotCreate = createSnapshotFunc;
		s_cmdFunc.clear();
		s_cmdName.clear();
		history_clear();

		s_history.reserve(256);					// reserve a history of 256, this may grow 65536.
		s_snapShots.reserve(256);				// reserve 256 snapshots, though this may grow to 1024.
		s_historyBuffer.reserve(1024 * 1024);   // reserve 1Mb for the history buffer.
	}

	void history_destroy()
	{
	}

	void history_clear()
	{
		// Clear the history, historyBuffer, and snapshots.
		s_snapShots.clear();
		s_history.clear();
		s_historyBuffer.clear();
		s_curPosInHistory = 0;
		s_curBufferAddr = 0;
		s_curSnapshot = 0;
		// Clear the previous snapshot index.
		if (s_snapshotUnpack)
		{
			s_snapshotUnpack(-1, 0, nullptr);
		}
	}

	// Register general commands and names.
	void history_registerCommand(u16 id, CmdApplyFunc func)
	{
		if (id >= s_cmdFunc.size())
		{
			size_t prevSize = s_cmdFunc.size();
			size_t newSize = id + 1;
			s_cmdFunc.resize(id + 1);

			for (size_t i = prevSize; i < newSize; i++)
			{
				s_cmdFunc[i] = nullptr;
			}
		}
		s_cmdFunc[id] = func;
	}

	void history_registerName(u16 id, const char* name)
	{
		if (id >= s_cmdName.size())
		{
			s_cmdName.resize(id + 1);
		}
		s_cmdName[id] = name;
	}

	const char* history_getName(u16 id)
	{
		return s_cmdName[id].c_str();
	}
		
	u32 hBuffer_pushCommand()
	{
		u32 addr = (u32)s_historyBuffer.size();
		// Make sure the next pointer is 4-byte aligned.
		if ((addr & 3) != 0)
		{
			s_historyBuffer.resize(s_historyBuffer.size() + 4 - (addr & 3));
			addr = (u32)s_historyBuffer.size();
			assert((addr & 3) == 0);
		}
		s_history.push_back(addr);
		return addr;
	}

	CommandHeader* hBuffer_createHeader()
	{
		u32 addr = hBuffer_pushCommand();
		s_historyBuffer.resize(s_historyBuffer.size() + sizeof(CommandHeader));
		s_curBufferAddr = (u32)s_historyBuffer.size();
		return (CommandHeader*)(s_historyBuffer.data() + addr);
	}

	CommandHeader* hBuffer_getHeader(u32 index)
	{
		u32 addr = s_history[index];
		s_curBufferAddr = addr + sizeof(CommandHeader);
		return (CommandHeader*)(s_historyBuffer.data() + addr);
	}

	void hideRange(s32 rMin, s32 rMax)
	{
		// Restore the buffer position afterward.
		const u32 bufferAddr = s_curBufferAddr;
		for (s32 i = rMin; i < rMax; i++)
		{
			CommandHeader* header = hBuffer_getHeader(i);
			header->hidden = 1;
		}
		s_curBufferAddr = bufferAddr;
	}
		
	// Create new commands and snapshots.
	s32 history_createSnapshotInternal(u32 size, void* data, const char* name/*=nullptr*/)
	{
		u16 parentId = u16(s_curPosInHistory);

		Snapshot snapshot = {};
		snapshot.uncompressedSize = size;

		bool useUncompressed = true;
		if (zstd_compress(snapshot.compressedData, (u8*)data, size, 4))
		{
			if (snapshot.compressedData.size() < size)
			{
				useUncompressed = false;
				snapshot.compressedSize = (u32)snapshot.compressedData.size();
			}
		}
		if (useUncompressed)
		{
			snapshot.compressedSize = size;
			snapshot.compressedData.resize(size);
			memcpy(snapshot.compressedData.data(), data, size);
		}

		if (name)
		{
			snapshot.name = name;
		}
		else
		{
			snapshot.name = "";
		}

		s32 id = (s32)s_snapShots.size();
		s_snapShots.push_back(std::move(snapshot));
		s_curSnapshot = u32(id);

		CommandHeader* header = hBuffer_createHeader();
		header->cmdId = CMD_SNAPSHOT;
		header->cmdName = id; // this holds the snapshot ID instead of a name for snapshots (which have their own name).
		header->parentId = parentId;
		header->depth = 0;
		header->hidden = 0;

		assert(s_history.size() > 0 && parentId >= 0);
		s_curPosInHistory = u16(s_history.size() - 1);
		hideRange(parentId + 1, s_curPosInHistory);

		return id;
	}

	void history_createSnapshot(const char* name/*=nullptr*/)
	{
		// Callback setup by the client.
		s_snapshotBuffer.clear();
		s_snapshotCreate(&s_snapshotBuffer);
		history_createSnapshotInternal((u32)s_snapshotBuffer.size(), s_snapshotBuffer.data(), name);
	}
		
	bool history_createCommand(u16 cmd, u16 name)
	{
		u16 parentId = u16(s_curPosInHistory);
		const CommandHeader prevHeader = *hBuffer_getHeader(parentId);
		if (prevHeader.depth >= CMD_MAX_DEPTH)
		{
			// Callback setup by the client.
			s_snapshotBuffer.clear();
			s_snapshotCreate(&s_snapshotBuffer);
			history_createSnapshotInternal((u32)s_snapshotBuffer.size(), s_snapshotBuffer.data(), s_cmdName[name].c_str());
			// Return false to let the caller know a snapshot was created instead of the command.
			return false;
		}

		CommandHeader* header = hBuffer_createHeader();
		header->cmdId = cmd;
		header->cmdName = name;
		header->parentId = parentId;
		header->depth = prevHeader.depth + 1;
		header->hidden = 0;
		
		assert(s_history.size() > 0);
		s_curPosInHistory = u16(s_history.size() - 1);
		hideRange(parentId + 1, s_curPosInHistory);

		return true;
	}

	void history_step(s32 count)
	{
		const s32 historyEnd = (s32)s_history.size() - 1;
		s32 pos = (s32)s_curPosInHistory;

		if (count == 0) { return; }
		if (count < 0 && pos <= 0) { return; }
		if (count > 0 && pos >= historyEnd) { return; }

		s32 curParent = pos;
		if (count < 0)
		{
			CommandHeader* header = hBuffer_getHeader(pos);
			pos = header->parentId;
		}
		else if (count > 0)
		{
			pos++;
			while (pos < s_history.size())
			{
				// Search for the next position that is NOT hidden and parentId == s_curPosInHistory
				CommandHeader* header = hBuffer_getHeader(pos);
				if (!header->hidden && header->parentId == curParent)
				{
					curParent = pos;
					break;
				}
				pos++;
			}
		}
		// Double-check...
		if (pos == s_curPosInHistory || pos < 0 || pos >= (s32)s_history.size())
		{
			return;
		}
		history_setPos(pos);
	}
		
	HistoryErrorType history_validateCommandHeaders(s32& errorIndex, u32& badValue)
	{
		s32 count = (s32)s_history.size();
		errorIndex = -1;
		HistoryErrorType errorType = hError_None;
		for (s32 i = 0; i < count; i++)
		{
			const CommandHeader* header = hBuffer_getHeader(i);
			if (header->cmdId >= s_cmdFunc.size())
			{
				errorIndex = i;
				badValue = header->cmdId;
				errorType = hError_InvalidCmdId;
				break;
			}
			else if (header->cmdName >= s_cmdName.size())
			{
				errorIndex = i;
				badValue = header->cmdName;
				errorType = hError_InvalidNameId;
				break;
			}
		}
		return errorType;
	}

	void history_showBranch(s32 pos)
	{
		assert(pos >= 0 && pos < (s32)s_history.size());
		s32 count = (s32)s_history.size();

		// Hide everything to start.
		for (s32 i = 0; i < count; i++)
		{
			CommandHeader* header = hBuffer_getHeader(i);
			header->hidden = 1;
		}

		// Then work backwards from the position to the root and unhide.
		s32 curPos = pos;
		while (1)
		{
			CommandHeader* header = hBuffer_getHeader(curPos);
			header->hidden = 0;
			if (curPos == 0)
			{
				break;
			}
			curPos = header->parentId;
		}
	}

	void history_setPos(s32 pos)
	{
		assert(pos >= 0 && pos < (s32)s_history.size());
		s_curPosInHistory = pos;

		// 1. Traverse backward through the parentIds until a snapshot is reached.
		u16 cmdList[257], listCount = 0;
		while (listCount <= 256)
		{
			cmdList[listCount++] = pos;

			const CommandHeader* header = hBuffer_getHeader(pos);
			if (header->cmdId == CMD_SNAPSHOT) { break; }
			assert(header->cmdId < s_cmdFunc.size());

			pos = header->parentId;
		}
		assert(listCount <= 256);

		// 2. Go backward towards the snapshot...
		for (s32 i = (s32)listCount - 1; i >= 0; i--)
		{
			const CommandHeader* cmdHeader = hBuffer_getHeader(cmdList[i]);
			assert(i != (s32)listCount - 1 || cmdHeader->cmdId == CMD_SNAPSHOT);

			if (cmdHeader->cmdId == CMD_SNAPSHOT)
			{
				const s32 id = cmdHeader->cmdName;
				Snapshot* snapshot = &s_snapShots[id];
				if (snapshot->uncompressedSize > snapshot->compressedSize)
				{
					s_snapshotBuffer.resize(snapshot->uncompressedSize);
					if (zstd_decompress(s_snapshotBuffer.data(), snapshot->uncompressedSize, snapshot->compressedData.data(), snapshot->compressedSize))
					{
						s_snapshotUnpack(id, snapshot->uncompressedSize, s_snapshotBuffer.data());
					}
				}
				else
				{
					s_snapshotUnpack(id, snapshot->compressedSize, snapshot->compressedData.data());
				}
			}
			else
			{
				assert(cmdHeader->cmdId < s_cmdFunc.size() && s_cmdFunc[cmdHeader->cmdId]);
				s_cmdFunc[cmdHeader->cmdId]();
			}
		}
	}

	u32 history_getItemCount()
	{
		return (u32)s_history.size();
	}

	void history_removeLast()
	{
		const u32 prevAddr = s_history.back();
		s_history.pop_back();
		s_historyBuffer.resize(prevAddr);

		assert(s_curPosInHistory > 0);
		s_curPosInHistory--;

		s_curBufferAddr = (u32)s_historyBuffer.size();
	}

	void history_collapse()
	{
		// Clear the history.
		history_clear();
		// Push the new snapshot.
		history_createSnapshot("Collapse History");
	}
		
	void history_collapseToPos(s32 pos)
	{
		// Remove all items from the history *after* pos.
		const s32 count = (s32)s_history.size();
		if (pos < 0 || pos >= count - 1)
		{
			return;
		}
		s32 snapShotMin = 65536;
		for (s32 i = pos + 1; i < count; i++)
		{
			CommandHeader* header = hBuffer_getHeader(i);
			if (header->cmdId == CMD_SNAPSHOT && header->cmdName < snapShotMin)
			{
				snapShotMin = header->cmdName;
			}
		}

		// Resize the history buffer.
		s_historyBuffer.resize(s_history[pos+1]);
		s_history.resize(pos + 1);
		s_curBufferAddr = (u32)s_historyBuffer.size();
		s_curPosInHistory = pos;

		// Then resize the snapshots.
		if (snapShotMin < 0xffff)
		{
			s_snapShots.resize(snapShotMin);
		}
		// Clear the previous snapshot index.
		s_snapshotUnpack(-1, 0, nullptr);
	}

	const char* history_getItemNameAndState(u32 index, u32& parentId, bool& isHidden)
	{
		const char* name = nullptr;

		if (index >= s_history.size()) { return nullptr; }
		const u32 bufferAddr = s_curBufferAddr;
		CommandHeader* header = hBuffer_getHeader(index);
		if (header && header->cmdId == CMD_SNAPSHOT)
		{
			name = s_snapShots[header->cmdName].name.c_str();
		}
		else if (header)
		{
			name = s_cmdName[header->cmdName].c_str();
		}
		parentId = header->parentId;
		isHidden = header->hidden ? true : false;
		s_curBufferAddr = bufferAddr;
		return name;
	}

	s32 history_getPos()
	{
		return s_curPosInHistory;
	}

	u32 history_getSize()
	{
		const s32 snapshotCount = (s32)s_snapShots.size();
		const Snapshot* snapshot = s_snapShots.data();

		u32 size = (u32)s_historyBuffer.size();
		for (s32 i = 0; i < snapshotCount; i++, snapshot++)
		{
			size += (u32)snapshot->compressedData.size();
			size += (u32)snapshot->name.length();
			size += sizeof(Snapshot);
		}
		size += (u32)s_history.size() * sizeof(u32);
		return size;
	}

	void history_getPrevCmdAndName(u16& cmd, u16& name)
	{
		CommandHeader* header = hBuffer_getHeader(s_curPosInHistory);
		cmd = header->cmdId;
		name = header->cmdName;
	}
	
	// Get values from the buffer.
	u8 hBuffer_getU8()
	{
		u8 value;
		memcpy(&value, &s_historyBuffer[s_curBufferAddr], sizeof(u8));
		s_curBufferAddr += sizeof(u8);
		return value;
	}

	s16 hBuffer_getS16()
	{
		s16 value;
		memcpy(&value, &s_historyBuffer[s_curBufferAddr], sizeof(s16));
		s_curBufferAddr += sizeof(s16);
		return value;
	}

	s32 hBuffer_getS32()
	{
		s32 value;
		memcpy(&value, &s_historyBuffer[s_curBufferAddr], sizeof(s32));
		s_curBufferAddr += sizeof(s32);
		return value;
	}

	f32 hBuffer_getF32()
	{
		f32 value;
		memcpy(&value, &s_historyBuffer[s_curBufferAddr], sizeof(f32));
		s_curBufferAddr += sizeof(f32);
		return value;
	}

	u32 hBuffer_getU32()
	{
		u32 value;
		memcpy(&value, &s_historyBuffer[s_curBufferAddr], sizeof(u32));
		s_curBufferAddr += sizeof(u32);
		return value;
	}

	u64 hBuffer_getU64()
	{
		u64 value;
		memcpy(&value, &s_historyBuffer[s_curBufferAddr], sizeof(u64));
		s_curBufferAddr += sizeof(u64);
		return value;
	}

	Vec2f hBuffer_getVec2f()
	{
		Vec2f value;
		memcpy(&value, &s_historyBuffer[s_curBufferAddr], sizeof(Vec2f));
		s_curBufferAddr += sizeof(Vec2f);
		return value;
	}

	const u8* hBuffer_getArrayU8(s32 count)
	{
		const u8* values = (u8*)&s_historyBuffer[s_curBufferAddr];
		s_curBufferAddr += count;
		return values;
	}

	// Add values to the buffer.
	void hBuffer_addU8(u8 value)
	{
		u8 dataSize = sizeof(u8);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		memcpy(&s_historyBuffer[s_curBufferAddr], &value, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addS16(s16 value)
	{
		u16 dataSize = sizeof(s16);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		memcpy(&s_historyBuffer[s_curBufferAddr], &value, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addS32(s32 value)
	{
		u32 dataSize = sizeof(s32);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		memcpy(&s_historyBuffer[s_curBufferAddr], &value, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addF32(f32 value)
	{
		u32 dataSize = sizeof(f32);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		memcpy(&s_historyBuffer[s_curBufferAddr], &value, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addU32(u32 value)
	{
		u32 dataSize = sizeof(u32);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		memcpy(&s_historyBuffer[s_curBufferAddr], &value, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addU64(u64 value)
	{
		u32 dataSize = sizeof(u64);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		memcpy(&s_historyBuffer[s_curBufferAddr], &value, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addVec2f(Vec2f value)
	{
		u32 dataSize = sizeof(Vec2f);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		memcpy(&s_historyBuffer[s_curBufferAddr], &value, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addArrayU8(s32 count, const u8* values)
	{
		u32 dataSize = count;
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		memcpy(&s_historyBuffer[s_curBufferAddr], values, dataSize);
		s_curBufferAddr += dataSize;
	}
}
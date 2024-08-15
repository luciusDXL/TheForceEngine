#include "history.h"
#include "errorMessages.h"
#include <TFE_Archive/zstdCompression.h>
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
		u16 depth;
	};

	UnpackSnapshotFunc s_snapshotUnpack = nullptr;
	CreateSnapshotFunc s_snapshotCreate = nullptr;
	std::vector<CmdApplyFunc> s_cmdFunc;
	std::vector<std::string> s_cmdName;

	std::vector<Snapshot> s_snapShots;
	std::vector<u32> s_history;
	std::vector<u8> s_historyBuffer;
	std::vector<u8> s_snapshotBuffer;
	std::vector<u8> s_unpackedData;

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

		s_history.reserve(256);					// reserve a history of 256, but much larger values are supported.
		s_historyBuffer.reserve(1024 * 1024);   // reserve 1Mb for the history buffer.
	}

	void history_destroy()
	{
	}

	void history_clear()
	{
		s_snapShots.clear();
		s_history.clear();
		s_historyBuffer.clear();
		s_curPosInHistory = 0;
		s_curBufferAddr = 0;
		s_curSnapshot = 0;
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

	u32 hBuffer_pushCommand()
	{
		u32 addr = (u32)s_historyBuffer.size();
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
		
	// Create new commands and snapshots.
	s32 history_createSnapshotInternal(u32 size, void* data, const char* name/*=nullptr*/)
	{
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

		snapshot.name = name ? name : "";

		s32 id = (s32)s_snapShots.size();
		s_snapShots.push_back(std::move(snapshot));
		s_curSnapshot = u32(id);

		CommandHeader* header = hBuffer_createHeader();
		header->cmdId = CMD_SNAPSHOT;
		header->cmdName = 0;
		header->parentId = id;
		header->depth = 0;

		s_curPosInHistory = u16(s_history.size() - 1);
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
		const CommandHeader* prevHeader = hBuffer_getHeader(parentId);
		if (prevHeader->depth >= CMD_MAX_DEPTH)
		{
			// Callback setup by the client.
			s_snapshotBuffer.clear();
			s_snapshotCreate(&s_snapshotBuffer);

			// TODO: Compress

			// Create the snapshot itself.
			history_createSnapshotInternal((u32)s_snapshotBuffer.size(), s_snapshotBuffer.data(), "");
			// Return false to let the caller know a snapshot was created instead of the command.
			return false;
		}

		CommandHeader* header = hBuffer_createHeader();
		header->cmdId = cmd;
		header->cmdName = name;
		header->parentId = parentId;
		header->depth = prevHeader->depth + 1;
		
		s_curPosInHistory = u16(s_history.size() - 1);
		return true;
	}

	void history_step(s32 count)
	{
		s32 pos = std::max(0, std::min((s32)s_history.size() - 1, (s32)s_curPosInHistory + count));
		history_setPos(pos);
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
				Snapshot* snapshot = &s_snapShots[cmdHeader->parentId];
				if (snapshot->uncompressedSize > snapshot->compressedSize)
				{
					s_unpackedData.resize(snapshot->uncompressedSize);
					if (zstd_decompress(s_unpackedData.data(), snapshot->uncompressedSize, snapshot->compressedData.data(), snapshot->compressedSize))
					{
						s_snapshotUnpack(cmdHeader->parentId, snapshot->uncompressedSize, s_unpackedData.data());
					}
				}
				else
				{
					s_snapshotUnpack(cmdHeader->parentId, snapshot->compressedSize, snapshot->compressedData.data());
				}
			}
			else
			{
				assert(s_cmdFunc[cmdHeader->cmdId]);
				s_cmdFunc[cmdHeader->cmdId]();
			}
		}
	}

	s32 history_getPos()
	{
		return s_curPosInHistory;
	}

	s32 history_getSize()
	{
		return (s32)s_history.size();
	}
		
	bool history_canMergeCommand(u16 cmd, u16 name, const void* dataToMatch, u32 matchSize)
	{
		CommandHeader* header = hBuffer_getHeader(s_curPosInHistory - 1);
		if (header->cmdId != cmd || header->cmdName != name)
		{
			return false;
		}
		const u8* prevCmdData = (u8*)&s_historyBuffer[s_curBufferAddr];
		return memcmp(dataToMatch, prevCmdData, matchSize) == 0;
	}

	u8* history_getPrevCmdBufferData(s32 offset)
	{
		hBuffer_getHeader(s_curPosInHistory - 1);
		return (u8*)&s_historyBuffer[s_curBufferAddr + offset];
	}
	
	// Get values from the buffer.
	u8 hBuffer_getU8()
	{
		u8 value = *((u8*)&s_historyBuffer[s_curBufferAddr]);
		s_curBufferAddr += sizeof(u8);
		return value;
	}

	s16 hBuffer_getS16()
	{
		s16 value = *((s16*)&s_historyBuffer[s_curBufferAddr]);
		s_curBufferAddr += sizeof(s16);
		return value;
	}

	s32 hBuffer_getS32()
	{
		s32 value = *((s32*)&s_historyBuffer[s_curBufferAddr]);
		s_curBufferAddr += sizeof(s32);
		return value;
	}

	f32 hBuffer_getF32()
	{
		f32 value = *((f32*)&s_historyBuffer[s_curBufferAddr]);
		s_curBufferAddr += sizeof(f32);
		return value;
	}

	u32 hBuffer_getU32()
	{
		u32 value = *((u32*)&s_historyBuffer[s_curBufferAddr]);
		s_curBufferAddr += sizeof(u32);
		return value;
	}

	u64 hBuffer_getU64()
	{
		u64 value = *((u64*)&s_historyBuffer[s_curBufferAddr]);
		s_curBufferAddr += sizeof(u64);
		return value;
	}

	Vec2f hBuffer_getVec2f()
	{
		Vec2f value = *((Vec2f*)&s_historyBuffer[s_curBufferAddr]);
		s_curBufferAddr += sizeof(Vec2f);
		return value;
	}

	const u8* hBuffer_getArrayU8(s32 count)
	{
		const u8* values = (u8*)&s_historyBuffer[s_curBufferAddr];
		s_curBufferAddr += count;
		return values;
	}

	const u16* hBuffer_getArrayU16(s32 count)
	{
		const u16* values = (u16*)&s_historyBuffer[s_curBufferAddr];
		s_curBufferAddr += count * sizeof(u16);
		return values;
	}

	const u32* hBuffer_getArrayU32(s32 count)
	{
		const u32* values = (u32*)&s_historyBuffer[s_curBufferAddr];
		s_curBufferAddr += count * sizeof(u32);
		return values;
	}

	const u64* hBuffer_getArrayU64(s32 count)
	{
		const u64* values = (u64*)&s_historyBuffer[s_curBufferAddr];
		s_curBufferAddr += count * sizeof(u64);
		return values;
	}
	
	const f32* hBuffer_getArrayF32(s32 count)
	{
		const f32* values = (f32*)&s_historyBuffer[s_curBufferAddr];
		s_curBufferAddr += count * sizeof(f32);
		return values;
	}

	const Vec2f* hBuffer_getArrayVec2f(s32 count)
	{
		const Vec2f* values = (Vec2f*)&s_historyBuffer[s_curBufferAddr];
		s_curBufferAddr += count * sizeof(Vec2f);
		return values;
	}

	// Add values to the buffer.
	void hBuffer_addU8(u8 value)
	{
		u8 dataSize = sizeof(u8);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		*((u8*)&s_historyBuffer[s_curBufferAddr]) = value;
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addS16(s16 value)
	{
		u16 dataSize = sizeof(s16);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		*((s16*)&s_historyBuffer[s_curBufferAddr]) = value;
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addS32(s32 value)
	{
		u32 dataSize = sizeof(s32);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		*((s32*)&s_historyBuffer[s_curBufferAddr]) = value;
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addF32(f32 value)
	{
		u32 dataSize = sizeof(f32);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		*((f32*)&s_historyBuffer[s_curBufferAddr]) = value;
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addU32(u32 value)
	{
		u32 dataSize = sizeof(u32);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		*((u32*)&s_historyBuffer[s_curBufferAddr]) = value;
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addU64(u64 value)
	{
		u32 dataSize = sizeof(u64);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		*((u64*)&s_historyBuffer[s_curBufferAddr]) = value;
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addVec2f(Vec2f value)
	{
		u32 dataSize = sizeof(Vec2f);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		*((Vec2f*)&s_historyBuffer[s_curBufferAddr]) = value;
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addArrayU8(s32 count, const u8* values)
	{
		u32 dataSize = count;
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		u8* dstValues = (u8*)&s_historyBuffer[s_curBufferAddr];
		memcpy(dstValues, values, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addArrayU16(s32 count, const u16* values)
	{
		u32 dataSize = count * sizeof(u16);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		u16* dstValues = (u16*)&s_historyBuffer[s_curBufferAddr];
		memcpy(dstValues, values, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addArrayU32(s32 count, const u32* values)
	{
		u32 dataSize = count * sizeof(u32);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		u32* dstValues = (u32*)&s_historyBuffer[s_curBufferAddr];
		memcpy(dstValues, values, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addArrayU64(s32 count, const u64* values)
	{
		u32 dataSize = count * sizeof(u64);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		u64* dstValues = (u64*)&s_historyBuffer[s_curBufferAddr];
		memcpy(dstValues, values, dataSize);
		s_curBufferAddr += dataSize;
	}
		
	void hBuffer_addArrayF32(s32 count, const f32* values)
	{
		u32 dataSize = count * sizeof(f32);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		f32* dstValues = (f32*)&s_historyBuffer[s_curBufferAddr];
		memcpy(dstValues, values, dataSize);
		s_curBufferAddr += dataSize;
	}

	void hBuffer_addArrayVec2f(s32 count, const Vec2f* values)
	{
		u32 dataSize = count * sizeof(Vec2f);
		s_historyBuffer.resize(s_curBufferAddr + dataSize);

		Vec2f* dstValues = (Vec2f*)&s_historyBuffer[s_curBufferAddr];
		memcpy(dstValues, values, dataSize);
		s_curBufferAddr += dataSize;
	}
}
#include "snapshotReaderWriter.h"
#include <cstring>

namespace TFE_Editor
{
	static SnapshotBuffer* s_buffer = nullptr;
	static const u8* s_readBuffer = nullptr;
	static u32 s_bufferSize = 0;

	void setSnapshotWriteBuffer(SnapshotBuffer* buffer)
	{
		s_buffer = buffer;
	}

	void setSnapshotReadBuffer(const u8* buffer, u32 size)
	{
		s_readBuffer = buffer;
		s_bufferSize = size;
	}

	// Note: memcpys() are used to avoid pointer alignment issues.
	void writeU8(u8 value)
	{
		s_buffer->push_back(value);
	}

	void writeU32(u32 value)
	{
		const size_t pos = s_buffer->size();
		s_buffer->resize(pos + sizeof(u32));
		memcpy(s_buffer->data() + pos, &value, sizeof(u32));
	}

	void writeS32(s32 value)
	{
		const size_t pos = s_buffer->size();
		s_buffer->resize(pos + sizeof(s32));
		memcpy(s_buffer->data() + pos, &value, sizeof(s32));
	}

	void writeF32(f32 value)
	{
		const size_t pos = s_buffer->size();
		s_buffer->resize(pos + sizeof(f32));
		memcpy(s_buffer->data() + pos, &value, sizeof(f32));
	}

	void writeData(const void* srcData, u32 size)
	{
		const size_t pos = s_buffer->size();
		s_buffer->resize(pos + size);
		memcpy(s_buffer->data() + pos, srcData, size);
	}

	void writeString(const std::string& str)
	{
		writeU32((u32)str.length());
		writeData(str.data(), (u32)str.length());
	}

	u8 readU8()
	{
		u8 value = *s_readBuffer;
		s_readBuffer++;
		return value;
	}

	u32 readU32()
	{
		u32 value;
		memcpy(&value, s_readBuffer, sizeof(u32));
		s_readBuffer += sizeof(u32);
		return value;
	}

	s32 readS32()
	{
		s32 value;
		memcpy(&value, s_readBuffer, sizeof(s32));
		s_readBuffer += sizeof(s32);
		return value;
	}

	f32 readF32()
	{
		f32 value;
		memcpy(&value, s_readBuffer, sizeof(f32));
		s_readBuffer += sizeof(f32);
		return value;
	}

	void readData(void* dstData, u32 size)
	{
		memcpy(dstData, s_readBuffer, size);
		s_readBuffer += size;
	}

	void readString(std::string& str)
	{
		u32 len = readU32();
		char strBuffer[1024];
		readData(strBuffer, len);
		strBuffer[len] = 0;

		str = strBuffer;
	}
}

#pragma once
#include <TFE_FileSystem/stream.h>

class MemoryStream : public Stream
{
public:
	MemoryStream();
	~MemoryStream();

	// Load the memory stream from pre-existing memory.
	// This makes a copy, so the original memory can be freed.
	// This will fail if size == 0 or mem == nullptr.
	// This will also reset the memory stream.
	bool load(size_t size, const void* const mem);
	// Pre-allocate stream memory, this assumes that the client will call
	// data() to get the pointer and then fill it in.
	bool allocate(size_t size);
	// Get the data so it can be saved or used elsewhere.
	// Note the client should not modify the pointer or memory.
	const void* const data() const;
	// Writeable data pointer.
	void* data();

	// This clears out the size, address, mode, etc. so the stream can be re-used,
	// but does not free memory.
	void clear();

	// Standard stream access.
	bool open(AccessMode mode);
	void close();
			
	//derived functions.
	bool seek(s32 offset, Origin origin=ORIGIN_START) override;
	size_t getLoc() override;
	size_t getSize() override;
	bool   isOpen()  const;

	void read(s8*  ptr, u32 count=1) override { readType(ptr, count); }
	void read(u8*  ptr, u32 count=1) override { readType(ptr, count); }
	void read(s16* ptr, u32 count=1) override { readType(ptr, count); }
	void read(u16* ptr, u32 count=1) override { readType(ptr, count); }
	void read(s32* ptr, u32 count=1) override { readType(ptr, count); }
	void read(u32* ptr, u32 count=1) override { readType(ptr, count); }
	void read(s64* ptr, u32 count=1) override { readType(ptr, count); }
	void read(u64* ptr, u32 count=1) override { readType(ptr, count); }
	void read(f32* ptr, u32 count=1) override { readType(ptr, count); }
	void read(f64* ptr, u32 count=1) override { readType(ptr, count); }
	void read(std::string* ptr, u32 count=1) override { readTypeStr(ptr, count); }
	u32  readBuffer(void* ptr, u32 size, u32 count=1) override;

	void write(const s8*  ptr, u32 count=1)  override { writeType(ptr, count); }
	void write(const u8*  ptr, u32 count=1)  override { writeType(ptr, count); }
	void write(const s16* ptr, u32 count=1)  override { writeType(ptr, count); }
	void write(const u16* ptr, u32 count=1)  override { writeType(ptr, count); }
	void write(const s32* ptr, u32 count=1)  override { writeType(ptr, count); }
	void write(const u32* ptr, u32 count=1)  override { writeType(ptr, count); }
	void write(const s64* ptr, u32 count=1)  override { writeType(ptr, count); }
	void write(const u64* ptr, u32 count=1)  override { writeType(ptr, count); }
	void write(const f32* ptr, u32 count=1) override { writeType(ptr, count); }
	void write(const f64* ptr, u32 count=1) override { writeType(ptr, count); }
	void write(const std::string* ptr, u32 count=1) override { writeTypeStr(ptr, count); }
	void writeBuffer(const void* ptr, u32 size, u32 count=1) override;

	void writeString(const char* fmt, ...) override;

private:
	template <typename T>
	void readType(T* ptr, u32 count);

	void readTypeStr(std::string* ptr, u32 count);

	template <typename T>
	void writeType(const T* ptr, u32 count);

	void writeTypeStr(const std::string* ptr, u32 count);

	void resizeBuffer(size_t newSize);

private:
	u8* m_memory;
	size_t m_size;
	size_t m_capacity;
	size_t m_addr;
	AccessMode m_mode;
};

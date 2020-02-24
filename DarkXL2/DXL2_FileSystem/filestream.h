#pragma once
#include <DXL2_FileSystem/stream.h>

class FileStream : public Stream
{
public:
	enum FileMode
	{
		MODE_READ = 0,		//read-only  (will fail if the file doesn't exist)
		MODE_WRITE,			//write      (will overwrite the file)
		MODE_READWRITE,		//read-write (will create a new file if it doesn't exist, the file can be read from and written to).
		MODE_COUNT,
		MODE_INVALID = MODE_COUNT
	};
public:
	FileStream();
	~FileStream();

	bool exists(const char* filename);
	bool open(const char* filename, FileMode mode);
	void close();
	
	//derived functions.
	void seek(u32 offset, Origin origin=ORIGIN_START);
	size_t getLoc();
	size_t getSize();
	bool   isOpen()  const;

	void flush();

	void read(s8*  ptr, u32 count=1) { readType(ptr, count); }
	void read(u8*  ptr, u32 count=1) { readType(ptr, count); }
	void read(s16* ptr, u32 count=1) { readType(ptr, count); }
	void read(u16* ptr, u32 count=1) { readType(ptr, count); }
	void read(s32* ptr, u32 count=1) { readType(ptr, count); }
	void read(u32* ptr, u32 count=1) { readType(ptr, count); }
	void read(s64* ptr, u32 count=1) { readType(ptr, count); }
	void read(u64* ptr, u32 count=1) { readType(ptr, count); }
	//void read(f16* ptr, u32 count=1) { readType(ptr, count); }
	void read(f32* ptr, u32 count=1) { readType(ptr, count); }
	void read(f64* ptr, u32 count=1) { readType(ptr, count); }
	void read(std::string* ptr, u32 count=1) { readType(ptr, count); }
	void readBuffer(void* ptr, u32 size, u32 count=1);

	void write(const s8*  ptr, u32 count=1) { writeType(ptr, count); }
	void write(const u8*  ptr, u32 count=1) { writeType(ptr, count); }
	void write(const s16* ptr, u32 count=1) { writeType(ptr, count); }
	void write(const u16* ptr, u32 count=1) { writeType(ptr, count); }
	void write(const s32* ptr, u32 count=1) { writeType(ptr, count); }
	void write(const u32* ptr, u32 count=1) { writeType(ptr, count); }
	void write(const s64* ptr, u32 count=1) { writeType(ptr, count); }
	void write(const u64* ptr, u32 count=1) { writeType(ptr, count); }
	//void write(const f16* ptr, u32 count=1) { writeType(ptr, count); }
	void write(const f32* ptr, u32 count=1) { writeType(ptr, count); }
	void write(const f64* ptr, u32 count=1) { writeType(ptr, count); }
	void write(const std::string* ptr, u32 count=1) { writeType(ptr, count); }
	void writeBuffer(const void* ptr, u32 size, u32 count=1);

private:
	template <typename T>
	void readType(T* ptr, u32 count);

	template <>
	void readType<std::string>(std::string* ptr, u32 count);

	template <typename T>
	void writeType(const T* ptr, u32 count);

	template <>
	void writeType<std::string>(const std::string* ptr, u32 count);

private:
	FILE*    m_file;
	FileMode m_mode;
};
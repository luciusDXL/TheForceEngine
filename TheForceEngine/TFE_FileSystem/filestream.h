#pragma once
#include <TFE_FileSystem/stream.h>
#include <TFE_FileSystem/paths.h>

////////////////////////////////////////////////////
// TODO: FileStream directly accesses arhive data.
////////////////////////////////////////////////////

class Archive;

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
	bool open(const FilePath* filePath, FileMode mode);
	void close();

	static u32 readContents(const char* filePath, void** output);
	static u32 readContents(const char* filePath, void* output, size_t size);
	static u32 readContents(const FilePath* filePath, void** output);
	static u32 readContents(const FilePath* filePath, void* output, size_t size);
	
	//derived functions.
	bool seek(u32 offset, Origin origin=ORIGIN_START) override;
	size_t getLoc() override;
	size_t getSize() override;
	bool   isOpen()  const;

	void flush();

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
	void read(std::string* ptr, u32 count=1) override { readString(ptr, count); }
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
	void write(const std::string* ptr, u32 count=1) override { writeString(ptr, count); }
	void writeBuffer(const void* ptr, u32 size, u32 count=1) override;

	void writeString(const char* fmt, ...) override;

private:
	template <typename T>
	void readType(T* ptr, u32 count);

	void readString(std::string* ptr, u32 count);

	template <typename T>
	void writeType(const T* ptr, u32 count);

	void writeString(const std::string* ptr, u32 count);

private:
	FILE*    m_file;
	Archive* m_archive;
	FileMode m_mode;
};
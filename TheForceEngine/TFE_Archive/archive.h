#pragma once
#include <cstdio>

#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>

enum ArchiveType
{
	ARCHIVE_GOB = 0,
	ARCHIVE_LFD,
	ARCHIVE_LAB,
	ARCHIVE_ZIP,
	ARCHIVE_COUNT,
	ARCHIVE_UNKNOWN = ARCHIVE_COUNT
};

#define INVALID_FILE 0xffffffff

class Archive
{
	// Public API handling the same archive in multiple locations.
public:
	static Archive* getArchive(ArchiveType type, const char* name, const char* path);
	static void freeArchive(Archive* archive);
	static void freeAllArchives();

	static Archive* createCustomArchive(ArchiveType type, const char* path);
	static void deleteCustomArchive(Archive* archive);

	static ArchiveType getArchiveTypeFromName(const char* path);
	
	// Public Archive API
public:
	virtual ~Archive() {}

	// Archive
	virtual bool create(const char *archivePath) = 0;
	virtual bool open(const char *archivePath) = 0;
	virtual void close() = 0;

	const char* getName() { return m_name; }
	const char* getPath() { return m_archivePath; }

	// File Access
	virtual bool openFile(const char *file) = 0;
	virtual bool openFile(u32 index) = 0;
	virtual void closeFile() = 0;

	virtual bool fileExists(const char *file) = 0;
	virtual bool fileExists(u32 index) = 0;
	virtual u32  getFileIndex(const char* file) = 0;

	virtual size_t getFileLength() = 0;
	virtual size_t readFile(void *data, size_t size) = 0;
	virtual bool seekFile(s32 offset, s32 origin = SEEK_SET) = 0;
	virtual size_t getLocInFile() = 0;

	// Directory
	virtual u32 getFileCount() = 0;
	virtual const char* getFileName(u32 index) = 0;
	virtual size_t getFileLength(u32 index) = 0;

	// Edit
	virtual void addFile(const char* fileName, const char* filePath) = 0;

	// Shared Private State
protected:
	ArchiveType m_type;
	char m_name[TFE_MAX_PATH];
	char m_archivePath[TFE_MAX_PATH];

	s32 m_fileOffset;
};

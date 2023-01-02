#pragma once
// A GOB archive fully loaded into memory.

#include <TFE_System/types.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include "archive.h"
#include "gobArchive.h"

class GobMemoryArchive : public Archive
{
public:
	GobMemoryArchive() : Archive(ARCHIVE_GOB), m_buffer(nullptr), m_size(0), m_readLoc(0), m_archiveOpen(false), m_curFile(-1) {}
	~GobMemoryArchive() override;

	// Archive
	bool create(const char *archivePath) override;
	bool open(const char *archivePath) override;
	bool open(const u8* buffer, size_t size);
	void close() override;

	// File Access
	bool openFile(const char *file) override;
	bool openFile(u32 index) override;
	void closeFile() override;

	u32 getFileIndex(const char* file) override;
	bool fileExists(const char *file) override;
	bool fileExists(u32 index) override;

	size_t getFileLength() override;
	size_t readFile(void *data, size_t size) override;
	bool seekFile(s32 offset, s32 origin = SEEK_SET) override;
	size_t getLocInFile() override;

	// Directory
	u32 getFileCount() override;
	const char* getFileName(u32 index) override;
	size_t getFileLength(u32 index) override;

	// Edit
	void addFile(const char* fileName, const char* filePath) override;

private:
	const u8* m_buffer;
	size_t m_size;
	size_t m_readLoc;
	bool m_archiveOpen;

	GobArchive::GOB_Header_t* m_header;
	GobArchive::GOB_Index_t   m_fileList;
	s32 m_curFile;
};

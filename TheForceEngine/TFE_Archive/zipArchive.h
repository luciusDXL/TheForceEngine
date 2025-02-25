#pragma once
#include "archive.h"
#include <string>

class ZipArchive : public Archive
{
public:
	ZipArchive() : Archive(ARCHIVE_ZIP), m_entryCount(0), m_curFile(INVALID_FILE), m_entries(nullptr), m_fileHandle(nullptr) {}
	~ZipArchive() override;

	// Archive
	bool create(const char *archivePath) override;
	bool open(const char *archivePath) override;
	void close() override;

	// File Access
	bool openFile(const char *file) override;
	bool openFile(u32 index) override;
	void closeFile() override;

	bool fileExists(const char *file) override;
	bool fileExists(u32 index) override;
	u32  getFileIndex(const char* file) override;

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
	struct ZipEntry
	{
		std::string name;
		size_t length;
		bool isDir;
	};

	s32 m_entryCount;
	u32 m_curFile;
	ZipEntry* m_entries;
	void* m_fileHandle;

	u8* m_tempBuffer = nullptr;
	size_t m_tempBufferSize = 0;
	bool m_entryRead;

	struct ZipNewFile
	{
		std::string name;
		std::string path;
		bool isDir;
	};
	std::vector<ZipNewFile> m_newFiles;
};

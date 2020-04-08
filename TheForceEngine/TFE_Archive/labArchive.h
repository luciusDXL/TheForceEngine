#pragma once
#include <TFE_System/types.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include "archive.h"

class LabArchive : public Archive
{
public:
	LabArchive() : m_archiveOpen(false), m_curFile(-1) {}
	~LabArchive() override;

	// Archive
	bool create(const char *archivePath) override;
	bool open(const char *archivePath) override;
	void close() override;

	// File Access
	bool openFile(const char *file) override;
	bool openFile(u32 index) override;
	void closeFile() override;

	u32 getFileIndex(const char* file) override;
	bool fileExists(const char *file) override;
	bool fileExists(u32 index) override;

	size_t getFileLength() override;
	bool readFile(void *data, size_t size) override;

	// Directory
	u32 getFileCount() override;
	const char* getFileName(u32 index) override;
	size_t getFileLength(u32 index) override;

	// Edit
	void addFile(const char* fileName, const char* filePath) override;

private:
	#pragma pack(push)
	#pragma pack(1)
	struct LAB_Header_t
	{
		char LAB_MAGIC[4];
		u32  version;	// 0x10000 for Outlaws
		u32  fileCount;
		u32  stringTableSize;
	};

	struct LAB_Entry_t
	{
		u32 nameOffset;
		u32 dataOffset;
		u32 len;
		u8  typeId[4];
	};
	#pragma pack(pop)

	FileStream m_file;
	bool m_archiveOpen;

	LAB_Header_t m_header;
	char* m_stringTable;
	LAB_Entry_t* m_entries;

	s32 m_curFile;
};

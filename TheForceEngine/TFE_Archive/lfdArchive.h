#pragma once
///////////////////////////////////////////////
// LFD Archives are very similar to GOB
// but have a different directory structure.
///////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include "archive.h"

class LfdArchive : public Archive
{
public:
	LfdArchive() : m_archiveOpen(false), m_curFile(-1) {}
	~LfdArchive() override;

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
	#pragma pack(push)
	#pragma pack(1)

	typedef struct
	{
		char TYPE[4];
		char NAME[8];
		u32 LENGTH;		//length of the file.
	} LFD_Entry_t;

	typedef struct
	{
		char NAME[16];
		u32 LENGTH;		//length of the file.
		u32 IX;
	} LFD_EntryFinal_t;

	typedef struct
	{
		u32 MASTERN;	//num files
		LFD_EntryFinal_t *entries;
	} LFD_Index_t;

	#pragma pack(pop)

	FileStream m_file;
	bool m_archiveOpen;

	LFD_Entry_t m_header;
	LFD_Index_t m_fileList;
	s32 m_curFile;
};

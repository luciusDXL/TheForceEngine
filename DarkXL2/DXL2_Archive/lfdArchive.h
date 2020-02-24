#pragma once
///////////////////////////////////////////////
// LFD Archives are very similar to GOB
// but have a different directory structure.
///////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_FileSystem/filestream.h>
#include <DXL2_FileSystem/paths.h>
#include "archive.h"

class LfdArchive : public Archive
{
public:
	LfdArchive() : m_archiveOpen(false), m_curFile(-1) {}
	~LfdArchive() override;

	// Archive
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
	bool readFile(void *data, size_t size) override;

	// Directory
	u32 getFileCount() override;
	const char* getFileName(u32 index) override;
	size_t getFileLength(u32 index) override;

private:
	#pragma pack(push)
	#pragma pack(1)

	typedef struct
	{
		char TYPE[4];
		char NAME[8];
		long LENGTH;		//length of the file.
	} LFD_Entry_t;

	typedef struct
	{
		char TYPE[5];
		char NAME[9];
		long LENGTH;		//length of the file.
		long IX;
	} LFD_EntryFinal_t;

	typedef struct
	{
		long MASTERN;	//num files
		LFD_EntryFinal_t *entries;
	} LFD_Index_t;

	#pragma pack(pop)

	FileStream m_file;
	bool m_archiveOpen;

	LFD_Entry_t m_header;
	LFD_Index_t m_fileList;
	s32 m_curFile;
};

#pragma once
#include <DXL2_System/types.h>
#include <DXL2_FileSystem/filestream.h>
#include <DXL2_FileSystem/paths.h>
#include "archive.h"

class GobArchive : public Archive
{
public:
	GobArchive() : m_archiveOpen(false), m_curFile(-1) {}
	~GobArchive() override;

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

	typedef struct
	{
		char GOB_MAGIC[4];
		long MASTERX;	//offset to GOX_Index_t
	} GOB_Header_t;

	typedef struct
	{
		long IX;		//offset to the start of the file.
		long LEN;		//length of the file.
		char NAME[13];	//file name.
	} GOB_Entry_t;

	typedef struct
	{
		long MASTERN;	//num files
		GOB_Entry_t *entries;
	} GOB_Index_t;

	#pragma pack(pop)

	FileStream m_file;
	bool m_archiveOpen;

	GOB_Header_t m_header;
	GOB_Index_t m_fileList;
	s32 m_curFile;
};

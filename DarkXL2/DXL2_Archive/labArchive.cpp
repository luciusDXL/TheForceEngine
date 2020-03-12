#include <DXL2_System/system.h>
#include "labArchive.h"
#include <assert.h>
#include <algorithm>

LabArchive::~LabArchive()
{
	close();
}

bool LabArchive::create(const char *archivePath)
{
	// STUB: No facilities to write Lab files yet.
	return false;
}

bool LabArchive::open(const char *archivePath)
{
	m_archiveOpen = m_file.open(archivePath, FileStream::MODE_READ);
	m_curFile = -1;
	if (!m_archiveOpen) { return false; }

	// Read the directory.
	m_file.readBuffer(&m_header, sizeof(GOB_Header_t));

	int stringTableSize;
	m_file.readBuffer(&m_fileList.MASTERN, sizeof(long));
	m_file.readBuffer(&stringTableSize, sizeof(long));
	m_fileList.entries = new GOB_Entry_t[m_fileList.MASTERN];

	//now read string table.
	m_file.seek(16 * (m_fileList.MASTERN + 1));
	char *stringTable = new char[stringTableSize + 1];
	m_file.readBuffer(stringTable, stringTableSize);

	//now read the entries.
	m_file.seek(16);
	for (s32 e = 0; e < m_fileList.MASTERN; e++)
	{
		u32 fname_offs, start, size, pad32;
		m_file.read(&fname_offs);
		m_file.read(&start);
		m_file.read(&size);
		m_file.read(&pad32);

		m_fileList.entries[e].IX = start;
		m_fileList.entries[e].LEN = size;
		strcpy(m_fileList.entries[e].NAME, &stringTable[fname_offs]);
	}
	delete[] stringTable;

	strcpy(m_archivePath, archivePath);
	m_file.close();

	return true;
}

void LabArchive::close()
{
	m_file.close();
	m_archiveOpen = false;
}

// File Access
bool LabArchive::openFile(const char *file)
{
	if (!m_archiveOpen) { return false; }

	m_file.open(m_archivePath, FileStream::MODE_READ);
	m_curFile = -1;

	//search for this file.
	for (s32 i = 0; i < m_fileList.MASTERN; i++)
	{
		if (strcasecmp(file, m_fileList.entries[i].NAME) == 0)
		{
			m_curFile = i;
			break;
		}
	}

	if (m_curFile == -1)
	{
		m_file.close();
		DXL2_System::logWrite(LOG_ERROR, "GOB", "Failed to load \"%s\" from \"%s\"", file, m_archivePath);
	}
	return m_curFile > -1 ? true : false;
}

bool LabArchive::openFile(u32 index)
{
	if (index >= getFileCount()) { return false; }

	m_curFile = s32(index);
	m_file.open(m_archivePath, FileStream::MODE_READ);
	return true;
}

void LabArchive::closeFile()
{
	m_curFile = -1;
	m_file.close();
}

u32 LabArchive::getFileIndex(const char* file)
{
	if (!m_archiveOpen) { return INVALID_FILE; }
	m_curFile = -1;

	//search for this file.
	for (s32 i = 0; i < m_fileList.MASTERN; i++)
	{
		if (strcasecmp(file, m_fileList.entries[i].NAME) == 0)
		{
			return i;
		}
	}
	return INVALID_FILE;
}

bool LabArchive::fileExists(const char *file)
{
	if (!m_archiveOpen) { return false; }
	m_curFile = -1;

	//search for this file.
	for (s32 i = 0; i < m_fileList.MASTERN; i++)
	{
		if (strcasecmp(file, m_fileList.entries[i].NAME) == 0)
		{
			return true;
		}
	}
	return false;
}

bool LabArchive::fileExists(u32 index)
{
	if (index >= getFileCount()) { return false; }
	return true;
}

size_t LabArchive::getFileLength()
{
	if (m_curFile < 0) { return 0; }
	return getFileLength(m_curFile);
}

bool LabArchive::readFile(void *data, size_t size)
{
	if (m_curFile < 0) { return false; }
	if (size == 0) { size = m_fileList.entries[m_curFile].LEN; }
	const size_t sizeToRead = std::min(size, (size_t)m_fileList.entries[m_curFile].LEN);

	m_file.seek(m_fileList.entries[m_curFile].IX);
	m_file.readBuffer(data, (u32)sizeToRead);
	return true;
}

// Directory
u32 LabArchive::getFileCount()
{
	if (!m_archiveOpen) { return 0; }
	return (u32)m_fileList.MASTERN;
}

const char* LabArchive::getFileName(u32 index)
{
	if (!m_archiveOpen) { return nullptr; }
	return m_fileList.entries[index].NAME;
}

size_t LabArchive::getFileLength(u32 index)
{
	if (!m_archiveOpen) { return 0; }
	return m_fileList.entries[index].LEN;
}

// Edit
void LabArchive::addFile(const char* fileName, const char* filePath)
{
	// STUB: No ability to add files yet.
	assert(0);
}

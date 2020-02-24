#include <DXL2_System/system.h>
#include "gobArchive.h"
#include <algorithm>

GobArchive::~GobArchive()
{
	close();
}

// Archive
bool GobArchive::open(const char *archivePath)
{
	m_archiveOpen = m_file.open(archivePath, FileStream::MODE_READ);
	m_curFile = -1;
	if (!m_archiveOpen) { return false; }

	// Read the directory.
	m_file.readBuffer(&m_header, sizeof(GOB_Header_t));
	m_file.seek(m_header.MASTERX);

	m_file.readBuffer(&m_fileList.MASTERN, sizeof(long));
	m_fileList.entries = new GOB_Entry_t[m_fileList.MASTERN];
	m_file.readBuffer(m_fileList.entries, sizeof(GOB_Entry_t), m_fileList.MASTERN);

	strcpy(m_archivePath, archivePath);
	m_file.close();

	return true;
}

void GobArchive::close()
{
	m_file.close();
	m_archiveOpen = false;
}

// File Access
bool GobArchive::openFile(const char *file)
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

bool GobArchive::openFile(u32 index)
{
	if (index >= getFileCount()) { return false; }

	m_curFile = s32(index);
	m_file.open(m_archivePath, FileStream::MODE_READ);
	return true;
}

void GobArchive::closeFile()
{
	m_curFile = -1;
	m_file.close();
}

u32 GobArchive::getFileIndex(const char* file)
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

bool GobArchive::fileExists(const char *file)
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

bool GobArchive::fileExists(u32 index)
{
	if (index >= getFileCount()) { return false; }
	return true;
}

size_t GobArchive::getFileLength()
{
	if (m_curFile < 0) { return 0; }
	return getFileLength(m_curFile);
}

bool GobArchive::readFile(void *data, size_t size)
{
	if (m_curFile < 0) { return false; }
	if (size == 0) { size = m_fileList.entries[m_curFile].LEN; }
	const size_t sizeToRead = std::min(size, (size_t)m_fileList.entries[m_curFile].LEN);

	m_file.seek(m_fileList.entries[m_curFile].IX);
	m_file.readBuffer(data, (u32)sizeToRead);
	return true;
}

// Directory
u32 GobArchive::getFileCount()
{
	if (!m_archiveOpen) { return 0; }
	return (u32)m_fileList.MASTERN;
}

const char* GobArchive::getFileName(u32 index)
{
	if (!m_archiveOpen) { return nullptr; }
	return m_fileList.entries[index].NAME;
}

size_t GobArchive::getFileLength(u32 index)
{
	if (!m_archiveOpen) { return 0; }
	return m_fileList.entries[index].LEN;
}

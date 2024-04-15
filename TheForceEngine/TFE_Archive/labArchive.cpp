#include <cstring>

#include <TFE_System/system.h>
#include <TFE_System/endian.h>
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
	m_archiveOpen = m_file.open(archivePath, Stream::MODE_READ);
	m_curFile = -1;
	m_fileOffset = 0;
	if (!m_archiveOpen) { return false; }

	// Read the directory.
	m_file.readBuffer(&m_header, sizeof(LAB_Header_t));
	m_header.version = TFE_Endian::le32_to_cpu(m_header.version);
	m_header.fileCount = TFE_Endian::le32_to_cpu(m_header.fileCount);
	m_header.stringTableSize = TFE_Endian::le32_to_cpu(m_header.stringTableSize);
	m_stringTable = new char[m_header.stringTableSize + 1];
	m_entries = new LAB_Entry_t[m_header.fileCount];

	// Read the file entries.
	m_file.readBuffer(m_entries, sizeof(LAB_Entry_t), m_header.fileCount);
	for (u32 i = 0; i < m_header.fileCount; i++)
	{
		m_entries[i].nameOffset = TFE_Endian::le32_to_cpu(m_entries[i].nameOffset);
		m_entries[i].dataOffset = TFE_Endian::le32_to_cpu(m_entries[i].dataOffset);
		m_entries[i].len = TFE_Endian::le32_to_cpu(m_entries[i].len);
	}

	// Read string table.
	m_file.readBuffer(m_stringTable, m_header.stringTableSize);
	m_file.close();
		
	strcpy(m_archivePath, archivePath);
	
	return true;
}

void LabArchive::close()
{
	m_file.close();
	m_archiveOpen = false;
	delete[] m_entries;
	delete[] m_stringTable;
}

// File Access
bool LabArchive::openFile(const char *file)
{
	if (!m_archiveOpen) { return false; }

	m_file.open(m_archivePath, Stream::MODE_READ);
	m_curFile = -1;
	m_fileOffset = 0;

	//search for this file.
	for (u32 i = 0; i < m_header.fileCount; i++)
	{
		if (strcasecmp(file, &m_stringTable[m_entries[i].nameOffset]) == 0)
		{
			m_curFile = i;
			break;
		}
	}

	if (m_curFile == -1)
	{
		m_file.close();
		TFE_System::logWrite(LOG_ERROR, "GOB", "Failed to load \"%s\" from \"%s\"", file, m_archivePath);
	}
	else
	{
		m_file.seek(m_entries[m_curFile].dataOffset);
	}
	return m_curFile > -1 ? true : false;
}

bool LabArchive::openFile(u32 index)
{
	if (index >= getFileCount()) { return false; }

	m_curFile = s32(index);
	m_fileOffset = 0;
	m_file.open(m_archivePath, Stream::MODE_READ);
	m_file.seek(m_entries[m_curFile].dataOffset);
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
	for (u32 i = 0; i < m_header.fileCount; i++)
	{
		if (strcasecmp(file, &m_stringTable[m_entries[i].nameOffset]) == 0)
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
	for (u32 i = 0; i < m_header.fileCount; i++)
	{
		if (strcasecmp(file, &m_stringTable[m_entries[i].nameOffset]) == 0)
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

size_t LabArchive::readFile(void *data, size_t size)
{
	if (m_curFile < 0) { return false; }
	if (size == 0) { size = m_entries[m_curFile].len; }
	const size_t sizeToRead = std::min(size, (size_t)m_entries[m_curFile].len);

	size_t bytesRead = m_file.readBuffer(data, (u32)sizeToRead);
	m_fileOffset += (s32)sizeToRead;
	return bytesRead;
}

bool LabArchive::seekFile(s32 offset, s32 origin)
{
	if (m_curFile < 0) { return false; }
	size_t size = m_entries[m_curFile].len;

	switch (origin)
	{
		case SEEK_SET:
		{
			m_fileOffset = offset;
		} break;
		case SEEK_CUR:
		{
			m_fileOffset += offset;
		} break;
		case SEEK_END:
		{
			m_fileOffset = (s32)size - offset;
		} break;
	}
	assert(m_fileOffset <= size && m_fileOffset >= 0);
	if (m_fileOffset > size || m_fileOffset < 0)
	{
		m_fileOffset = 0;
		return false;
	}

	m_file.seek(m_entries[m_curFile].dataOffset + m_fileOffset);
	return true;
}

size_t LabArchive::getLocInFile()
{
	return m_fileOffset;
}

// Directory
u32 LabArchive::getFileCount()
{
	if (!m_archiveOpen) { return 0; }
	return m_header.fileCount;
}

const char* LabArchive::getFileName(u32 index)
{
	if (!m_archiveOpen) { return nullptr; }
	return &m_stringTable[m_entries[index].nameOffset];
}

size_t LabArchive::getFileLength(u32 index)
{
	if (!m_archiveOpen) { return 0; }
	return m_entries[index].len;
}

// Edit
void LabArchive::addFile(const char* fileName, const char* filePath)
{
	// STUB: No ability to add files yet.
	assert(0);
}

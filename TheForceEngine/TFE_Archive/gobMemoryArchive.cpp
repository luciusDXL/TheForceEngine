#include <cstring>

#include "gobMemoryArchive.h"
#include <TFE_System/system.h>
#include <TFE_Game/igame.h>
#include <assert.h>
#include <algorithm>

GobMemoryArchive::~GobMemoryArchive()
{
	close();
}

bool GobMemoryArchive::create(const char *archivePath)
{
	// STUB
	return false;
}

bool GobMemoryArchive::open(const char *archivePath)
{
	return false;
}

bool GobMemoryArchive::open(const u8* buffer, size_t size)
{
	if (!buffer || !size) { return false; }

	m_buffer = buffer;
	if (!m_buffer) { return false; }

	m_size    = size;
	m_readLoc = 0;

	const u8* readBuffer = m_buffer;
	m_header   = (GobArchive::GOB_Header_t*)readBuffer;
	readBuffer = m_buffer + m_header->MASTERX;

	m_fileList.MASTERN = *((long*)readBuffer); readBuffer += sizeof(long);
	m_fileList.entries = (GobArchive::GOB_Entry_t*)(readBuffer);

	m_archiveOpen = true;

	return true;
}

void GobMemoryArchive::close()
{
	m_archiveOpen = false;
	free((void*)m_buffer);
	m_buffer = nullptr;
}

// File Access
bool GobMemoryArchive::openFile(const char *file)
{
	if (!m_archiveOpen) { return false; }

	m_curFile = -1;
	m_fileOffset = 0;

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
		TFE_System::logWrite(LOG_ERROR, "GOB", "Failed to load \"%s\" from \"%s\"", file, m_archivePath);
	}
	else
	{
		m_readLoc = m_fileList.entries[m_curFile].IX;
	}
	return m_curFile > -1 ? true : false;
}

bool GobMemoryArchive::openFile(u32 index)
{
	if (index >= getFileCount()) { return false; }

	m_curFile = s32(index);
	m_fileOffset = 0;
	m_readLoc = m_fileList.entries[m_curFile].IX;
	return true;
}

void GobMemoryArchive::closeFile()
{
	m_curFile = -1;
	m_readLoc = 0;
}

u32 GobMemoryArchive::getFileIndex(const char* file)
{
	if (!m_archiveOpen) { return INVALID_FILE; }

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

bool GobMemoryArchive::fileExists(const char *file)
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

bool GobMemoryArchive::fileExists(u32 index)
{
	if (index >= getFileCount()) { return false; }
	return true;
}

size_t GobMemoryArchive::getFileLength()
{
	if (m_curFile < 0) { return 0; }
	return getFileLength(m_curFile);
}

size_t GobMemoryArchive::readFile(void *data, size_t size)
{
	if (m_curFile < 0) { return false; }
	if (size == 0) { size = m_fileList.entries[m_curFile].LEN; }
	const size_t sizeToRead = std::min(size, (size_t)m_fileList.entries[m_curFile].LEN);

	memcpy(data, m_buffer + m_readLoc, sizeToRead);
	m_readLoc += sizeToRead;
	m_fileOffset += (s32)sizeToRead;
	return sizeToRead;
}

bool GobMemoryArchive::seekFile(s32 offset, s32 origin)
{
	if (m_curFile < 0) { return false; }
	size_t size = m_fileList.entries[m_curFile].LEN;

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

	m_readLoc = m_fileList.entries[m_curFile].IX + m_fileOffset;
	return true;
}

size_t GobMemoryArchive::getLocInFile()
{
	return m_fileOffset;
}

// Directory
u32 GobMemoryArchive::getFileCount()
{
	if (!m_archiveOpen) { return 0; }
	return (u32)m_fileList.MASTERN;
}

const char* GobMemoryArchive::getFileName(u32 index)
{
	if (!m_archiveOpen) { return nullptr; }
	return m_fileList.entries[index].NAME;
}

size_t GobMemoryArchive::getFileLength(u32 index)
{
	if (!m_archiveOpen) { return 0; }
	return m_fileList.entries[index].LEN;
}

// Edit
void GobMemoryArchive::addFile(const char* fileName, const char* filePath)
{
	// STUB
}

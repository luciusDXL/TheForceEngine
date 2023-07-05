#include <cstring>

#include <TFE_System/system.h>
#include <TFE_System/endian.h>
#include "lfdArchive.h"
#include <assert.h>
#include <algorithm>

LfdArchive::~LfdArchive()
{
	close();
}

bool LfdArchive::create(const char *archivePath)
{
	m_archiveOpen = m_file.open(archivePath, Stream::MODE_WRITE);
	m_curFile = -1;
	m_fileOffset = 0;
	if (!m_archiveOpen) { return false; }

	// Write the directory.
	LFD_Entry_t root = { 0 };
	m_file.writeBuffer(&root, sizeof(LFD_Entry_t));
	m_fileList.MASTERN = 0;
	m_fileList.entries = nullptr;

	strcpy(m_archivePath, archivePath);
	m_file.close();

	return true;
}

bool LfdArchive::open(const char *archivePath)
{
	m_archiveOpen = m_file.open(archivePath, Stream::MODE_READ);
	m_curFile = -1;
	m_fileOffset = 0;
	if (!m_archiveOpen) { return false; }

	// Read the directory.
	LFD_Entry_t root, entry;
	m_file.readBuffer(&root, sizeof(LFD_Entry_t));
	root.LENGTH = TFE_Endian::le32_to_cpu(root.LENGTH);
	m_fileList.MASTERN = root.LENGTH / sizeof(LFD_Entry_t);
	m_fileList.entries = new LFD_EntryFinal_t[m_fileList.MASTERN];

	s32 IX = sizeof(LFD_Entry_t) + root.LENGTH;
	for (s32 i = 0; i < m_fileList.MASTERN; i++)
	{
		m_file.readBuffer(&entry, sizeof(LFD_Entry_t));
		entry.LENGTH = TFE_Endian::le32_to_cpu(entry.LENGTH);

		char name[9] = { 0 };
		char ext[5]  = { 0 };
		memcpy(name, entry.NAME, 8);
		memcpy(ext, entry.TYPE, 4);
		ext[4] = 0;

		sprintf(m_fileList.entries[i].NAME, "%s.%s", name, ext);
		m_fileList.entries[i].LENGTH = entry.LENGTH;
		m_fileList.entries[i].IX = IX + sizeof(LFD_Entry_t);

		IX += sizeof(LFD_Entry_t) + entry.LENGTH;
	}

	strcpy(m_archivePath, archivePath);
	m_file.close();

	return true;
}

void LfdArchive::close()
{
	m_file.close();
	m_archiveOpen = false;

	if (m_fileList.entries)
	{
		delete[] m_fileList.entries;
		m_fileList.entries = nullptr;
	}
}

// File Access
bool LfdArchive::openFile(const char *file)
{
	if (!m_archiveOpen) { return false; }

	m_file.open(m_archivePath, Stream::MODE_READ);
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
		m_file.close();
		TFE_System::logWrite(LOG_ERROR, "LFD", "Failed to load \"%s\" from \"%s\"", file, m_archivePath);
	}
	else
	{
		m_file.seek(m_fileList.entries[m_curFile].IX);
	}
	return m_curFile > -1 ? true : false;
}

bool LfdArchive::openFile(u32 index)
{
	if (index >= getFileCount()) { return false; }

	m_curFile = s32(index);
	m_fileOffset = 0;
	m_file.open(m_archivePath, Stream::MODE_READ);
	m_file.seek(m_fileList.entries[m_curFile].IX);
	return true;
}

void LfdArchive::closeFile()
{
	m_curFile = -1;
	m_file.close();
}

u32 LfdArchive::getFileIndex(const char* file)
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

bool LfdArchive::fileExists(const char *file)
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

bool LfdArchive::fileExists(u32 index)
{
	if (index >= getFileCount()) { return false; }
	return true;
}

size_t LfdArchive::getFileLength()
{
	if (m_curFile < 0) { return 0; }
	return getFileLength(m_curFile);
}

size_t LfdArchive::readFile(void *data, size_t size)
{
	if (m_curFile < 0) { return false; }
	if (size == 0) { size = m_fileList.entries[m_curFile].LENGTH; }
	const size_t sizeToRead = std::min(size, (size_t)m_fileList.entries[m_curFile].LENGTH);

	size_t bytesRead = m_file.readBuffer(data, (u32)sizeToRead);
	m_fileOffset += (s32)sizeToRead;
	return bytesRead;
}

bool LfdArchive::seekFile(s32 offset, s32 origin)
{
	if (m_curFile < 0) { return false; }
	size_t size = m_fileList.entries[m_curFile].LENGTH;

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

	m_file.seek(m_fileList.entries[m_curFile].IX + m_fileOffset);
	return true;
}

size_t LfdArchive::getLocInFile()
{
	return m_fileOffset;
}

// Directory
u32 LfdArchive::getFileCount()
{
	if (!m_archiveOpen) { return 0; }
	return (u32)m_fileList.MASTERN;
}

const char* LfdArchive::getFileName(u32 index)
{
	if (!m_archiveOpen) { return nullptr; }
	return m_fileList.entries[index].NAME;
}

size_t LfdArchive::getFileLength(u32 index)
{
	if (!m_archiveOpen) { return 0; }
	return m_fileList.entries[index].LENGTH;
}

// Edit
void LfdArchive::addFile(const char* fileName, const char* filePath)
{

}

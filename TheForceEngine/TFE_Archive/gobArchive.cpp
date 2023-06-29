#include <cstring>

#include <TFE_System/system.h>
#include <TFE_System/endian.h>
#include "gobArchive.h"
#include <assert.h>
#include <algorithm>
#include <vector>

GobArchive::~GobArchive()
{
	close();
}

bool GobArchive::create(const char *archivePath)
{
	m_archiveOpen = m_file.open(archivePath, Stream::MODE_WRITE);
	m_curFile = -1;
	m_fileOffset = 0;
	if (!m_archiveOpen) { return false; }

	memset(&m_header, 0, sizeof(GOB_Header_t));
	m_header.GOB_MAGIC[0] = 'G';
	m_header.GOB_MAGIC[1] = 'O';
	m_header.GOB_MAGIC[2] = 'B';
	m_header.GOB_MAGIC[3] = '\n';
	m_header.MASTERX = sizeof(GOB_Header_t);
	
	m_fileList.MASTERN = 0;
	m_fileList.entries = nullptr;

	m_file.writeBuffer(&m_header, sizeof(GOB_Header_t));
	m_file.writeBuffer(&m_fileList.MASTERN, sizeof(u32));

	strcpy(m_archivePath, archivePath);
	m_file.close();

	return true;
}

bool GobArchive::validate(const char *archivePath, s32 minFileCount)
{
	FileStream file;
	if (!file.open(archivePath, Stream::MODE_READ))
	{
		return false;
	}

	// Read the directory.
	GOB_Header_t header;
	long MASTERN;
	if (file.readBuffer(&header, sizeof(GOB_Header_t)) != sizeof(GOB_Header_t))
	{
		file.close();
		return false;
	}
	header.MASTERX = TFE_Endian::swapLE32(header.MASTERX);
	file.seek(header.MASTERX);
	file.readBuffer(&MASTERN, sizeof(long));
	MASTERN = TFE_Endian::swapLE32(MASTERN);
	file.close();

	return MASTERN >= minFileCount;
}

bool GobArchive::open(const char *archivePath)
{
	m_archiveOpen = m_file.open(archivePath, Stream::MODE_READ);
	m_curFile = -1;
	if (!m_archiveOpen) { return false; }

	// Read the directory.
	m_file.readBuffer(&m_header, sizeof(GOB_Header_t));
	m_header.MASTERX = TFE_Endian::swapLE32(m_header.MASTERX);
	m_file.seek(m_header.MASTERX);

	m_file.readBuffer(&m_fileList.MASTERN, sizeof(u32));
	m_fileList.MASTERN = TFE_Endian::swapLE32(m_fileList.MASTERN);
	m_fileList.entries = new GOB_Entry_t[m_fileList.MASTERN];
	m_file.readBuffer(m_fileList.entries, sizeof(GOB_Entry_t), m_fileList.MASTERN);
	for (s32 i = 0; i < m_fileList.MASTERN; i++)
	{
		m_fileList.entries[i].IX = TFE_Endian::swapLE32(m_fileList.entries[i].IX);
		m_fileList.entries[i].LEN = TFE_Endian::swapLE32(m_fileList.entries[i].LEN);
	}

	strcpy(m_archivePath, archivePath);
	m_file.close();

	return true;
}

void GobArchive::close()
{
	m_file.close();
	m_archiveOpen = false;
	delete[] m_fileList.entries;
	m_fileList.entries = nullptr;
}

// File Access
bool GobArchive::openFile(const char *file)
{
	if (!m_archiveOpen) { return false; }

	m_file.open(m_archivePath, Stream::MODE_READ);
	m_curFile = -1;
	m_fileOffset = 0;

	//search for this file.
	for (u32 i = 0; i < m_fileList.MASTERN; i++)
	{
		if (strcasecmp(file, m_fileList.entries[i].NAME) == 0)
		{
			m_curFile = s32(i);
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
		m_file.seek(m_fileList.entries[m_curFile].IX);
	}
	return m_curFile > -1 ? true : false;
}

bool GobArchive::openFile(u32 index)
{
	if (index >= getFileCount()) { return false; }

	m_curFile = s32(index);
	m_fileOffset = 0;
	m_file.open(m_archivePath, Stream::MODE_READ);
	m_file.seek(m_fileList.entries[m_curFile].IX);
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

	//search for this file.
	for (u32 i = 0; i < m_fileList.MASTERN; i++)
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
	for (u32 i = 0; i < m_fileList.MASTERN; i++)
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

size_t GobArchive::readFile(void *data, size_t size)
{
	if (m_curFile < 0) { return false; }
	if (size == 0) { size = m_fileList.entries[m_curFile].LEN; }
	const size_t sizeToRead = std::min(size, (size_t)m_fileList.entries[m_curFile].LEN);

	u32 bytesRead = m_file.readBuffer(data, (u32)sizeToRead);
	m_fileOffset += (s32)sizeToRead;
	return bytesRead;
}

bool GobArchive::seekFile(s32 offset, s32 origin)
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

	m_file.seek(m_fileList.entries[m_curFile].IX + m_fileOffset);
	return true;
}

size_t GobArchive::getLocInFile()
{
	return m_fileOffset;
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

// Edit
void GobArchive::addFile(const char* fileName, const char* filePath)
{
	FileStream file;
	if (!file.open(filePath, Stream::MODE_READ))
	{
		return;
	}
	const size_t len = file.getSize();
	const u32 newId = m_fileList.MASTERN;
	m_fileList.MASTERN++;
	GOB_Entry_t* newEntries = new GOB_Entry_t[m_fileList.MASTERN];
	memcpy(newEntries, m_fileList.entries, sizeof(GOB_Entry_t) * newId);
	m_fileList.entries = newEntries;

	GOB_Entry_t* newFile = &m_fileList.entries[newId];
	memset(newFile, 0, sizeof(GOB_Entry_t));
	newFile->IX  = newId > 0 ? m_fileList.entries[newId - 1].IX + m_fileList.entries[newId - 1].LEN : sizeof(GOB_Header_t);
	newFile->LEN = u32(len);
	strcpy(newFile->NAME, fileName);
	m_header.MASTERX += newFile->LEN;

	// Read all of the file data.
	std::vector<std::vector<u8>> fileData(m_fileList.MASTERN);
	if (m_file.open(m_archivePath, Stream::MODE_READ) && m_fileList.MASTERN >= 1)
	{
		for (u32 f = 0; f < m_fileList.MASTERN - 1; f++)
		{
			fileData[f].resize(m_fileList.entries[f].LEN);
			m_file.seek(m_fileList.entries[f].IX);
			m_file.readBuffer(fileData[f].data(), m_fileList.entries[f].LEN);
		}
		m_file.close();

		fileData[newId].resize(len);
		file.readBuffer(fileData[newId].data(), (u32)len);
		file.close();
	}

	// Now write the new file.
	if (m_file.open(m_archivePath, Stream::MODE_WRITE))
	{
		m_file.writeBuffer(&m_header, sizeof(GOB_Header_t));
		for (u32 f = 0; f < m_fileList.MASTERN; f++)
		{
			m_file.writeBuffer(fileData[f].data(), m_fileList.entries[f].LEN);
		}
		assert(m_file.getLoc() == m_header.MASTERX);
		m_file.writeBuffer(&m_fileList.MASTERN, sizeof(u32));
		m_file.writeBuffer(m_fileList.entries, sizeof(GOB_Entry_t), m_fileList.MASTERN);
		m_file.close();
	}
}

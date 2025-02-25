#include "zipArchive.h"
#include <TFE_FileSystem/fileutil.h>
#include <TFE_System/system.h>
#include "zip/zip.h"
#include <assert.h>
#include <string>
#include <algorithm>
#include <map>

namespace
{
	const size_t c_blockShift = 8;	// 256 bytes.
	const size_t c_blockSize = 1 << c_blockShift;

	size_t roundBufferSize(size_t size)
	{
		size = (size + c_blockSize - 1) >> c_blockShift;
		return size << c_blockShift;
	}
}

ZipArchive::~ZipArchive()
{
	free(m_tempBuffer);
	m_tempBuffer = nullptr;
	m_tempBufferSize = 0;

	close();
}

bool ZipArchive::create(const char *archivePath)
{
	m_curFile = INVALID_FILE;
	m_entryCount = 0;
	m_fileOffset = 0;
	m_tempBuffer = nullptr;
	m_tempBufferSize = 0;
	m_newFiles.clear();

	strcpy(m_archivePath, archivePath);
	m_fileHandle = nullptr;

	return true;
}

bool ZipArchive::open(const char *archivePath)
{
	m_curFile = INVALID_FILE;
	m_entryCount = 0;
	m_fileOffset = 0;
	m_tempBuffer = nullptr;
	m_tempBufferSize = 0;

	struct zip_t* zip = zip_open(archivePath, 0, 'r');
	if (!zip)
	{
		TFE_System::logWrite(LOG_ERROR, "zipArchive", "Cannot open Zip Archive '%s'", archivePath);
		return false;
	}

	// Read the directory.
	m_entryCount = zip_total_entries(zip);
	if (m_entryCount <= 0)
	{
		TFE_System::logWrite(LOG_ERROR, "zipArchive", "Zip Archive '%s' is empty.", archivePath);
		zip_close(zip);
		return false;
	}
	m_entries = new ZipEntry[m_entryCount];

	for (s32 i = 0; i < m_entryCount; i++)
	{
		if (zip_entry_openbyindex(zip, i) != 0)
		{
			TFE_System::logWrite(LOG_ERROR, "zipArchive", "Cannot read entry '%d' from archive '%s'", i, archivePath);
			zip_close(zip);
			return false;
		}

		m_entries[i].isDir = (zip_entry_isdir(zip) == 1);
		m_entries[i].name = zip_entry_name(zip);
		m_entries[i].length = (size_t)zip_entry_size(zip);
		zip_entry_close(zip);
	}
	zip_close(zip);

	strcpy(m_archivePath, archivePath);
	m_fileHandle = nullptr;

	return true;
}

void ZipArchive::close()
{
	// Flush new files.
	if (!m_newFiles.empty())
	{
		const size_t fileCount = m_newFiles.size();
		std::vector<const char*> filePaths;
		for (size_t i = 0; i < fileCount; i++)
		{
			filePaths.push_back(m_newFiles[i].path.c_str());
		}
		zip_create(m_archivePath, filePaths.data(), fileCount);
		m_newFiles.clear();
	}

	closeFile();

	delete[] m_entries;
	m_entries = nullptr;
	m_curFile = INVALID_FILE;
}

// File Access
bool ZipArchive::openFile(const char *file)
{
	m_curFile = getFileIndex(file);
	m_fileOffset = 0;
	if (m_curFile != INVALID_FILE)
	{
		m_fileHandle = zip_open(m_archivePath, 0, 'r');
		if (zip_entry_openbyindex((struct zip_t*)m_fileHandle, m_curFile) != 0)
		{
			TFE_System::logWrite(LOG_ERROR, "zipArchive", "Cannot open file '%s' from archive '%s'", file, m_archivePath);
			m_curFile = INVALID_FILE;
			zip_close((struct zip_t*)m_fileHandle);
			m_fileHandle = nullptr;
		}
	}

	// Make sure our temp buffer is large enough to hold the entry.
	if (m_curFile != INVALID_FILE && m_tempBufferSize < m_entries[m_curFile].length)
	{
		m_tempBufferSize = roundBufferSize(m_entries[m_curFile].length);
		m_tempBuffer = (u8*)realloc(m_tempBuffer, m_tempBufferSize);
	}
	m_entryRead = false;

	return m_curFile != INVALID_FILE;
}

bool ZipArchive::openFile(u32 index)
{
	m_curFile = INVALID_FILE;
	m_fileOffset = 0;
	if (index <= (u32)m_entryCount)
	{
		// Open the system file.
		m_fileHandle = zip_open(m_archivePath, 0, 'r');
		// Open the file entry itself.
		m_curFile = index;
		if (zip_entry_openbyindex((struct zip_t*)m_fileHandle, index) != 0)
		{
			TFE_System::logWrite(LOG_ERROR, "zipArchive", "Cannot open file '%s' from archive '%s'", m_entries[index].name.c_str(), m_archivePath);
			m_curFile = INVALID_FILE;
			zip_close((struct zip_t*)m_fileHandle);
			m_fileHandle = nullptr;
		}
	}

	// Make sure our temp buffer is large enough to hold the entry.
	if (m_curFile != INVALID_FILE && m_tempBufferSize < m_entries[m_curFile].length)
	{
		m_tempBufferSize = roundBufferSize(m_entries[m_curFile].length);
		m_tempBuffer = (u8*)realloc(m_tempBuffer, m_tempBufferSize);
	}
	m_entryRead = false;

	return m_curFile != INVALID_FILE;
}

void ZipArchive::closeFile()
{
	if (m_fileHandle)
	{
		// Close the file entry.
		zip_entry_close((struct zip_t*)m_fileHandle);
		// Close the system file.
		zip_close((struct zip_t*)m_fileHandle);
		m_fileHandle = nullptr;
	}
	m_curFile = INVALID_FILE;
}

bool ZipArchive::fileExists(const char *file)
{
	return getFileIndex(file) != INVALID_FILE;
}

bool ZipArchive::fileExists(u32 index)
{
	return index < (u32)m_entryCount;
}

u32 ZipArchive::getFileIndex(const char* file)
{
	for (s32 i = 0; i < m_entryCount; i++)
	{
		if (strcasecmp(file, m_entries[i].name.c_str()) == 0)
		{
			return i;
		}
	}
	return INVALID_FILE;
}

size_t ZipArchive::getFileLength()
{
	if (m_curFile == INVALID_FILE) { return 0; }
	return m_entries[m_curFile].length;
}

size_t ZipArchive::readFile(void* data, size_t size)
{
	if (m_curFile == INVALID_FILE) { return 0u; }
	if (size == 0) { size = m_entries[m_curFile].length; }

	const size_t sizeToRead = std::min(size, m_entries[m_curFile].length);
	// The fast path is to just read the entire entry into the provided memory, avoiding the extra memcopy.
	// This is only done if we are reading the entire file and there is no offset.
	if (m_fileOffset == 0 && sizeToRead == m_entries[m_curFile].length)
	{
		m_fileOffset += (s32)sizeToRead;
		s64 actualSizeRead = zip_entry_noallocread((struct zip_t*)m_fileHandle, data, sizeToRead);
		if (actualSizeRead <= 0)
		{
			return 0u;
		}
		return size_t(actualSizeRead);
	}

	// Otherwise go through the slower path - a one time decompression and read, followed
	// by memcopying the data into the output as needed.
	if (!m_entryRead)
	{
		// Read the whole entry into temporary memory.
		assert(m_tempBufferSize >= m_entries[m_curFile].length);
		if (zip_entry_noallocread((struct zip_t*)m_fileHandle, m_tempBuffer, m_entries[m_curFile].length) <= 0)
		{
			return 0u;
		}
		m_entryRead = true;
	}
	// Then copy the section we want into the output.
	memcpy(data, m_tempBuffer + m_fileOffset, sizeToRead);
	m_fileOffset += (s32)sizeToRead;
	return sizeToRead;
}

bool ZipArchive::seekFile(s32 offset, s32 origin)
{
	if (m_curFile < 0) { return false; }
	size_t size = m_entries[m_curFile].length;

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
	return true;
}

size_t ZipArchive::getLocInFile()
{
	return m_fileOffset;
}

// Directory
u32 ZipArchive::getFileCount()
{
	return m_entryCount;
}

const char* ZipArchive::getFileName(u32 index)
{
	if (index >= (u32)m_entryCount) { return nullptr; }
	return m_entries[index].name.c_str();
}

size_t ZipArchive::getFileLength(u32 index)
{
	if (index >= (u32)m_entryCount) { return 0; }
	return m_entries[index].length;
}

// Edit
void ZipArchive::addFile(const char* fileName, const char* filePath)
{
	m_newFiles.push_back({fileName, filePath, false});
}
#pragma once
#include "filestream.h"
#include <TFE_Archive/archive.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

//Work buffers for handling special cases like std::string without allocating memory (beyond what the strings needs itself).
static u32  s_workBufferU32[1024];		//4k buffer.
static char s_workBufferChar[32768];	//32k buffer.

FileStream::FileStream() : Stream()
{
	m_file = NULL;
	m_mode = MODE_INVALID;
}

FileStream::~FileStream()
{
	close();
}

bool FileStream::exists(const char* filename)
{
	bool res = open(filename, MODE_READ);
	close();

	return res;
}

bool FileStream::open(const char* filename, FileMode mode)
{
	const char* modeStrings[] = { "rb", "wb", "rb+" };
	m_file = fopen(filename, modeStrings[mode]);
	m_mode = mode;

	return m_file != nullptr;
}

bool FileStream::open(const FilePath* filePath, FileMode mode)
{
	if (filePath->archive)
	{
		if (filePath->index < 0) { return false; }
		m_mode = mode;
		m_file = nullptr;
		m_archive = filePath->archive;

		return filePath->archive->openFile(filePath->index);
	}
	else
	{
		return open(filePath->path, mode);
	}
}

void FileStream::close()
{
	if (m_file)
	{
		if (m_mode == MODE_WRITE || m_mode == MODE_READWRITE) 
		{ 
			fflush(m_file); 
		}

		fclose(m_file);
		m_file = nullptr;
	}
	else if (m_archive)
	{
		m_archive->closeFile();
		m_archive = nullptr;
	}
	m_mode = MODE_INVALID;
}

u32 FileStream::readContents(const char* filePath, void** output)
{
	assert(output);

	u32 size = 0;
	FileStream file;
	if (file.open(filePath, MODE_READ))
	{
		size = file.getSize();
		*output = realloc(*output, size + 1);
		file.readBuffer(*output, size);
		file.close();
	}
	return size;
}

//derived from Stream
void FileStream::seek(u32 offset, Origin origin/*=ORIGIN_START*/)
{
	const s32 forigin[] = { SEEK_SET, SEEK_END, SEEK_CUR };
	if (m_file)
	{
		fseek(m_file, offset, forigin[origin]);
	}
	else if (m_archive)
	{
		// TODO: implement seek functionality.
	}
}

size_t FileStream::getLoc()
{
	if (!m_file && !m_archive)
	{
		return 0;
	}
	if (m_file)
	{
		return ftell(m_file);
	}
	// TODO: implement.
	return 0;
}

size_t FileStream::getSize()
{
	if (!m_file && !m_archive)
	{
		return 0;
	}

	size_t filesize = 0;
	if (m_file)
	{
		seek(0, FileStream::ORIGIN_END);
		filesize = getLoc();
		seek(0, FileStream::ORIGIN_START);
	}
	else
	{
		filesize = m_archive->getFileLength();
	}

	return filesize;
}

bool FileStream::isOpen() const
{
	return m_file!=NULL;
}

void FileStream::readBuffer(void* ptr, u32 size, u32 count)
{
	assert(m_mode == MODE_READ || m_mode == MODE_READWRITE);
	fread(ptr, size, count, m_file);
}

void FileStream::writeBuffer(const void* ptr, u32 size, u32 count)
{
	assert(m_mode == MODE_WRITE || m_mode == MODE_READWRITE);
	fwrite(ptr, size, count, m_file);
}

void FileStream::writeString(const char* fmt, ...)
{
	static char tmpStr[4096];
	assert(m_mode == MODE_WRITE || m_mode == MODE_READWRITE);

	va_list arg;
	va_start(arg, fmt);
	vsprintf(tmpStr, fmt, arg);
	va_end(arg);

	const size_t len = strlen(tmpStr);
	fwrite(tmpStr, len, 1, m_file);
}

void FileStream::flush()
{
	assert(m_file);
	fflush(m_file);
}

//internal
template <>	//template specialization for the string type since it has to be handled differently.
void FileStream::readType<std::string>(std::string* ptr, u32 count)
{
	assert(m_mode == MODE_READ || m_mode == MODE_READWRITE);
	assert(count <= 256);
	//first read the length.
	fread(s_workBufferU32, sizeof(u32), count, m_file);

	//then read the string data.
	for (u32 s=0; s<count; s++)
	{
		assert(s_workBufferU32[s] <= 32768);
		fread(s_workBufferChar, 1, s_workBufferU32[s], m_file);
		s_workBufferChar[ s_workBufferU32[s] ] = 0;

		ptr[s] = s_workBufferChar;
	}
}

template <typename T>
void FileStream::readType(T* ptr, u32 count)
{
	assert(m_mode == MODE_READ || m_mode == MODE_READWRITE);
	fread(ptr, sizeof(T), count, m_file);
}

template <>	//template specialization for the string type since it has to be handled differently.
void FileStream::writeType<std::string>(const std::string* ptr, u32 count)
{
	assert(m_mode == MODE_WRITE || m_mode == MODE_READWRITE);
	assert(count <= 256);
	//first read the length.
	for (u32 s=0; s<count; s++)
	{
		s_workBufferU32[s] = (u32)ptr[s].length();
	}
	fwrite(s_workBufferU32, sizeof(u32), count, m_file);

	//then write the string data.
	for (u32 s=0; s<count; s++)
	{
		fwrite(ptr[s].data(), 1, s_workBufferU32[s], m_file);
	}
}

template <typename T>
void FileStream::writeType(const T* ptr, u32 count)
{
	assert(m_mode == MODE_WRITE || m_mode == MODE_READWRITE);
	fwrite(ptr, sizeof(T), count, m_file);
}

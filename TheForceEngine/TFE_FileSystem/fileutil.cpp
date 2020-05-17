#pragma once
#include "fileutil.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>

#ifdef _WIN32
	#include <Windows.h>
#endif

namespace FileUtil
{
	void readDirectory(const char* dir, const char* ext, FileList& fileList)
	{
		char searchStr[512];
		_finddata_t cmfFile;

		sprintf(searchStr, "%s*.%s", dir, ext);
		intptr_t hFile = _findfirst(searchStr, &cmfFile);
		if (hFile != -1)
		{
			do
			{
				fileList.push_back( string(cmfFile.name) );
			} while ( _findnext(hFile, &cmfFile) == 0 );
			_findclose(hFile);
		}
	}

	void readSubdirectories(const char* dir, FileList& dirList)
	{
		#ifdef _WIN32
			static char baseDir[4096];
			strcpy(baseDir, dir);

			size_t len = strlen(baseDir);
			baseDir[len] = '*';
			baseDir[len+1] = 0;

			WIN32_FIND_DATAA fi;
			HANDLE h = FindFirstFileExA(baseDir, FindExInfoStandard, &fi, FindExSearchLimitToDirectories, NULL, 0);
			if (h != INVALID_HANDLE_VALUE) 
			{
				do 
				{
					if (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
					{
						string name = string(fi.cFileName);
						if (name != "." && name != "..")
						{
							dirList.push_back( dir+name+"/" );
						}
					}
				} while (FindNextFileA(h, &fi));
				FindClose(h);
			}
		#else
	
		#endif
	}

	bool makeDirectory(const char* dir)
	{
	#ifdef _WIN32
		if (CreateDirectoryA(dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			return true;
		}
	#endif

		return false;
	}

	void getCurrentDirectory(char* dir)
	{
		GetCurrentDirectoryA(512, dir);
	}

	void getFilePath(const char* filename, char* path)
	{
		s32 lastSlash = -1;
		s32 firstNonSpace = -1;
		s32 lastNonSpace = -1;
		u32 len = (u32)strlen(filename);

		for (u32 c=0; c<len; c++)
		{
			if (filename[c] == '\\' || filename[c] == '/')
			{
				lastSlash = c;
			}
			if (firstNonSpace < 0 && filename[c] != ' ')
			{
				firstNonSpace = c;
			}
			else if (filename[c] != ' ')
			{
				lastNonSpace = c;
			}
		}

		if (lastSlash >= 0)
		{
			s32 start = 0;
			s32 end   = lastSlash;

			for (s32 c=0; c<=end; c++)
			{
				path[c] = filename[c];
			}
			path[end+1] = 0;
		}
		else
		{
			path[0] = 0;
		}
	}

	void getFileExtension(const char* filename, char* extension)
	{
		assert(filename && extension);

		s32 len = (s32)strlen(filename);
		//search for the last '.'
		s32 lastPeriod = -1;
		for (s32 c = 0; c < len; c++)
		{
			if (filename[c] == '.')
			{
				lastPeriod = c;
			}
		}

		if (lastPeriod < 0) { extension[0] = 0; return; }
		for (s32 c = lastPeriod + 1; c < len; c++)
		{
			extension[c - lastPeriod - 1] = filename[c];
		}
		extension[len - lastPeriod - 1] = 0;
	}

	void getFileNameFromPath(const char* path, char* name, bool includeExt)
	{
		s32 lastSlash = -1;
		s32 firstNonSpace = -1;
		s32 lastNonSpace = -1;
		s32 lastPeriod = -1;
		u32 len = (u32)strlen(path);

		for (u32 c=0; c<len; c++)
		{
			if (path[c] == '\\' || path[c] == '/')
			{
				lastSlash = c;
			}
			if (firstNonSpace < 0 && path[c] != ' ')
			{
				firstNonSpace = c;
			}
			else if (path[c] != ' ')
			{
				lastNonSpace = c;
			}
			if (path[c] == '.')
			{
				lastPeriod = c;
			}
		}

		s32 start = max( lastSlash+1, firstNonSpace );
		s32 end   = len-1;
		if (lastPeriod >= 0 && !includeExt)
		{
			end = lastPeriod-1;
		}
		else if (lastNonSpace >= 0)
		{
			end = lastNonSpace;
		}

		s32 c=start;
		for (; c<=end; c++)
		{
			name[c-start] = path[c];
		}
		name[c-start] = 0;
	}

	void copyFile(const char* srcFile, const char* dstFile)
	{
		CopyFile(srcFile, dstFile, FALSE);
	}

	void deleteFile(const char* srcFile)
	{
		DeleteFile(srcFile);
	}

	bool directoryExits(const char* path)
	{
		DWORD attr = GetFileAttributesA(path);
		if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) { return false; }
		return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	bool exists( const char *path )
	{
		return !(GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES && GetLastError()==ERROR_FILE_NOT_FOUND);
	}

	u64 getModifiedTime( const char* path )
	{
		FILETIME creationTime;
		FILETIME lastAccessTime;
		FILETIME lastWriteTime;

		HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (fileHandle == INVALID_HANDLE_VALUE)
		{
			return 0;
		}

		u64 modTime = 0;
		if ( GetFileTime(fileHandle, &creationTime, &lastAccessTime, &lastWriteTime) )
		{
			modTime = u64(lastWriteTime.dwHighDateTime) << 32ULL | u64(lastWriteTime.dwLowDateTime);
		}
		
		CloseHandle(fileHandle);

		return modTime;
	}
}

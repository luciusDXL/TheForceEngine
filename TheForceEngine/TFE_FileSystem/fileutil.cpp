#include <cerrno>
#include <cstring>
#include <ctime>

#include "fileutil.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
	#include <Windows.h>
	#include <io.h>
#else
	#include <sys/stat.h>
	#include <dirent.h>
	#include <unistd.h>
#endif

namespace FileUtil
{
	void readDirectory(const char* dir, const char* ext, FileList& fileList)
	{
		#ifdef _WIN32
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
		#else
			DIR *baseDir;
			struct dirent *currentDir;

			baseDir = opendir(dir);

			while ((currentDir = readdir(baseDir)) != NULL) {
				const char *currentDirName = currentDir->d_name;
				const size_t extLength = strlen(ext);
				const size_t nameLength = strlen(currentDirName);
				char *fileExt;

				if (!strcmp(currentDirName, ".") || !strcmp(currentDirName, ".."))
					continue;

				if (nameLength < (extLength + 2))
					continue;

				if (currentDirName[nameLength - (extLength + 1)] != '.')
					continue;

				fileExt = (char *) malloc(sizeof (*fileExt) * extLength);
				strncpy(fileExt, &currentDirName[nameLength - extLength], extLength);

				if (!strncmp(fileExt, ext, extLength)) {
					struct stat pathStat;
					size_t pathLength;
					char *pathString;

					pathLength = strlen(dir) + nameLength + 1;
					pathString = (char *) malloc(sizeof (*pathString) * pathLength);

					strcpy(pathString, dir);
					strcat(pathString, currentDirName);

					stat(pathString, &pathStat);

					if (pathStat.st_mode & S_IFREG || pathStat.st_mode & S_IFLNK)
						fileList.push_back(string(pathString));

					free(pathString);
				}

				free(fileExt);
			}

			closedir(baseDir);
		#endif
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
			DIR *baseDir;
			struct dirent *currentDir;

			baseDir = opendir(dir);

			while ((currentDir = readdir(baseDir)) != NULL) {
				const char *currentDirName = currentDir->d_name;
				struct stat pathStat;
				size_t pathLength;
				char *pathString;

				if (!strcmp(currentDirName, ".") || !strcmp(currentDirName, ".."))
					continue;

				pathLength = strlen(dir) + 1 + strlen(currentDirName) + 2;
				pathString = (char *) malloc(sizeof (*pathString) * pathLength);

				strcpy(pathString, dir);
				strcat(pathString, currentDirName);

				stat(pathString, &pathStat);

				if (pathStat.st_mode & S_IFDIR)
					dirList.push_back(string(pathString));

				free(pathString);
			}

			closedir(baseDir);
		#endif
	}

	bool makeDirectory(const char* dir)
	{
	#ifdef _WIN32
		if (CreateDirectoryA(dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			return true;
		}
	#else
		if (!mkdir(dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) || errno == EEXIST)
			return true;
	#endif

		return false;
	}

	void getCurrentDirectory(char* dir)
	{
		#ifdef _WIN32
			GetCurrentDirectoryA(512, dir);
		#else
			getcwd(dir, 512);
		#endif
	}

	void getExecutionDirectory(char* dir)
	{
	#ifdef _WIN32
		GetModuleFileNameA(NULL, dir, MAX_PATH);
	#else
		char modulePath[PATH_MAX];

		sprintf(modulePath, "/proc/%d/exe", getpid());
		readlink(modulePath, dir, PATH_MAX);
	#endif
		size_t len = strlen(dir);
		size_t lastSlash = 0;
		for (size_t i = 0; i < len; i++)
		{
			if (dir[i] == '/' || dir[i] == '\\')
			{
				lastSlash = i;
			}
		}
		dir[lastSlash] = 0;
	}

	void setCurrentDirectory(const char* dir)
	{
		#ifdef _WIN32
			SetCurrentDirectoryA(dir);
		#else
			chdir(dir);
		#endif
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
		#ifdef _WIN32
			CopyFile(srcFile, dstFile, FALSE);
		#else
			FILE *src, *dst;
			char buf[BUFSIZ];

			src = fopen(srcFile, "rb");
			dst = fopen(dstFile, "wb");

			while (!feof(src)) {
				size_t readCount;

				readCount = fread(buf, sizeof (char), BUFSIZ, src);
				fwrite(buf, sizeof (char), readCount, dst);
			}
			fclose(dst);
			fclose(src);
		#endif
	}

	void deleteFile(const char* srcFile)
	{
		#ifdef _WIN32
			DeleteFile(srcFile);
		#else
			unlink(srcFile);
		#endif
	}

	bool directoryExits(const char* path)
	{
		#ifdef _WIN32
			DWORD attr = GetFileAttributesA(path);
			if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) { return false; }
			return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
		#else
			struct stat dirstat;

			stat(path, &dirstat);

			if (errno == ENOENT)
				return false;

			return (dirstat.st_mode & S_IFDIR) != 0;
		#endif
	}

	bool exists( const char *path )
	{
		#ifdef _WIN32
			return !(GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES && GetLastError()==ERROR_FILE_NOT_FOUND);
		#else
			struct stat pathstat;
			int error;

			error = stat(path, &pathstat);

			return error ? errno != ENOENT : true;
		#endif
	}

	u64 getModifiedTime( const char* path )
	{
		u64 modTime = 0;
		#ifdef _WIN32
			FILETIME creationTime;
			FILETIME lastAccessTime;
			FILETIME lastWriteTime;

			HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
			if (fileHandle == INVALID_HANDLE_VALUE)
			{
				return 0;
			}

			if ( GetFileTime(fileHandle, &creationTime, &lastAccessTime, &lastWriteTime) )
			{
				modTime = u64(lastWriteTime.dwHighDateTime) << 32ULL | u64(lastWriteTime.dwLowDateTime);
			}
			
			CloseHandle(fileHandle);
		#else
			struct stat pathStat;
			struct timespec lastWriteTime;

			stat(path, &pathStat);

			lastWriteTime = pathStat.st_mtim;

			modTime = (u64) lastWriteTime.tv_sec * 10000 + (u64) ((double) lastWriteTime.tv_nsec / 100.0);
		#endif

		return modTime;
	}

	void fixupPath(char* path)
	{
		const size_t len = strlen(path);
		for (size_t i = 0; i < len; i++)
		{
			if (path[i] == '\\')
			{
				path[i] = '/';
			}
		}
	}

	void convertToOSPath(const char* path, char* pathOS)
	{
#ifdef _WIN32
		const size_t len = strlen(path);
		for (size_t i = 0; i < len; i++)
		{
			if (path[i] == '/')
			{
				pathOS[i] = '\\';
			}
			else
			{
				pathOS[i] = path[i];
			}
		}
#else
		const size_t len = strlen(path);
		for (size_t i = 0; i < len; i++)
		{
			if (path[i] == '\\')
			{
				pathOS[i] = '/';
			}
			else
			{
				pathOS[i] = path[i];
			}
		}
#endif
		pathOS[len] = 0;
	}
}

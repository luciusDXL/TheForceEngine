#pragma once
#include "paths.h"
#include "fileutil.h"
#include "filestream.h"
#include <TFE_Archive/archive.h>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#ifdef min
#undef min
#undef max
#endif

#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")
#include "shlobj.h"
#endif

namespace TFE_Paths
{
	static std::string s_paths[PATH_COUNT];
	static std::vector<Archive*> s_localArchives;
	static std::vector<std::string> s_searchPaths;

	void setPath(TFE_PathType pathType, const char* path)
	{
		s_paths[pathType] = path;
	}

	bool setProgramDataPath(const char* append)
	{
#ifdef _WIN32
		char path[TFE_MAX_PATH];
		// Get path for each computer, non-user specific and non-roaming data.
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path)))
		{
			// Append product-specific path
			PathAppend(path, append);

			// Only setup the path if successful.
			s_paths[PATH_PROGRAM_DATA] = path;
			s_paths[PATH_PROGRAM_DATA] += "\\";

			FileUtil::makeDirectory(path);
			return true;
		}
		else
		{
			// If getting the program data folder fails, then revert back to using
			// the program path if possible.
			s_paths[PATH_PROGRAM_DATA] = s_paths[PATH_PROGRAM];
			return !s_paths[PATH_PROGRAM].empty();
		}
#endif
		return false;
	}

	bool setUserDocumentsPath(const char* append)
	{
#ifdef _WIN32
		char path[TFE_MAX_PATH];
		// Get path for each computer, non-user specific and non-roaming data.
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, 0, path)))
		{
			// Append product-specific path
			PathAppend(path, append);

			// Only setup the path if successful.
			s_paths[PATH_USER_DOCUMENTS] = path;
			s_paths[PATH_USER_DOCUMENTS] += "\\";

			FileUtil::makeDirectory(path);
			return true;
		}
		else
		{
			// If getting the documents folder fails, then revert back to using
			// the program path if possible.
			s_paths[PATH_USER_DOCUMENTS] = s_paths[PATH_PROGRAM];
			return !s_paths[PATH_PROGRAM].empty();
		}
#endif
		return false;
	}

	bool setProgramPath()
	{
		char path[TFE_MAX_PATH];
		FileUtil::getCurrentDirectory(path);
		s_paths[PATH_PROGRAM] = path;
		s_paths[PATH_PROGRAM] += "\\";
		return true;
	}

	const char* getPath(TFE_PathType pathType)
	{
		return s_paths[pathType].c_str();
	}

	bool hasPath(TFE_PathType pathType)
	{
		return !s_paths[pathType].empty();
	}

	void appendPath(TFE_PathType pathType, const char* filename, char* path, size_t bufferLen/* = TFE_MAX_PATH*/)
	{
		snprintf(path, bufferLen, "%s%s", getPath(pathType), filename);
	}

	void clearSearchPaths()
	{
		s_localArchives.clear();
		s_searchPaths.clear();
	}

	void addLocalSearchPath(const char* localSearchPath)
	{
		char fullPath[TFE_MAX_PATH];
		snprintf(fullPath, TFE_MAX_PATH, "%s%s", getPath(PATH_SOURCE_DATA), localSearchPath);
		s_searchPaths.push_back(fullPath);
	}

	void addLocalArchive(Archive* archive)
	{
		s_localArchives.push_back(archive);
	}

	// This is the slow first-pass version.
	// I want to make sure the functionality is correct before delving into optimizations.
	bool getFilePath(const char* fileName, FilePath* outPath)
	{
		outPath->archive = nullptr;
		outPath->index = INVALID_FILE;
		outPath->path[0] = 0;

		// Search in the local search paths first: s_searchPaths.
		const size_t pathCount = s_searchPaths.size();
		const std::string* localPath = s_searchPaths.data();
		for (size_t i = 0; i < pathCount; i++, localPath++)
		{
			char fullName[TFE_MAX_PATH];
			sprintf(fullName, "%s%s", localPath->c_str(), fileName);

			FileStream file;
			if (file.exists(fullName))
			{
				strncpy(outPath->path, fullName, TFE_MAX_PATH);
				return true;
			}
		}

		// Then archives: s_localArchives.
		const size_t archiveCount = s_localArchives.size();
		Archive** archive = s_localArchives.data();
		for (size_t i = 0; i < archiveCount; i++, archive++)
		{
			u32 index = (*archive)->getFileIndex(fileName);
			if (index != INVALID_FILE)
			{
				outPath->archive = *archive;
				outPath->index = index;
				return true;
			}
		}

		// Finally admit defeat.
		return false;
	}
}

#pragma once
#include "paths.h"
#include "fileutil.h"
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
}

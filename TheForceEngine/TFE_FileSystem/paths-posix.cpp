#include <cstring>

#include "paths.h"
#include "fileutil.h"
#include "filestream.h"
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>
#include <deque>
#include <string>

namespace TFE_Paths
{
	struct FileMapping
	{
		std::string fileName;
		std::string realPath;
	};

	static std::string s_paths[PATH_COUNT];
	static std::deque<Archive*> s_localArchives;
	static std::deque<std::string> s_searchPaths;
	static std::deque<FileMapping> s_fileMappings;

	void setPath(TFE_PathType pathType, const char* path)
	{
		s_paths[pathType] = path;
	}

	bool setProgramDataPath(const char* append)
	{
		char path[TFE_MAX_PATH];
		char *home = getenv("HOME");
		memset(path, 0, TFE_MAX_PATH);
		snprintf(path, TFE_MAX_PATH, "%s/.local/share/%s", (home != NULL) ? home : "~", append);

		s_paths[PATH_PROGRAM_DATA] = path;
		s_paths[PATH_PROGRAM_DATA] += "/";

		FileUtil::makeDirectory(path);
		return true;
	}

	bool setUserDocumentsPath(const char* append)
	{
		char path[TFE_MAX_PATH];
		char *home = getenv("HOME");
		memset(path, 0, TFE_MAX_PATH);
		snprintf(path, TFE_MAX_PATH, "%s/%s",  home ?: "~", append);

		s_paths[PATH_USER_DOCUMENTS] = path;
		s_paths[PATH_USER_DOCUMENTS] += "/";

		FileUtil::makeDirectory(path);
		return true;
	}

	bool setProgramPath()
	{
		char path[TFE_MAX_PATH];
		memset(path, 0, TFE_MAX_PATH);
		FileUtil::getCurrentDirectory(path);
		s_paths[PATH_PROGRAM] = path;
		s_paths[PATH_PROGRAM] += "/";
		FileUtil::setCurrentDirectory(s_paths[PATH_PROGRAM].c_str());
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

	void fixupPathAsDirectory(char* fullPath)
	{
		size_t len = strlen(fullPath);
		// Fix-up slashes.
		for (size_t i = 0; i < len; i++)
		{
			if (fullPath[i] == '\\')
			{
				fullPath[i] = '/';
			}
		}
		// Make sure it ends with a slash.
		if (fullPath[len - 1] != '/')
		{
			fullPath[len] = '/';
			fullPath[len + 1] = 0;
		}
	}

	void addSearchPath(const char* fullPath)
	{
		if (FileUtil::directoryExits(fullPath))
		{
			const size_t count = s_searchPaths.size();
			for (size_t i = 0; i < count; i++)
			{
				// If the path already exists, then don't add it again.
				if (!strcasecmp((s_searchPaths[i]).c_str(), fullPath))
				{
					return;
				}
			}

			s_searchPaths.push_back(fullPath);
		}
	}

	void addSearchPathToHead(const char* fullPath)
	{
		if (FileUtil::directoryExits(fullPath))
		{
			const size_t count = s_searchPaths.size();
			for (size_t i = 0; i < count; i++)
			{
				// If the path already exists, then don't add it again.
				if (!strcasecmp((s_searchPaths[i]).c_str(), fullPath))
				{
					return;
				}
			}

			s_searchPaths.push_front(fullPath);
		}
	}

	void clearSearchPaths()
	{
		s_searchPaths.clear();
		s_fileMappings.clear();
	}

	void clearLocalArchives()
	{
		const size_t count = s_localArchives.size();
		for (size_t i = 0; i < count ; i++)
		{
			Archive::freeArchive(s_localArchives[i]);
		}
		s_localArchives.clear();
	}

	// Add a single file that can be referenced by 'fileName' even though the real name may be different.
	void addSingleFilePath(const char* fileName, const char* filePath)
	{
		char fileNameLC[TFE_MAX_PATH];
		strcpy(fileNameLC, fileName);
		__strlwr(fileNameLC);

		char filePathFixed[TFE_MAX_PATH];
		strcpy(filePathFixed, filePath);
		FileUtil::fixupPath(filePathFixed);

		FileMapping mapping = { fileNameLC, filePathFixed };
		s_fileMappings.push_back(mapping);
	}

	void addLocalSearchPath(const char* localSearchPath)
	{
		char fullPath[TFE_MAX_PATH];
		snprintf(fullPath, TFE_MAX_PATH, "%s%s", getPath(PATH_SOURCE_DATA), localSearchPath);
		fixupPathAsDirectory(fullPath);

		addSearchPath(fullPath);
	}

	void addAbsoluteSearchPathToHead(const char* absoluteSearchPath)
	{
		char fullPath[TFE_MAX_PATH];
		strcpy(fullPath, absoluteSearchPath);
		fixupPathAsDirectory(fullPath);

		addSearchPathToHead(fullPath);
	}

	void addAbsoluteSearchPath(const char* absoluteSearchPath)
	{
		char fullPath[TFE_MAX_PATH];
		strcpy(fullPath, absoluteSearchPath);
		fixupPathAsDirectory(fullPath);

		addSearchPath(fullPath);
	}
		
	void addLocalArchiveToFront(Archive* archive)
	{
		s_localArchives.push_front(archive);
	}

	void removeFirstArchive()
	{
		s_localArchives.pop_front();
	}

	void addLocalArchive(Archive* archive)
	{
		s_localArchives.push_back(archive);
	}

	void removeLastArchive()
	{
		s_localArchives.pop_back();
	}

	bool getFilePath(const char* fileName, FilePath* outPath)
	{
		outPath->archive = nullptr;
		outPath->index = INVALID_FILE;
		outPath->path[0] = 0;

		// Search for any filemappings.
		// This is usually only used with mods and usually limited to 0-3 files.
		const size_t mappingCount  = s_fileMappings.size();
		for (size_t i = 0; i < mappingCount; i++)
		{
			if (0 == strcasecmp(s_fileMappings[i].fileName.c_str(), fileName))
			{
				strcpy(outPath->path, s_fileMappings[i].realPath.c_str());
				return true;
			}
		}

		// Search in the local search paths before local archives: s_searchPaths.
		const size_t pathCount = s_searchPaths.size();
		for (size_t i = 0; i < pathCount; i++)
		{
			char fullName[TFE_MAX_PATH];
			sprintf(fullName, "%s%s", s_searchPaths[i].c_str(), fileName);

			FileStream file;
			if (file.exists(fullName))
			{
				strncpy(outPath->path, fullName, TFE_MAX_PATH);
				return true;
			}
		}

		// Then archives: s_localArchives.
		const size_t archiveCount = s_localArchives.size();
		for (size_t i = 0; i < archiveCount; i++)
		{
			Archive *archive = s_localArchives[i];

			u32 index = archive->getFileIndex(fileName);
			if (index != INVALID_FILE)
			{
				outPath->archive = archive;
				outPath->index = index;
				return true;
			}
		}

		// Finally admit defeat.
		return false;
	}
}

#include <cstring>

#include "paths.h"
#include "fileutil.h"
#include "filestream.h"
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>
#include <algorithm>
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
	static std::deque<std::string> s_systemPaths;	// TFE Support data paths

	void setPath(TFE_PathType pathType, const char* path)
	{
		s_paths[pathType] = path;
	}


	// Where to store TFE settings, saves, screenshots.
	// If the envvar "TFE_DATA_HOME" is set, the all
	// this stuff is stored beneath $HOME$TFE_DATA_HOME/TheForceEngine
	// otherwise in $HOME/.local/share/TheForceEngine.
	// So, to have it all stored at the root of home, just set
	// TFE_DATA_HOME=/
	static void setXDGPath(const char *tfe, u32 pathid)
	{
		char path[TFE_MAX_PATH];
		char *home = getenv("HOME");
		char *pfx = getenv("TFE_DATA_HOME");
		int i;

		if (!pfx) {
			pfx = getenv("XDG_DATA_HOME");
			if (!pfx) {
				pfx = strdup("/.local/share");
			}
		}

		// remove trailing slashes in prefix
		i = strlen(pfx);
		while ((i > 0) && (pfx[i - 1] == '/')) {
			pfx[i - 1] = 0;
			i--;
		}

		if (!home) {
			// whoa, how did that happen?
			TFE_System::logWrite(LOG_ERROR, "paths", "$HOME is undefined! Defaulting to /tmp");
			home = strdup("/tmp");
		}

		memset(path, 0, TFE_MAX_PATH);
		snprintf(path, TFE_MAX_PATH, "%s%s/%s/", home, pfx, tfe);
		s_paths[pathid] = path;
		FileUtil::makeDirectory(path);
	}

	// System Paths: where all the support data is located, which is
	// accessed mainly via relative paths. We look at, in this order:
	// [debug build] $PWD
	// /usr/local/share/
	// /usr/share/
	// $HOME/.local/share/
	//  the above last because it is not touched when a new version
	//  is installed via e.g. package manager or 'make install',
	//  and user might end up with incompatible data causing hard
	//  to debug (non-)issues.
	bool setProgramDataPath(const char *append)
	{
		std::string s;
#ifndef NDEBUG
		// debug build: look at where we're running from first.
		s_systemPaths.push_back(getPath(PATH_PROGRAM));
#endif
		s = std::string("/usr/local/share/") + append + "/";
		s_systemPaths.push_back(s);
		s = std::string("/usr/share/") + append + "/";
		s_systemPaths.push_back(s);
		setXDGPath(append, PATH_PROGRAM_DATA);
		s_systemPaths.push_back(getPath(PATH_PROGRAM_DATA));
		
		return true;
	}

	bool setUserDocumentsPath(const char *append)
	{
		setXDGPath(append, PATH_USER_DOCUMENTS);
		s_systemPaths.push_back(getPath(PATH_USER_DOCUMENTS));
		return true;
	}

	bool setProgramPath(void)
	{
		char p[TFE_MAX_PATH];
		memset(p, 0, TFE_MAX_PATH);
		FileUtil::getCurrentDirectory(p);
		s_paths[PATH_PROGRAM] = p;
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

	void appendPath(TFE_PathType pathType, const char *filename, char *path, size_t bufferLen/* = TFE_MAX_PATH*/)
	{
		snprintf(path, bufferLen, "%s%s", getPath(pathType), filename);
	}

	void fixupPathAsDirectory(char *fullPath)
	{
		char *c = fullPath;
		int len = strlen(fullPath);

		while (NULL != (c = strchr(c, '\\'))) {
			*c = '/';
		}

		// Make sure it ends with a slash.
		if (fullPath[len - 1] != '/')
		{
			fullPath[len] = '/';
			fullPath[len + 1] = 0;
		}
	}

	// take a relative input filepath, try to find it anywhere in the
	// system paths, and return an absolute path to the file if found.
	// report whether a change was made to the input.
	bool mapSystemPath(char *fname)
	{
		char fullname[TFE_MAX_PATH];
		for (auto it = s_systemPaths.begin(); it != s_systemPaths.end(); it++) {
			sprintf(fullname, "%s%s", it->c_str(), fname);
			if (FileUtil::existsNoCase(fullname)) {
				strncpy(fname, fullname, TFE_MAX_PATH);
				return true;
			}
		}
		return false;
	}

	void addSearchPath(const char *fullPath)
	{
		if (!FileUtil::directoryExits(fullPath))
			return;

		for (auto it = s_searchPaths.begin(); it != s_searchPaths.end(); it++)
		{
			// If the path already exists, then don't add it again.
			if (!strcasecmp(it->c_str(), fullPath))
			{
				return;
			}
		}
		s_searchPaths.push_back(fullPath);
	}

	void addSearchPathToHead(const char *fullPath)
	{
		if (!FileUtil::directoryExits(fullPath))
			return;

		for (auto it = s_searchPaths.begin(); it != s_searchPaths.end(); it++)
		{
			// If the path already exists, then don't add it again.
			if (!strcasecmp(it->c_str(), fullPath))
			{
				return;
			}
		}
		s_searchPaths.push_front(fullPath);
	}

	void clearSearchPaths(void)
	{
		s_searchPaths.clear();
		s_fileMappings.clear();
	}

	void clearLocalArchives(void)
	{
		std::for_each(s_localArchives.begin(), s_localArchives.end(),
				[](Archive *a) { Archive::freeArchive(a); });
		s_localArchives.clear();
	}

	// Add a single file that can be referenced by 'fileName' even though the real name may be different.
	void addSingleFilePath(const char *fileName, const char *filePath)
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

	void addLocalSearchPath(const char *locpath)
	{
		char p[TFE_MAX_PATH];
		snprintf(p, TFE_MAX_PATH, "%s%s", getPath(PATH_SOURCE_DATA), locpath);
		fixupPathAsDirectory(p);

		addSearchPath(p);
	}

	void addAbsoluteSearchPathToHead(const char *abspath)
	{
		char p[TFE_MAX_PATH];
		strcpy(p, abspath);
		fixupPathAsDirectory(p);

		addSearchPathToHead(p);
	}

	void addAbsoluteSearchPath(const char *abspath)
	{
		char p[TFE_MAX_PATH];
		strcpy(p, abspath);
		fixupPathAsDirectory(p);

		addSearchPath(p);
	}

	void addLocalArchiveToFront(Archive *a)
	{
		s_localArchives.push_front(a);
	}

	void removeFirstArchive(void)
	{
		s_localArchives.pop_front();
	}

	void addLocalArchive(Archive *a)
	{
		s_localArchives.push_back(a);
	}

	void removeLastArchive(void)
	{
		s_localArchives.pop_back();
	}

	bool getFilePath(const char *fileName, FilePath *outPath)
	{
		char fullname[TFE_MAX_PATH];

		outPath->archive = nullptr;
		outPath->index = INVALID_FILE;
		outPath->path[0] = 0;

		// Search for any filemappings.
		// This is usually only used with mods and usually limited to 0-3 files.
		for (auto it = s_fileMappings.begin(); it != s_fileMappings.end(); it++) {
			if (0 == strcasecmp(it->fileName.c_str(), fileName)) {
				strcpy(outPath->path, it->realPath.c_str());
				return true;
			}
		}

		// Search in the local search paths before local archives: s_searchPaths.
		for (auto it = s_searchPaths.begin(); it != s_searchPaths.end(); it++) {
			sprintf(fullname, "%s%s", it->c_str(), fileName);
			if (FileUtil::existsNoCase(fullname)) {
				strncpy(outPath->path, fullname, TFE_MAX_PATH);
				return true;
			}
		}

		// Then archives: s_localArchives.
		for (auto it = s_localArchives.begin(); it != s_localArchives.end(); it++) {
			u32 index = (*it)->getFileIndex(fileName);
			if (index != INVALID_FILE) {
				outPath->archive = *it;
				outPath->index = index;
				return true;
			}
		}

		// Finally admit defeat.
		return false;
	}
}

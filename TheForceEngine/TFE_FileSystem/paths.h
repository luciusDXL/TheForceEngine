#pragma once
#include "fileutil.h"
#include <TFE_System/types.h>

enum TFE_PathType
{
	PATH_PROGRAM = 0,		// Program folder
	PATH_PROGRAM_DATA,		// ProgramData/ on Windows - save temporary or cached data here.
	PATH_USER_DOCUMENTS,	// Users/username/Documents/ on Windows - save settings here.
	PATH_SOURCE_DATA,		// This is the location of the source data, such as maps, textures, etc.
	PATH_EMULATOR,			// Path to the dosbox exe (for the editor).
	PATH_MOD,				// Use this to reference mods.
	PATH_COUNT
};

// Use an extended MAX_PATH, the Windows value is actually too small for some valid paths.
#define TFE_MAX_PATH 1024

class Archive;

struct FilePath
{
	Archive* archive;			// archive pointer.
	u32 index;					// file index into an archive or INVALID_FILE.
	char path[TFE_MAX_PATH];	// path string.
};

namespace TFE_Paths
{
	// Paths, such as the source data or mods.
	void setPath(TFE_PathType pathType, const char* path);

	// Platform specific user data path.
	bool setProgramDataPath(const char* append);
	bool setUserDocumentsPath(const char* append);
	// Platform specific executable path.
	bool setProgramPath();
	// find a relative path in a TFE system directory. true if mapping was done.
	bool mapSystemPath(char *path);
	
	const char* getPath(TFE_PathType pathType);
	bool hasPath(TFE_PathType pathType);
	void appendPath(TFE_PathType pathType, const char* filename, char* path, size_t bufferLen = TFE_MAX_PATH);
	void fixupPathAsDirectory(char* fullPath);

	void clearSearchPaths();
	void clearLocalArchives();

	void addLocalSearchPath(const char* localSearchPath);
	void addAbsoluteSearchPath(const char* absoluteSearchPath);
	void addAbsoluteSearchPathToHead(const char* absoluteSearchPath);
	void addLocalArchive(Archive* archive);
	void removeLastArchive();
	void addLocalArchiveToFront(Archive* archive);
	void removeFirstArchive();
	bool getFilePath(const char* fileName, FilePath* path);
	void getAllFilesFromSearchPaths(const char* subdirectory, const char* ext, FileList& allFiles);

	// Add a single file that can be referenced by 'fileName' even though the real name may be different.
	void addSingleFilePath(const char* fileName, const char* filePath);
}

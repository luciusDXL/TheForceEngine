#pragma once
#include <DXL2_System/types.h>

enum DXL2_PathType
{
	PATH_PROGRAM = 0,		// Program folder
	PATH_PROGRAM_DATA,		// ProgramData/ on Windows - save temporary or cached data here.
	PATH_USER_DOCUMENTS,	// Users/username/Documents/ on Windows - save settings here.
	PATH_SOURCE_DATA,		// This is the location of the source data, such as maps, textures, etc.
	PATH_MOD,				// Use this to reference mods.
	PATH_COUNT
};

// Use an extended MAX_PATH, the Windows value is actually too small for some valid paths.
#define DXL2_MAX_PATH 1024

namespace DXL2_Paths
{
	// Paths, such as the source data or mods.
	void setPath(DXL2_PathType pathType, const char* path);

	// Platform specific user data path.
	bool setProgramDataPath(const char* append);
	bool setUserDocumentsPath(const char* append);
	// Platform specific executable path.
	bool setProgramPath();
	
	const char* getPath(DXL2_PathType pathType);
	void appendPath(DXL2_PathType pathType, const char* filename, char* path, size_t bufferLen = DXL2_MAX_PATH);
}

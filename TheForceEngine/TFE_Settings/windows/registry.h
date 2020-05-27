#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Settings
// This is a global repository for program settings in an INI like
// format.
//
// This includes reading and writing settings as well as storing an
// in-memory cache to get accessed at runtime.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>

namespace WindowsRegistry
{
	bool getGogPathFromRegistry(const char* productId, char* outPath);
	bool getSteamPathFromRegistry(const char* localPath, char* outPath);
}
#include "registry.h"
#include <TFE_FileSystem/fileutil.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>

namespace WindowsRegistry
{
	bool readStringFromRegistry(HKEY key, const char* subKey, const char* valueName, char* value)
	{
		if (!subKey || !valueName || !value)
		{
			return false;
		}

		value[0] = 0;
		HKEY pathKey;
		if (RegOpenKeyExA(key, subKey, 0, KEY_QUERY_VALUE, &pathKey) == ERROR_SUCCESS)
		{
			DWORD pathType, pathLen;
			if (RegQueryValueExA(pathKey, valueName, nullptr, &pathType, nullptr, &pathLen) == ERROR_SUCCESS)
			{
				assert(pathLen < TFE_MAX_PATH);
				if (pathType == REG_SZ && RegQueryValueExA(pathKey, valueName, nullptr, nullptr, (BYTE*)value, &pathLen) == ERROR_SUCCESS)
				{
					value[pathLen] = 0;
				}
			}
			RegCloseKey(pathKey);
		}
		return value[0] != 0;
	}

	void fixPathSlashes(char* path)
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

	bool getGogPathFromRegistry(const char* productId, char* outPath)
	{
		char registryPath[TFE_MAX_PATH];
	#ifdef _WIN64
		sprintf(registryPath, "Software\\Wow6432Node\\GOG.com\\Games\\%s", productId);
	#else
		sprintf(registryPath, "Software\\GOG.com\\Games\\%s", productId);
	#endif
		char gogPath[TFE_MAX_PATH];
		if (!readStringFromRegistry(HKEY_LOCAL_MACHINE, registryPath, "path", gogPath))
		{
			return false;
		}
		sprintf(outPath, "%s/", gogPath);
		fixPathSlashes(outPath);
		return FileUtil::directoryExits(outPath);
	}

	bool getSteamPathFromRegistry(const char* localPath, char* outPath)
	{
		char steamPath[TFE_MAX_PATH];
		if (!readStringFromRegistry(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", steamPath) &&
			!readStringFromRegistry(HKEY_LOCAL_MACHINE, "Software\\Valve\\Steam", "InstallPath", steamPath))
		{
			return false;
		}
		sprintf(outPath, "%s/SteamApps/common/%s", steamPath, localPath);
		fixPathSlashes(outPath);
		return FileUtil::directoryExits(outPath);
	}
}

#include <TFE_FileSystem/fileutil.h>

namespace LinuxDefPaths
{
	bool getGogPathFromRegistry(const char* productId, char* outPath)
	{
		return false;
	}

	bool getSteamPathFromRegistry(const char* localPath, char* outPath)
	{
		sprintf(outPath, "~/.local/share/Steam/steamapps/common/%s", localPath);
		return FileUtil::directoryExits(outPath);
	}
}

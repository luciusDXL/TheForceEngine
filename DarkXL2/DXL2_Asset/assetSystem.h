#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Asset System
// Handles global asset state, such as custom archive sources.
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>
#include <vector>

class Archive;

namespace DXL2_AssetSystem
{
	void setCustomArchive(Archive* archive);
	void clearCustomArchive();

	Archive* getCustomArchive();
	bool readAssetFromGob(const char* defaultGob, const char* filename, std::vector<u8>& buffer);
	bool readAssetFromGob(const char* defaultGob, const char* filename, std::vector<char>& buffer);
}

#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Asset System
// Handles global asset state, such as custom archive sources.
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>
#include <DXL2_Archive/archive.h>
#include <vector>

namespace DXL2_AssetSystem
{
	void setCustomArchive(Archive* archive);
	void clearCustomArchive();

	Archive* getCustomArchive();
	bool readAssetFromArchive(const char* defaultArchive, ArchiveType type, const char* filename, std::vector<u8>& buffer);
	bool readAssetFromArchive(const char* defaultArchive, ArchiveType type, const char* filename, std::vector<char>& buffer);
	bool readAssetFromArchive(const char* defaultArchive, const char* filename, std::vector<u8>& buffer);
	bool readAssetFromArchive(const char* defaultArchive, const char* filename, std::vector<char>& buffer);
}

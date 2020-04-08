#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Asset System
// Handles global asset state, such as custom archive sources.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Archive/archive.h>
#include <vector>

namespace TFE_AssetSystem
{
	void setCustomArchive(Archive* archive);
	void clearCustomArchive();

	Archive* getCustomArchive();
	bool readAssetFromArchive(const char* defaultArchive, ArchiveType type, const char* filename, std::vector<u8>& buffer);
	bool readAssetFromArchive(const char* defaultArchive, ArchiveType type, const char* filename, std::vector<char>& buffer);
	bool readAssetFromArchive(const char* defaultArchive, const char* filename, std::vector<u8>& buffer);
	bool readAssetFromArchive(const char* defaultArchive, const char* filename, std::vector<char>& buffer);
}

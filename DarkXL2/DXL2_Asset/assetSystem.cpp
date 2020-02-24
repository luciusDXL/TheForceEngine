#include "assetSystem.h"
#include <DXL2_System/system.h>
#include <DXL2_Archive/archive.h>

namespace DXL2_AssetSystem
{
	static Archive* s_customArchive = nullptr;

	void setCustomArchive(Archive* archive)
	{
		s_customArchive = archive;
	}

	void clearCustomArchive()
	{
		s_customArchive = nullptr;
	}

	Archive* getCustomArchive()
	{
		return s_customArchive;
	}

	Archive* openGobFile(const char* defaultGob, const char* filename)
	{
		// First try reading from the custom Archive, if there is one.
		if (s_customArchive)
		{
			const u32 index = s_customArchive->getFileIndex(filename);
			if (index != INVALID_FILE && s_customArchive->openFile(index))
			{
				return s_customArchive;
			}
		}

		// Otherwise read from the specified GOB archive.
		char gobPath[DXL2_MAX_PATH];
		DXL2_Paths::appendPath(PATH_SOURCE_DATA, defaultGob, gobPath);
		Archive* archive = Archive::getArchive(ARCHIVE_GOB, defaultGob, gobPath);
		if (!archive)
		{
			DXL2_System::logWrite(LOG_ERROR, "Archive", "Cannot open source archive file \"%s\"", gobPath);
			return nullptr;
		}

		if (!archive->openFile(filename))
		{
			return nullptr;
		}
		return archive;
	}

	bool readAssetFromGob(const char* defaultGob, const char* filename, std::vector<u8>& buffer)
	{
		Archive* archive = openGobFile(defaultGob, filename);
		if (archive)
		{
			// Read the file into memory for parsing.
			const size_t len = archive->getFileLength();
			buffer.resize(len);
			archive->readFile(buffer.data(), len);
			archive->closeFile();
			return true;
		}
		return false;
	}

	bool readAssetFromGob(const char* defaultGob, const char* filename, std::vector<char>& buffer)
	{
		Archive* archive = openGobFile(defaultGob, filename);
		if (archive)
		{
			// Read the file into memory for parsing.
			const size_t len = archive->getFileLength();
			buffer.resize(len);
			archive->readFile(buffer.data(), len);
			archive->closeFile();
			return true;
		}
		return false;
	}
}

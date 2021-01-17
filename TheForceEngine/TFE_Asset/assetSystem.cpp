#include "assetSystem.h"
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>

namespace TFE_AssetSystem
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

	Archive* openArchiveFile(const char* defaultArchive, ArchiveType type, const char* filename)
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
		char gobPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_SOURCE_DATA, defaultArchive, gobPath);
		Archive* archive = Archive::getArchive(type, defaultArchive, gobPath);
		if (!archive)
		{
			// Try the local mods directory.
			TFE_Paths::appendPath(PATH_PROGRAM, defaultArchive, gobPath);
			archive = Archive::getArchive(type, defaultArchive, gobPath);
			if (!archive)
			{
				TFE_System::logWrite(LOG_ERROR, "Archive", "Cannot open source archive file \"%s\"", gobPath);
				return nullptr;
			}
		}

		if (!archive->openFile(filename))
		{
			return nullptr;
		}
		return archive;
	}
		
	bool readAssetFromArchive(const char* defaultArchive, ArchiveType type, const char* filename, std::vector<u8>& buffer)
	{
		Archive* archive = openArchiveFile(defaultArchive, type, filename);
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

	bool readAssetFromArchive(const char* defaultArchive, ArchiveType type, const char* filename, std::vector<char>& buffer)
	{
		Archive* archive = openArchiveFile(defaultArchive, type, filename);
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

	bool readAssetFromArchive(const char* defaultArchive, const char* filename, std::vector<u8>& buffer)
	{
		const ArchiveType type = Archive::getArchiveTypeFromName(defaultArchive);
		Archive* archive = openArchiveFile(defaultArchive, type, filename);
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

	bool readAssetFromArchive(const char* defaultArchive, const char* filename, std::vector<char>& buffer)
	{
		const ArchiveType type = Archive::getArchiveTypeFromName(defaultArchive);
		Archive* archive = openArchiveFile(defaultArchive, type, filename);
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

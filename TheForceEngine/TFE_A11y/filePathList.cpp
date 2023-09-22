#include "filePathList.h"
#include <TFE_FileSystem/fileutil.h>

namespace TFE_A11Y {

	FilePathList::FilePathList()
	{
	}

	void FilePathList::addFiles(const char directoryPath[], const char extension[], std::function<bool(const string)> filterFunc)
	{
		FileList dirList;
		FileUtil::readDirectory(directoryPath, extension, dirList);
		if (!dirList.empty())
		{
			const size_t count = dirList.size();
			const std::string* dir = dirList.data();
			for (size_t d = 0; d < count; d++)
			{
				if (filterFunc == nullptr || filterFunc(dir[d]))
				{
					m_fileNames.push_back(dir[d]);
					m_filePaths.push_back(directoryPath + dir[d]);
				}
			}
		}
	}

	/**/
	FilePath FilePathList::at(size_t index)
	{
		if (index < 0 || index >= count()) { return FilePath(nullptr, nullptr); }
		return FilePath(m_fileNames.at(index), m_filePaths.at(index));
	}
	
	void FilePathList::clear()
	{
		m_fileNames.clear();
		m_filePaths.clear();
	}

	size_t FilePathList::count()
	{
		return m_filePaths.size();
	}

	std::vector<string>* FilePathList::getFilePaths()
	{
		return &m_filePaths;
	}

	std::vector<string>* FilePathList::getFileNames()
	{
		return &m_fileNames;
	}
}
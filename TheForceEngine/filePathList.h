#pragma once
#include <functional>
#include <string>
#include <vector>
using std::string;

namespace TFE_A11Y {

	// Pairs together a file name and full path to the file.
	struct FilePath {
		string name;
		string path;

		FilePath() = default;

		FilePath(const string& name, const string& path)
		{
			this->name = name;
			this->path = path;
		}
	};

	// Stores parallel lists of file names and paths. Useful when displaying a dropdown
	// list of files in the front-end UI.
	class FilePathList
	{
		public:
			FilePathList();

			void addFiles(const char directoryPath[], const char extension[], std::function<bool(const string)> filterFunc = nullptr);
			FilePath at(size_t index);
			void clear();

			size_t count();
			std::vector<string>* getFilePaths();
			std::vector<string>* getFileNames();

		private:
			std::vector<string> m_filePaths;
			std::vector<string> m_fileNames;
	};
}
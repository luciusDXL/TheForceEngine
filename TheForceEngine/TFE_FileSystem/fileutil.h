#pragma once
#include <TFE_System/types.h>
#include <vector>
#include <string>
#include <tuple>

using namespace std;
typedef vector<string> FileList;

// tuple of path, filename, extension
typedef std::tuple<std::string, std::string, std::string> FilePath2;
// List of Files and their paths
typedef std::vector<FilePath2> FileList2;

// Get the Filename
static inline std::string& FL2_NAME(FilePath2& entry)
{
	return std::get<1>(entry);
}

// Get the path to the File
static inline std::string& FL2_PATH(FilePath2& entry)
{
	return std::get<0>(entry);
}

// Get the extension of the file
static inline std::string& FL2_EXT(FilePath2& entry)
{
	return std::get<2>(entry);
}

// construct a full absolute path to the file
static inline std::string FL2_FULLFILE(FilePath2& entry)
{
	return FL2_PATH(entry) + FL2_NAME(entry) + "." + FL2_EXT(entry);
}

namespace FileUtil
{
	void readDirectory(const char* dir, const char* ext, FileList& fileList);
	void readTFEDirectory(const char* dir, const char* ext, FileList2& fileList);
	bool makeDirectory(const char* dir);
	void getCurrentDirectory(char* dir);
	void getExecutionDirectory(char* dir);
	void setCurrentDirectory(const char* dir);

	void readSubdirectories(const char* dir, FileList& dirList);

	void getFileNameFromPath(const char* path, char* name, bool includeExt=false);
	void getFilePath(const char* filename, char* path);
	void getFileExtension(const char* filename, char* extension);

	void copyFile(const char* srcFile, const char* dstFile);
	void deleteFile(const char* srcFile);

	bool exists(const char* path);
	bool directoryExits(const char* path, char* outPath = nullptr);
	u64  getModifiedTime(const char* path);

	void fixupPath(char* path);
	void convertToOSPath(const char* path, char* pathOS);

	void replaceExtension(const char* srcPath, const char* newExt, char* outPath);
	void stripExtension(const char* srcPath, char* outPath);
}

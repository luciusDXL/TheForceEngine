#pragma once
#include <DXL2_System/types.h>
#include <vector>
#include <string>

using namespace std;
typedef vector<string> FileList;

namespace FileUtil
{
	void readDirectory(const char* dir, const char* ext, FileList& fileList);
	bool makeDirectory(const char* dir);
	void getCurrentDirectory(char* dir);

	void readSubdirectories(const char* dir, FileList& dirList);

	void getFileNameFromPath(const char* path, char* name, bool includeExt=false);
	void getFilePath(const char* filename, char* path);
	void getFileExtension(const char* filename, char* extension);

	bool exists(const char* path);
	u64  getModifiedTime(const char* path);
}

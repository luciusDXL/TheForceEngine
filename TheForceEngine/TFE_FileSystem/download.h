#pragma once
#include <string>

using namespace std;

namespace Download
{
	std::string curlWeb(const char* webPath);
	bool download(const char* url, const char* dest);
}
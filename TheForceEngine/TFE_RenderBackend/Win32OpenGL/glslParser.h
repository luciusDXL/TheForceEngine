#pragma once
//////////////////////////////////////////////////////////////////////
// Wrapper for OpenGL textures that expose the limited set of 
// required functionality.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <vector>

namespace GLSLParser
{
	size_t parseInclude(const char* input, size_t len, std::vector<char>& outputBuffer);
	void parseBuffer(const std::vector<char>& tempBuffer, std::vector<char>& outputBuffer);
	bool parseFile(const char* fileName, std::vector<char>& outputBuffer);
}
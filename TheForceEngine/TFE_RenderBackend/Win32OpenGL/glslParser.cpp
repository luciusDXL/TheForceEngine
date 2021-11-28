#include <cstring>

#include "glslParser.h"
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_RenderBackend/renderState.h>
#include <GL/glew.h>
#include <assert.h>

namespace GLSLParser
{
	size_t parseInclude(const char* input, size_t len, std::vector<char>& outputBuffer)
	{
		s32 start = 0, end = 0;
		char fileName[64];
		for (size_t c = 9; c < len; c++)
		{
			if ((input[c] == '"' || input[c] == '<') && !start)
			{
				start = s32(c + 1);
			}
			else if ((input[c] == '"' || input[c] == '>') && start)
			{
				end = s32(c);
				break;
			}
			else if (start)
			{
				fileName[c - start] = input[c];
			}
		}
		fileName[end - start] = 0;

		parseFile(fileName, outputBuffer);
		return end + 1;
	}

	// Parse the buffer for includes...
	void parseBuffer(const std::vector<char>& tempBuffer, std::vector<char>& outputBuffer)
	{
		const size_t len = tempBuffer.size();
		const char* contents = tempBuffer.data();

		for (size_t c = 0; c < len - 1; c++)
		{
			if (contents[c] == '#' && strncasecmp(&contents[c], "#include", 8) == 0)
			{
				c += parseInclude(&contents[c], len - c + 1, outputBuffer);
			}
			else
			{
				outputBuffer.push_back(contents[c]);
			}
		}
	}

	bool parseFile(const char* fileName, std::vector<char>& outputBuffer)
	{
		char filePath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_PROGRAM, fileName, filePath);

		FileStream file;
		if (!file.open(filePath, FileStream::MODE_READ)) { return false; }

		const u32 fileSize = (u32)file.getSize();
		std::vector<char> tempBuffer;
		tempBuffer.resize(fileSize + 1);
		tempBuffer.data()[fileSize] = 0;
		file.readBuffer(tempBuffer.data(), fileSize);
		file.close();

		parseBuffer(tempBuffer, outputBuffer);
		return true;
	}
}
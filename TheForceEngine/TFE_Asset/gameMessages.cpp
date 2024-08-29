#include "gameMessages.h"
#include <TFE_FileSystem/physfswrapper.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

namespace TFE_GameMessages
{
	static bool s_loaded = false;
	std::map<u32, std::string> s_msgMap;
	static std::vector<char> s_buffer;
	
	bool load()
	{
		if (s_loaded) { return true; }

		vpFile file(VPATH_GAME, "TEXT.MSG", false);
		if (!file)
			return false;
		unsigned int len = file.size();
		s_buffer.resize(len);
		file.read(s_buffer.data(), len);
		file.close();

		TFE_Parser parser;
		parser.init(s_buffer.data(), len);
		parser.addCommentString("//");
		parser.addCommentString("#");
		parser.enableBlockComments();

		size_t bufferPos = 0;
		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }
			
			TokenList tokens;
			parser.tokenizeLine(line, tokens);
			if (tokens.size() < 3) { continue; }
			
			// Skip until the first token is a number.
			const char* idStr = tokens[0].c_str();
			if (idStr[0] < '0' || idStr[0] > '9') { continue; }

			// Finally read the line.
			char* endPtr = nullptr;
			u32 id = strtoul(idStr, &endPtr, 10);
			u32 internal = strtoul(tokens[1].c_str(), &endPtr, 10);
				
			// And then add the message.
			s_msgMap[id] = tokens[2];
		};
		s_loaded = true;
		return true;
	}

	void unload()
	{
		s_loaded = false;
		s_msgMap.clear();
	}

	const char* getMessage(u32 msgId)
	{
		if (!s_loaded) { load(); }

		std::map<u32, std::string>::iterator iMsg = s_msgMap.find(msgId);
		if (iMsg == s_msgMap.end())
		{
			// cannot find the message.
			return nullptr;
		}

		return iMsg->second.c_str();
	}
}

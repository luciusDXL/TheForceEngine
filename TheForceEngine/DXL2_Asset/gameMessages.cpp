#include "gameMessages.h"
#include <DXL2_System/system.h>
#include <DXL2_Asset/assetSystem.h>
#include <DXL2_Archive/archive.h>
#include <DXL2_System/parser.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

namespace DXL2_GameMessages
{
	static const char* c_defaultGob = "DARK.GOB";

	static bool s_loaded = false;
	std::map<u32, std::string> s_msgMap;
	static std::vector<char> s_buffer;
	
	bool load()
	{
		if (s_loaded) { return true; }

		if (DXL2_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, "TEXT.MSG", s_buffer))
		{
			DXL2_Parser parser;
			size_t len = s_buffer.size();
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
			return true;
		}
		return false;
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

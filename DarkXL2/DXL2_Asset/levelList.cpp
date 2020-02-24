#include "levelList.h"
#include <DXL2_System/system.h>
#include <DXL2_Asset/assetSystem.h>
#include <DXL2_Archive/archive.h>
#include <DXL2_System/parser.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

namespace DXL2_LevelList
{
	struct LevelInfo
	{
		std::string name;
		std::string fileName;
	};

	static const char* c_defaultGob = "DARK.GOB";
	   
	static bool s_loaded = false;
	std::vector<LevelInfo> s_levelInfo;
	static std::vector<char> s_buffer;
	
	bool load()
	{
		if (s_loaded) { return true; }

		// TODO: It would be more robust to modify the parser to allow it to ignore white space
		// in favor of seperators (except for leading/trailing whitespace) - as an option. This
		// way the first two tokens will be the name and file name, making the code simpler and less likely to break.
		if (DXL2_AssetSystem::readAssetFromGob(c_defaultGob, "JEDI.LVL", s_buffer))
		{
			DXL2_Parser parser;
			size_t len = s_buffer.size();
			parser.init(s_buffer.data(), len);
			parser.addCommentString("//");
			parser.addCommentString("#");
			parser.enableBlockComments();

			size_t bufferPos = 0;
			u32 count = 0;
			u32 index = 0;
			char* endPtr;
			while (bufferPos < len)
			{
				const char* line = parser.readLine(bufferPos);
				if (!line) { break; }

				TokenList tokens;
				parser.tokenizeLine(line, tokens);
				// First line should have the level count.
				if (tokens.size() == 2 && tokens[0] == "LEVELS")
				{
					count = strtoul(tokens[1].c_str(), &endPtr, 10);
					s_levelInfo.resize(count);
					continue;
				}
				// Should be at least a name, file name and a list of paths that aren't useful anymore.
				if (tokens.size() < 3) { continue; }

				const s32 tokenOffset = tokens[tokens.size() - 1].c_str()[0] == 'L' && tokens[tokens.size() - 1].c_str()[1] == ':' ? 2 : 1;
				const s32 nameCount = (s32)tokens.size() - tokenOffset;
				s_levelInfo[index].name = tokens[0];
				for (s32 i = 1; i < nameCount; i++)
				{
					s_levelInfo[index].name += ' ';
					s_levelInfo[index].name += tokens[i];
				}

				s_levelInfo[index].fileName = tokens[nameCount];
				index++;
			};
			return true;
		}
		return false;
	}

	void unload()
	{
		s_loaded = false;
		s_levelInfo.clear();
	}

	u32 getLevelCount()
	{
		return (u32)s_levelInfo.size();
	}

	const char* getLevelFileName(u32 index)
	{
		if (index >= (u32)s_levelInfo.size())
		{
			DXL2_System::logWrite(LOG_ERROR, "Level", "Tried to access an invalid level index: %u", index);
			return nullptr;
		}
		return s_levelInfo[index].fileName.c_str();
	}

	const char* getLevelName(u32 index)
	{
		if (index >= (u32)s_levelInfo.size())
		{
			DXL2_System::logWrite(LOG_ERROR, "Level", "Tried to access an invalid level index: %u", index);
			return nullptr;
		}
		return s_levelInfo[index].name.c_str();
	}
}

#include <cstring>

#include "cutsceneList.h"
#include <TFE_DarkForces/util.h>
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_System/parser.h>

namespace TFE_DarkForces
{
	static char* s_buffer = nullptr;

	void cutsceneList_freeBuffer()
	{
		s_buffer = nullptr;
	}

	char* copyStringAndNullTerminate(char* dstString, const char* srcString, s32 bufferSize)
	{
		strCopyAndZero(dstString, srcString, bufferSize - 1);
		dstString[bufferSize - 1] = 0;
		return dstString;
	}

	CutsceneState* cutsceneList_load(const char* filename)
	{
		FilePath filePath;
		if (!TFE_Paths::getFilePath(filename, &filePath))
		{
			return nullptr;
		}

		FileStream file;
		if (!file.open(&filePath, FileStream::MODE_READ)) { return nullptr; }

		size_t size = file.getSize();
		s_buffer = (char*)game_realloc(s_buffer, size);
		if (!s_buffer) { return nullptr; }

		file.readBuffer(s_buffer, (u32)size);
		file.close();

		TFE_Parser parser;
		parser.init(s_buffer, size);
		parser.addCommentString("#");

		size_t bufferPos = 0;
		const char* fileData = parser.readLine(bufferPos);
		s32 v0, v1;
		if (sscanf(fileData, "CUT %d.%d", &v0, &v1) != 2)
		{
			TFE_System::logWrite(LOG_ERROR, "LoadList", "Cannot read version.");
			return nullptr;
		}

		if (v0 != 1 || v1 != 0)
		{
			TFE_System::logWrite(LOG_ERROR, "LoadList", "Invalid version: %d.%d; should be 1.0", v0, v1);
			return nullptr;
		}

		s32 count = 0;
		fileData = parser.readLine(bufferPos);
		if (sscanf(fileData, "CUTS %d", &count) != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "LoadList", "Cannot read count.");
			return nullptr;
		}

		CutsceneState* items = (CutsceneState*)game_alloc((count + 1) * sizeof(CutsceneState));
		memset(items, 0, (count + 1) * sizeof(CutsceneState));
		if (!items)
		{
			TFE_System::logWrite(LOG_ERROR, "LoadList", "Cannot allocate %d list items.", count + 1);
			return nullptr;
		}

		for (s32 i = 0; i < count; i++)
		{
			fileData = parser.readLine(bufferPos);
			if (!fileData)
			{
				break;
			}

			s32 id;
			char archive[32], scene[32];
			s32 speed, nextId, skip, music = 0, volume;
			s32 r = sscanf(fileData, " %d: %s %s %d %d %d %d %d", &id, archive, scene, &speed, &nextId, &skip, &music, &volume);
			if (r != 8)
			{
				TFE_System::logWrite(LOG_ERROR, "LoadList", "Invalid cutscene data.");
				break;
			}

			copyStringAndNullTerminate(items[i].archive, archive, 14);
			copyStringAndNullTerminate(items[i].scene, scene, 10);
			items[i].id     = id;
			items[i].nextId = nextId;
			items[i].skip   = skip;
			items[i].speed  = speed;
			items[i].music  = music;
			items[i].volume = volume;
		}

		return items;
	}
}  // TFE_DarkForces
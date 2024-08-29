#include "tfeMessage.h"
#include "parser.h"
#include <TFE_FileSystem/physfswrapper.h>
#include <cassert>
#include <cstring>

namespace TFE_System
{
	static char* s_tfeMessage[TFE_MSG_COUNT];
	static bool s_messagesLoaded = false;

	const char* getMessage(TFE_Message msg)
	{
		if (msg < TFE_MSG_SAVE || msg >= TFE_MSG_COUNT) { return nullptr; }
		return s_tfeMessage[msg];
	}

	void freeMessages()
	{
		if (!s_messagesLoaded) { return; }

		for (s32 i = 0; i < TFE_MSG_COUNT; i++)
		{
			free(s_tfeMessage[i]);
			s_tfeMessage[i] = nullptr;
		}
		s_messagesLoaded = false;
	}

	bool loadMessages(const char* path)
	{
		char *contents;
		unsigned int len;
		vpFile ff(VPATH_TFE, path, &contents, &len);
		if (!ff)
			return false;

		TFE_Parser parser;
		parser.init(contents, len);
		parser.addCommentString(";");
		parser.addCommentString("#");
		parser.addCommentString("//");

		size_t bufferPos = 0;
		s32 index = 0;
		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }

			TokenList tokens;
			parser.tokenizeLine(line, tokens);
			if (tokens.size() < 1) { continue; }
			const char* item = tokens[0].c_str();

			s_tfeMessage[index] = (char*)malloc(strlen(item) + 1);
			strcpy(s_tfeMessage[index], item);
			index++;
		}
		free(contents);
		s_messagesLoaded = (index == TFE_MSG_COUNT);
		assert(s_messagesLoaded);
		return s_messagesLoaded;
	}
}
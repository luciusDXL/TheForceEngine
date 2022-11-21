#include "tfeMessage.h"
#include "parser.h"
#include <TFE_FileSystem/filestream.h>
#include <assert.h>

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
		FileStream file;
		if (!file.open(path, Stream::MODE_READ))
		{
			return false;
		}

		// Read the file into memory.
		const size_t len = file.getSize();
		char* contents = new char[len + 1];
		if (!contents)
		{
			file.close();
			return false;
		}
		file.readBuffer(contents, (u32)len);
		file.close();

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
		s_messagesLoaded = (index == TFE_MSG_COUNT);
		assert(s_messagesLoaded);
		return s_messagesLoaded;
	}
}
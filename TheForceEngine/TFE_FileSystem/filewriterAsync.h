#pragma once
#include <TFE_System/system.h>

// TODO: Flesh out error codes.
enum AsyncFileWriteCodes
{
	AFW_SUCCESS = 0,
};

typedef void(*FileWriteCompletionCallback)(size_t bytesWritten, void* userData, u32 errorCode);

namespace FileWriterAsync
{
	bool writeFileToDisk(const char* path, u8* data, size_t dataSize, FileWriteCompletionCallback completionCallback = nullptr, void* userData = nullptr);
};

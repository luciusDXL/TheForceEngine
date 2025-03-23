#pragma once
//////////////////////////////////////////////////////////////////////
// OS Specific "shell" code to execute other applications.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
bool osShellExecute(const char* pathToExe, const char* exeDir, const char* param, bool waitForCompletion);
void osShellOpenUrl(const char* url);
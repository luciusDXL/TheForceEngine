#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine System Library
// System functionality, such as timers and logging.
//////////////////////////////////////////////////////////////////////

#include "types.h"

void convertExtendedAsciiToUtf8(const char* str, char* utf);
void convertUtf8ToExtendedAscii(const char* utf, char* str);
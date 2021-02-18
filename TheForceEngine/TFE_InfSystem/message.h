#pragma once
//////////////////////////////////////////////////////////////////////
// Message Address is the way named sectors are flagged during load
// in order to be looked up by the INF system.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <vector>
#include <string>

struct RSector;

struct MessageAddress
{
	char name[16];		// 0x00
	s32  param0;		// 0x10
	s32  param1;		// 0x14
	RSector* sector;	// 0x18
};

namespace Message
{
	void addAddress(const char* name, s32 param0, s32 param1, RSector* sector);
	MessageAddress* getAddress(const char* name);
	void free();
}
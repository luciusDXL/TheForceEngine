#pragma once
#include <TFE_System/types.h>

namespace TFE_MidiDevice
{
	bool init();
	void destroy();

	u32  getDeviceCount();
	void getDeviceName(u32 index, char* buffer, u32 maxLength);
	bool selectDevice(s32 index);

	void sendMessage(const u8* msg, u32 size);
	void sendMessage(u8 arg0, u8 arg1, u8 arg2 = 0);
};

#pragma once
#include <TFE_System/types.h>

enum iMuseCommand
{
	IMUSE_START_NEW = 0,
	IMUSE_STALK_TRANS,
	IMUSE_FIGHT_TRANS,
	IMUSE_ENGAGE_TRANS,
	IMUSE_FROM_FIGHT,
	IMUSE_FROM_STALK,
	IMUSE_FROM_BOSS,
	IMUSE_CLEAR_CALLBACK,
	IMUSE_TO,
	IMUSE_LOOP_START,
	IMUSE_LOOP_END,
	IMUSE_COUNT,
	IMUSE_UNKNOWN = IMUSE_COUNT
};

struct iMuseEventArg
{
	union
	{
		f32 fArg;
		s32 nArg;
	};
};

struct iMuseEvent
{
	iMuseCommand cmd = IMUSE_UNKNOWN;
	iMuseEventArg arg[3] = { 0 };
};

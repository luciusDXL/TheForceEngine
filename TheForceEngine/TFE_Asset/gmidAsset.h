#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Midi Asset
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Audio/midi.h>
#include <string>
#include <vector>

struct MidiEvent
{
	u8  type;
	s8  channel;
	u8  data[2];
};

struct MidiTempoEvent
{
	f32 msPerTick;	// milliseconds per tick.
};

struct MidiMarker
{
	char name[256];
};

enum MidiTrackEventType
{
	MTK_TEMPO = 0,
	MTK_MARKER,
	MTK_MIDI,
	MTK_IMUSE,
	MTK_COUNT
};

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

struct MidiTrackEvent
{
	u32 tick;
	MidiTrackEventType type;
	u32 index;
};

struct Track
{
	char name[256];
	u16 id;
	u16 pad16;
	f32 msPerTick;	// tempo at time 0.
	u32 length;

	s32 loopStart;
	s32 loopEnd;

	u32 ticksPerBeat;
	u32 ticksPerMeasure;

	// Ordered event list.
	std::vector<MidiTrackEvent> eventList;

	// Event type arrays.
	std::vector<MidiTempoEvent> tempoEvents;
	std::vector<MidiEvent>      midiEvents;
	std::vector<iMuseEvent>     imuseEvents;
	std::vector<MidiMarker>     markers;
};

struct GMidiAsset
{
	char name[32];
	u32 trackCount;
	u32 length;

	std::vector<Track> tracks;
};

namespace TFE_GmidAsset
{
	GMidiAsset* get(const char* name);
	void free(GMidiAsset* asset);
	void freeAll();
};

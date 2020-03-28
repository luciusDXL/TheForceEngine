#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Midi Asset
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>
#include <DXL2_Audio/midi.h>
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
	char name[32];
};

struct MidiSysExImuse
{
	u8  id;	//1 or 3?
	char msg[32];
};

enum MidiTrackEventType
{
	MTK_TEMPO = 0,
	MTK_MARKER,
	MTK_MIDI,
	MTK_IMUSE,
	MTK_COUNT
};

struct MidiTrackEvent
{
	u32 tick;
	MidiTrackEventType type;
	u32 index;
};

struct Track
{
	char name[32];
	u16 id;
	u16 pad16;
	f32 msPerTick;	// tempo at time 0.
	u32 length;

	s32 loopStart;
	s32 loopEnd;

	// Ordered event list.
	std::vector<MidiTrackEvent> eventList;

	// Event type arrays.
	std::vector<MidiTempoEvent> tempoEvents;
	std::vector<MidiEvent>      midiEvents;
	std::vector<MidiSysExImuse> imuseEvents;
	std::vector<MidiMarker>     markers;
};

struct GMidiAsset
{
	char name[32];
	u32 trackCount;
	u32 length;

	std::vector<Track> tracks;
};

namespace DXL2_GmidAsset
{
	GMidiAsset* get(const char* name);
	void free(GMidiAsset* asset);
	void freeAll();
};

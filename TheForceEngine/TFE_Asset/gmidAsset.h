#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Midi Asset
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Audio/midi.h>
#include <TFE_Audio/iMuseEvent.h>
#include <string>
#include <vector>

enum MidiTrackEventType
{
	MTK_TEMPO = 0,
	MTK_MARKER,
	MTK_MIDI,
	MTK_IMUSE,
	MTK_COUNT
};

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

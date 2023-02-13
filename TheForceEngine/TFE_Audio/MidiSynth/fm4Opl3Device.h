///////////////////////////////////////////////////////////////
// The low-level driver code that interfaces with the OPL3
// device (virtual device in this case) was reverse-engineered
// from the iMuse driver interface code.
//
// I moved the driver level code into the OPL3 device instead
// of as part of iMuse for several reasons:
// * The OPL3 device can be handled using the same abstraction
//   as the SF2 and System midi devices.
// * It avoids the need to add addition complexity to the iMuse
//   library in TFE, which is already quite large.
///////////////////////////////////////////////////////////////
#pragma once
#include <TFE_System/types.h>
#include <TFE_Audio/midiDevice.h>
#include <TFE_Audio/midi.h>

namespace TFE_Audio
{
	struct TimbreBank;

	class Fm4Opl3Device : public MidiDevice
	{
	public:
		Fm4Opl3Device() : m_streamActive(false), m_volume(1.0f), m_volumeScaled(1.0f), m_fmVoicePitchRight(nullptr), m_fmVoicePitchLeft(nullptr), m_fmVoiceLevel(nullptr) {}
		~Fm4Opl3Device() override;

		MidiDeviceType getType() override { return MIDI_TYPE_OPL3; }

		void exit() override;
		bool hasGlobalVolumeCtrl() override { return true; }
		const char* getName() override;

		bool render(f32* buffer, u32 sampleCount) override;
		bool canRender() override;

		void message(u8 type, u8 arg1, u8 arg2) override;
		void message(const u8* msg, u32 len) override;
		void noteAllOff() override;
		void setVolume(f32 volume) override;

		u32  getOutputCount() override;
		void getOutputName(s32 index, char* buffer, u32 maxLength) override;
		bool selectOutput(s32 index) override;
		s32  getActiveOutput(void) override;

	private:
		enum
		{
			FM4_VoiceCount = 9,
			FM4_RegisterCount = 256,
			FM4_NoteOutputCount = 10,
		};

		enum FmOutputChannel
		{
			FM4_OutRight = 0,
			FM4_OutLeft  = 2,
			FM4_OutCount = 2,
		};

		struct Fm4Channel
		{
			s32 priority;
			s32 noteReq;
			s32 refCount;
			s32 port;

			s32 timbre;
			s32 volume;
			s32 pan;
			s32 pitch;
		};

		struct Fm4Voice
		{
			Fm4Voice* prev;
			Fm4Voice* next;
			Fm4Channel* channel;
			s32 channelId;
			s32 key;
			s32 id;
		};

		struct Fm4VoiceList
		{
			Fm4Voice* active;
			Fm4Voice* free;
		};

		void beginStream(s32 sampleRate);
		// Low level message API.
		void fm4_controlChange(s32 channelId, s32 arg1, s32 arg2);
		void fm4_programChange(s32 channelId, s32 timbre);
		void fm4_noteOn(s32 channel, s32 key, s32 velocity);
		void fm4_noteOff(s32 channel, s32 key);
				
		Fm4Voice* fm4_allocVoice(Fm4Channel* channel);

		void fm4_reset();
		void fm4_sendOutput(s32 port, u16 reg, u8 value, bool force = false);
		void fm4_setVoiceVolume(s32 voice, s32 volume);
		void fm4_setVoicePitch(s32 voice, s32 key, s32 pitchOffset);
		void fm4_setVoiceDelta(s32 voice, s32 l0, s32 l1, s32 l2, s32 l3);
		void fm4_setVoiceTimbre(FmOutputChannel outChannel, s32 voice, TimbreBank* bank);
		s32  fm4_keyToDrumTimbre(s32 key);
		s32  fm4_timbreToLevel(FmOutputChannel outChannel, s32 timbre, s32 remapIndex);
		TimbreBank* fm4_getBankPtr(FmOutputChannel outChannel, s32 timbre);

		void fm4_voiceOn(s32 voice, s32 channelId, s32 key, s32 velocity, s32 timbre, s32 volume, s32 pan, s32 pitch);
		void fm4_voiceOff(s32 voice);

		void fm4_voiceOnSide(FmOutputChannel outChannel, s32 voice, s32 timbre, s32 velocity, s32 volume, s32* output, s32& v0, s32& v1);
	
		bool m_streamActive;
		f32  m_volume;
		f32  m_volumeScaled;
		Fm4Channel m_channels[MIDI_CHANNEL_COUNT];
		Fm4Voice m_voices[FM4_VoiceCount];
		Fm4Voice m_voiceTemp;
		Fm4VoiceList m_voiceList;

		u8  m_registers[FM4_RegisterCount * FM4_OutCount];
		s32 m_noteOutput[FM4_VoiceCount * FM4_NoteOutputCount];

		u8* m_fmVoicePitchRight;
		u8* m_fmVoicePitchLeft;
		u8* m_fmVoiceLevel;
	};
};
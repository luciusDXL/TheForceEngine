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
#include "fm4Opl3Device.h"
#include "fm4Tables.h"
#include "opl3.h"
#include <TFE_Audio/midi.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/IMuse/imList.h>
#include <cstring>
#include <algorithm>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_Audio
{
	// Registers and Values used by the OPL3 driver. Note that this doesn't include everything possible.
	enum Opl3Reg
	{
		REG_MODE_REGISTER       = 0x05,
		REG_KBD_SPLIT_REGISTER  = 0x08,
		REG_ENABLE_WAVE_SELECT  = 0x20,
		REG_KSL_LEVEL           = 0x40,
		REG_ATTACK_DECAY        = 0x60,
		REG_SUSTAIN_RELEASE     = 0x80,
		REG_FNUM_LOW            = 0xa0,
		REG_KEYON_BLOCK         = 0xb0,
		REG_PERCUSSION_REGISTER = 0xbd,
		REG_FEEDBACK_CONNECTION = 0xc0,
		REG_WAVE_SELECT         = 0xe0,
	};
	enum Opl3Value
	{
		VALUE_ENABLE           = 0x01,
		VALUE_KEYON_BIT        = 0x20,
		VALUE_LEVEL_MASK       = 0x3f,
		VALUE_KEYBOARD_SPLIT   = 0x40,

		VALUE_VOICE_LEFT       = 0x10,
		VALUE_VOICE_RIGHT      = 0x20,
	};

	enum InternalConstants
	{
		FM4_Port      = 0x388,
		FM4_MaxVolume = 127,
		FM4_OutputCount = 1,
		FM4_SampleRate  = 44100,
		FM4_DrumChannel = 9,
		FM4_PitchCenter = 0x2000,
		FM4_MaxAtten = 0x3f,
		FM4_TimbreCount = 167,
		FM4_BankRemapMax = 27,
		FM4_BankCenter = 68,
	};
	const f32 c_outputScale = 1.5f / 32768.0f;	// slight volume boost to compete with other midi outputs.

	static const char* c_Opl3_Name = "OPL3";
	static const char* c_Output_Name = "FM4 Driver";

	// This is stored internally, there will only be one FM chip (for now).
	static opl3_chip s_fmChip = { 0 };

	Fm4Opl3Device::~Fm4Opl3Device()
	{
		exit();
	}

	u32 Fm4Opl3Device::getOutputCount()
	{
		return FM4_OutputCount;
	}

	void Fm4Opl3Device::getOutputName(s32 index, char* buffer, u32 maxLength)
	{
		// Only output 0 exists.
		strncpy(buffer, c_Output_Name, maxLength);
		buffer[strlen(c_Output_Name)] = 0;
	}

	bool Fm4Opl3Device::selectOutput(s32 index)
	{
		if (!m_streamActive)
		{
			beginStream(FM4_SampleRate);
		}
		return m_streamActive;
	}

	s32 Fm4Opl3Device::getActiveOutput(void)
	{
		// Only output 0 exists.
		return 0;
	}

	void Fm4Opl3Device::fm4_reset()
	{
		// Reset all of the registers.
		for (s32 i = 0; i < FM4_RegisterCount; i++)
		{
			fm4_sendOutput(FM4_OutLeft,  i, 0);
			fm4_sendOutput(FM4_OutRight, i, 0);
		}
		// Initialize and enable.
		fm4_sendOutput(FM4_OutRight, REG_KBD_SPLIT_REGISTER, VALUE_KEYBOARD_SPLIT);
		fm4_sendOutput(FM4_OutRight, REG_PERCUSSION_REGISTER, 0x00);
		fm4_sendOutput(FM4_OutLeft,  REG_MODE_REGISTER, VALUE_ENABLE);
	}

	void Fm4Opl3Device::beginStream(s32 sampleRate)
	{
		assert(!m_streamActive);
		s_fmChip = { 0 };
		memset(m_registers, 0, FM4_RegisterCount * FM4_OutCount);

		OPL3_Reset(&s_fmChip, sampleRate);
		fm4_reset();

		// Initialize channels
		for (s32 i = 0; i < MIDI_CHANNEL_COUNT; i++)
		{
			m_channels[i].priority = 0;
			m_channels[i].noteReq  = 1;
			m_channels[i].refCount = 0;
			m_channels[i].port     = 0;
			m_channels[i].timbre   = 0;
			m_channels[i].volume   = 0x7f;
			m_channels[i].pan      = 64;
			m_channels[i].pitch    = FM4_PitchCenter;
		}
		m_fmVoicePitchRight = &m_registers[REG_KEYON_BLOCK];
		m_fmVoicePitchLeft  = &m_registers[REG_KEYON_BLOCK + FM4_RegisterCount];
		m_fmVoiceLevel      = &m_registers[REG_KSL_LEVEL];

		// Initialize the voice list.
		m_voiceList.active = nullptr;
		m_voiceList.free   = nullptr;
		memset(m_voices, 0, sizeof(Fm4Voice) * FM4_VoiceCount);
		for (s32 i = 0; i < FM4_VoiceCount; i++)
		{
			m_voices[i].id = i;
			IM_LIST_ADD(m_voiceList.free, &m_voices[i]);
		}

		m_volumeScaled = m_volume * c_outputScale;
		m_streamActive = true;
	}

	void Fm4Opl3Device::exit()
	{
		s_fmChip = { 0 };
		m_streamActive = false;
	}
		
	const char* Fm4Opl3Device::getName()
	{
		return c_Opl3_Name;
	}

	bool Fm4Opl3Device::render(f32* buffer, u32 sampleCount)
	{
		if (!m_streamActive) { return false; }

		for (u32 i = 0; i < sampleCount; i++)
		{
			s16 buf[2];
			OPL3_GenerateResampled(&s_fmChip, buf);
			
			s16 left  = clamp(s32(buf[0]), INT16_MIN, INT16_MAX);
			s16 right = clamp(s32(buf[1]), INT16_MIN, INT16_MAX);
			*buffer++ = f32(left)  * m_volumeScaled;
			*buffer++ = f32(right) * m_volumeScaled;
		}
		return true;
	}

	bool Fm4Opl3Device::canRender()
	{
		return m_streamActive;
	}

	void Fm4Opl3Device::setVolume(f32 volume)
	{
		m_volume = volume;
		m_volumeScaled = m_volume * c_outputScale;
	}

	// Raw midi commands.
	void Fm4Opl3Device::message(u8 type, u8 arg1, u8 arg2)
	{
		if (!m_streamActive) { return; }
		const u8 msgType = type & 0xf0;
		const u8 channel = type & 0x0f;

		switch (msgType)
		{
		case MID_NOTE_OFF:
			fm4_noteOff(channel, arg1);
			break;
		case MID_NOTE_ON:
			fm4_noteOn(channel, arg1, arg2);
			break;
		case MID_CONTROL_CHANGE:
			fm4_controlChange(channel, arg1, arg2);
			break;
		case MID_PROGRAM_CHANGE:
			fm4_programChange(channel, arg1);
			break;
		case MID_PITCH_BEND:
			// No-op
			break;
		}
	}

	void Fm4Opl3Device::message(const u8* msg, u32 len)
	{
		message(msg[0], msg[1], len >= 2 ? msg[2] : 0);
	}

	void Fm4Opl3Device::noteAllOff()
	{
		if (!m_streamActive) { return; }

		Fm4Voice* voice = m_voiceList.active;
		while (voice)
		{
			Fm4Voice* next = voice->next;

			fm4_voiceOff(voice->id);
			IM_LIST_REM(m_voiceList.active, voice);
			IM_LIST_ADD(m_voiceList.free, voice);

			voice = next;
		}

		for (s32 i = 0; i < MIDI_CHANNEL_COUNT; i++)
		{
			m_channels[i].refCount = 0;
			m_channels[i].noteReq = 1;
		}
	}
		
	/////////////////////////////////////////////
	// Low-level driver implementation.
	/////////////////////////////////////////////
	s32 Fm4Opl3Device::fm4_getVelocityToVolumeMapping(s32 velocity)
	{
		assert(velocity >= 0 && velocity < 64);
		return s_fmVelocityToVolumeMapping[velocity];
	}

	void Fm4Opl3Device::fm4_setVoiceVolumeSide(FmOutputChannel outChannel, s32 voice, s32 offset, s32 volume)
	{
		const s32 outIndex  = outChannel + NoteLevelRight;
		const s32 regOffset = outChannel * FM4_RegisterCount;
		const s32 level = m_noteOutput[voice*NoteOut_Count + outIndex];
		const s32 fullVolume = fm4_getVelocityToVolumeMapping(((level + 1) * volume) >> 7);

		// The new level factors in the previous level.
		const s32 prevLevel = m_fmVoiceLevel[offset + regOffset];
		const s32 newLevel  = (prevLevel | FM4_MaxAtten) - fullVolume;
		// This will update m_fmVoiceLevel[offset] to the new level since m_fmVoiceLevel is a pointer into the register array.
		fm4_sendOutput(outChannel, offset + REG_KSL_LEVEL, newLevel);
	}

	void Fm4Opl3Device::fm4_setVoiceVolume(s32 voice, s32 volume)
	{
		const s32 offset = s_fmVoiceOperators[1][voice];
		fm4_setVoiceVolumeSide(FM4_OutRight, voice, offset, volume);
		fm4_setVoiceVolumeSide(FM4_OutLeft,  voice, offset, volume);

		s32* output = &m_noteOutput[voice*NoteOut_Count];
		if (output[NoteTimbre0_Right] == 1)
		{
			assert(0);
		}
		if (output[NoteTimbre0_Left] == 1)
		{
			assert(0);
		}
	}

	void Fm4Opl3Device::fm4_controlChange(s32 channelId, s32 arg1, s32 arg2)
	{
		Fm4Channel* channel = &m_channels[channelId];
		Fm4Voice* voice = m_voiceList.active;
		switch (arg1)
		{
			case MID_VOLUME_MSB:
			{
				channel->volume = arg2;
				while (voice)
				{
					if (voice->channelId == channelId)
					{
						fm4_setVoiceVolume(voice->id, arg2);
					}
					voice = voice->next;
				}
			} break;
			case MID_PAN_MSB:
			{
				channel->pan = arg2;
			} break;
			case MID_GPC2_MSB:
			{
				channel->noteReq = arg2;
			} break;
			case MID_GPC3_MSB:
			{
				channel->priority = arg2;
			} break;
		}
	}

	void Fm4Opl3Device::fm4_programChange(s32 channelId, s32 timbre)
	{
		m_channels[channelId].timbre = timbre;
	}

	void Fm4Opl3Device::fm4_noteOn(s32 channelId, s32 key, s32 velocity)
	{
		Fm4Channel* channel = &m_channels[channelId];

		if (channelId == FM4_DrumChannel)
		{
			channel->timbre = fm4_keyToDrumTimbre(key);
			if (channel->timbre >= 128)
			{
				channel->pitch = FM4_PitchCenter;
			}
		}

		Fm4Voice* voice = fm4_allocVoice(channel);
		if (!voice) { return; }
		voice->channel = channel;
		voice->channelId = channelId;
		voice->key = key;

		channel->refCount++;
		channel->port = channel->refCount > channel->noteReq ? 1 : 0;
		fm4_voiceOn(voice->id, channelId, key, velocity, channel->timbre, channel->volume, channel->pan, channel->pitch);
	}

	void Fm4Opl3Device::fm4_noteOff(s32 channelId, s32 key)
	{
		Fm4Voice* voice = m_voiceList.active;
		while (voice)
		{
			Fm4Voice* next = voice->next;
			if (channelId == voice->channelId && voice->key == key)
			{
				fm4_voiceOff(voice->id);
				m_channels[channelId].refCount--;
				m_channels[channelId].port = m_channels[channelId].refCount > m_channels[channelId].noteReq ? 1 : 0;

				IM_LIST_REM(m_voiceList.active, voice);
				IM_LIST_ADD(m_voiceList.free, voice);
				break;
			}
			voice = next;
		}
	}

	s32 Fm4Opl3Device::fm4_keyToDrumTimbre(s32 key)
	{
		return (s8)s_fmKeyToDrumTimbre[key] + 128;
	}

	Fm4Opl3Device::Fm4Voice* Fm4Opl3Device::fm4_allocVoice(Fm4Channel* channel)
	{
		Fm4Voice* voice = m_voiceList.free;
		Fm4Voice* newVoice = nullptr;
		while (voice)
		{
			Fm4Voice* next = voice->next;
			if (next)
			{
				voice = next;
			}
			else
			{
				IM_LIST_REM(m_voiceList.free, voice);
				newVoice = voice;
				break;
			}
		}

		if (!newVoice)
		{
			Fm4Voice* bestChoice = &m_voiceTemp;
			bestChoice->channel = channel;
			voice = m_voiceList.active;

			while (voice)
			{
				Fm4Channel* vchannel = voice->channel;
				Fm4Channel* bchannel = bestChoice->channel;

				if (vchannel->port == bchannel->port)
				{
					if (bchannel->priority >= vchannel->priority)
					{
						bestChoice = voice;
					}
				}
				else if (vchannel->port)
				{
					bestChoice = voice;
				}
				voice = voice->next;
			}

			if (bestChoice != &m_voiceTemp)
			{
				fm4_voiceOff(bestChoice->id);
				m_channels[bestChoice->channelId].refCount--;
				m_channels[bestChoice->channelId].port = m_channels[bestChoice->channelId].refCount > m_channels[bestChoice->channelId].noteReq ? 1 : 0;

				IM_LIST_REM(m_voiceList.active, bestChoice);
				newVoice = bestChoice;
			}
		}

		if (newVoice)
		{
			IM_LIST_ADD(m_voiceList.active, newVoice);
		}
		return newVoice;
	}

	void Fm4Opl3Device::fm4_voiceOff(s32 voice)
	{
		fm4_sendOutput(FM4_OutRight, voice + REG_KEYON_BLOCK, m_fmVoicePitchRight[voice] & 0xdfu);
		fm4_sendOutput(FM4_OutLeft,  voice + REG_KEYON_BLOCK, m_fmVoicePitchLeft[voice]  & 0xdfu);
	}

	void Fm4Opl3Device::fm4_sendOutput(s32 port, u16 reg, u8 value)
	{
		const u32 regIndex = reg + port*FM4_RegisterCount;
		if (m_registers[regIndex] == value) { return; }

		m_registers[regIndex] = value;
		OPL3_WriteRegBuffered(&s_fmChip, regIndex, value);
	}

	void Fm4Opl3Device::fm4_setVoicePitch(s32 voice, s32 key, s32 pitchOffset)
	{
		s32 index     = max(0, key + (pitchOffset >> 8) - 7);
		u8  freqIndex = s_fmKeyPitchToFreqIndex[index] + ((pitchOffset >> 4) & 0x0f);
		s32 pitch     = s_fmFreqIndexToPitch[freqIndex];
		s32 pitchLo   = pitch & 0xff;
		s32 pitchHi   = ((m_fmVoicePitchRight[voice] & 0x20) | (s_fmKeyPitchHi[index] << 2)) + (pitch >> 8);

		fm4_sendOutput(FM4_OutRight, voice + REG_FNUM_LOW,    pitchLo);
		fm4_sendOutput(FM4_OutRight, voice + REG_KEYON_BLOCK, pitchHi);
		fm4_sendOutput(FM4_OutLeft,  voice + REG_FNUM_LOW,    pitchLo);
		fm4_sendOutput(FM4_OutLeft,  voice + REG_KEYON_BLOCK, pitchHi);
	}

	void Fm4Opl3Device::fm4_setVoiceDelta(s32 voice, s32 l0, s32 l1, s32 l2, s32 l3)
	{
		const s32 offOp1 = REG_KSL_LEVEL + s_fmVoiceOperators[0][voice];
		const s32 offOp2 = REG_KSL_LEVEL + s_fmVoiceOperators[1][voice];

		const s32 delta0 = m_registers[offOp1] - l0;
		const s32 delta1 = m_registers[offOp2] - l1;
		const s32 delta2 = m_registers[offOp1 + FM4_RegisterCount] - l2;
		const s32 delta3 = m_registers[offOp2 + FM4_RegisterCount] - l3;

		fm4_sendOutput(FM4_OutRight, offOp1, delta0);
		fm4_sendOutput(FM4_OutRight, offOp2, delta1);
		fm4_sendOutput(FM4_OutLeft,  offOp1, delta2);
		fm4_sendOutput(FM4_OutLeft,  offOp2, delta3);

		fm4_sendOutput(FM4_OutRight, voice + REG_KEYON_BLOCK, (m_fmVoicePitchRight[voice] | VALUE_KEYON_BIT) & 0xffu);
		fm4_sendOutput(FM4_OutLeft,  voice + REG_KEYON_BLOCK, (m_fmVoicePitchLeft[voice]  | VALUE_KEYON_BIT) & 0xffu);
	}
		
	void Fm4Opl3Device::fm4_setVoiceTimbre(FmOutputChannel outChannel, s32 voice, TimbreBank* bank)
	{
		s32 offOp1 = s_fmVoiceOperators[0][voice];
		s32 offOp2 = s_fmVoiceOperators[1][voice];
		fm4_sendOutput(outChannel, REG_KSL_LEVEL + offOp1, VALUE_LEVEL_MASK);
		fm4_sendOutput(outChannel, REG_KSL_LEVEL + offOp2, VALUE_LEVEL_MASK);

		// Op1
		fm4_sendOutput(outChannel, REG_ENABLE_WAVE_SELECT + offOp1, bank->op1.waveEnable);
		fm4_sendOutput(outChannel, REG_KSL_LEVEL          + offOp1, bank->op1.level | VALUE_LEVEL_MASK);
		fm4_sendOutput(outChannel, REG_ATTACK_DECAY       + offOp1, (~u32(bank->op1.attackDelay))&0xffu);
		fm4_sendOutput(outChannel, REG_SUSTAIN_RELEASE    + offOp1, (~u32(bank->op1.sustainRelease))&0xffu);
		fm4_sendOutput(outChannel, REG_WAVE_SELECT        + offOp1, bank->op1.waveSelect);

		// Op2
		fm4_sendOutput(outChannel, REG_ENABLE_WAVE_SELECT + offOp2, bank->op2.waveEnable);
		fm4_sendOutput(outChannel, REG_KSL_LEVEL          + offOp2, bank->op2.level | VALUE_LEVEL_MASK);
		fm4_sendOutput(outChannel, REG_ATTACK_DECAY       + offOp2, (~u32(bank->op2.attackDelay))&0xffu);
		fm4_sendOutput(outChannel, REG_SUSTAIN_RELEASE    + offOp2, (~u32(bank->op2.sustainRelease))&0xffu);
		fm4_sendOutput(outChannel, REG_WAVE_SELECT        + offOp2, bank->op2.waveSelect);

		fm4_sendOutput(outChannel, REG_FEEDBACK_CONNECTION + voice, bank->feedback | (outChannel == FM4_OutRight ? VALUE_VOICE_RIGHT : VALUE_VOICE_LEFT));
	}

	s32 Fm4Opl3Device::fm4_timbreToLevel(FmOutputChannel outChannel, s32 timbre, s32 remapIndex)
	{
		if (timbre < FM4_TimbreCount)
		{
			const TimbreBank* bank = fm4_getBankPtr(outChannel, timbre);
			if (remapIndex <= FM4_BankRemapMax)
			{
				s32 offset = s_fmBankAdjust[remapIndex].offset;
				s32 div    = s_fmBankAdjust[remapIndex].div;
				s32 mask   = s_fmBankAdjust[remapIndex].mask;
				return (bank->data[offset] & mask) >> div;
			}
			else if (remapIndex == FM4_BankCenter)
			{
				return bank->centerLevel;
			}
		}
		return 0;
	}

	TimbreBank* Fm4Opl3Device::fm4_getBankPtr(FmOutputChannel outChannel, s32 timbre)
	{
		assert(timbre < FM4_TimbreCount);
		TimbreBank* bank = (outChannel == FM4_OutRight) ? s_timbreBanksRight : s_timbreBanksLeft;
		return &bank[timbre];
	}
		
	void Fm4Opl3Device::fm4_voiceOnSide(FmOutputChannel outChannel, s32 voice, s32 timbre, s32 velocity, s32 volume, s32* output, s32& v0, s32& v1)
	{
		const s32 offset = outChannel;

		output[NoteTimbre0_Right+offset] =  fm4_timbreToLevel(outChannel, timbre, FM4_BankRemapMax-1);
		output[NoteTimbre1_Right+offset] = (fm4_timbreToLevel(outChannel, timbre, FM4_BankCenter) << 2) - 1;

		v1  = ((fm4_timbreToLevel(outChannel, timbre, 1) + 1) * velocity) >> 6;
		v1 += fm4_timbreToLevel(outChannel, timbre, 0);
		v1  = min(v1, FM4_MaxAtten);
		output[NoteLevelRight +offset] = v1;

		v0  = ((fm4_timbreToLevel(outChannel, timbre, 14) + 1) * velocity) >> 6;
		v0 += fm4_timbreToLevel(outChannel, timbre, 13);
		v0  = min(v0, FM4_MaxAtten);
		output[NoteLevel2Right+offset] = v0;

		v1 = fm4_getVelocityToVolumeMapping(((v1 + 1) * volume) >> 7);
		if (output[2+offset] == 1)
		{
			v0 = fm4_getVelocityToVolumeMapping(((v0 + 1) * volume) >> 7);
		}
		TimbreBank* bankPtr = fm4_getBankPtr(outChannel, timbre);
		fm4_setVoiceTimbre(outChannel, voice, bankPtr);
	}
		
	void Fm4Opl3Device::fm4_voiceOn(s32 voice, s32 channelId, s32 key, s32 velocity, s32 timbre, s32 volume, s32 pan, s32 pitch)
	{
		// Remap velocity.
		assert((velocity >> 1) < 64);
		velocity = fm4_getVelocityToVolumeMapping(velocity >> 1) * 2;

		s32* output = &m_noteOutput[voice * NoteOut_Count];
		output[NoteChannelId] = channelId;
		output[NoteKey] = key;

		s32 v0, v1, v2, v3;
		fm4_voiceOnSide(FM4_OutRight, voice, timbre, velocity, volume, output, v0, v1);
		fm4_voiceOnSide(FM4_OutLeft,  voice, timbre, velocity, volume, output, v2, v3);

		// Pitch
		s32 pitchOffset = (pitch - FM4_PitchCenter) >> 1;
		fm4_setVoicePitch(voice, key, pitchOffset);
		fm4_setVoiceDelta(voice, v0, v1, v2, v3);
	}
};
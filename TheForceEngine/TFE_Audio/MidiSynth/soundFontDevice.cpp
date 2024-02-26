#include "soundFontDevice.h"
#include <TFE_Audio/midi.h>
#include <TFE_FileSystem/filestream.h>
#include <algorithm>
#include <assert.h>

#define TSF_IMPLEMENTATION
#include "tsf.h"

namespace TFE_Audio
{
	enum Constants
	{
		SFD_MAX_VOICES   = 512,
		SFD_DRUM_CHANNEL = 9,
		SFD_DRUM_BANK    = 128,
		SFD_SAMPLE_RATE  = 44100,
	};
	static const char* c_SFD_Name = "SF2 Synthesized Midi";
	static const char* c_defaultOutput = "Roland SC-55";

	SoundFontDevice::~SoundFontDevice()
	{
		exit();
	}

	u32 SoundFontDevice::getOutputCount()
	{
		if (m_outputs.empty())
		{
			char dir[TFE_MAX_PATH];
			const char* programDir = TFE_Paths::getPath(PATH_PROGRAM);
			sprintf(dir, "%s", "SoundFonts/");
			if (!TFE_Paths::mapSystemPath(dir))
				sprintf(dir, "%sSoundFonts/", programDir);

			FileUtil::readDirectory(dir, "sf2", m_outputs);
			// Remove the extension.
			for (size_t i = 0; i < m_outputs.size(); i++)
			{
				char name[TFE_MAX_PATH];
				FileUtil::getFileNameFromPath(m_outputs[i].c_str(), name);
				m_outputs[i] = name;
			}
		}
		return (u32)m_outputs.size();
	}

	void SoundFontDevice::getOutputName(s32 index, char* buffer, u32 maxLength)
	{
		if (index < 0 || index >= (s32)getOutputCount()) { return; }
		const char* name = m_outputs[index].c_str();
		strncpy(buffer, name, maxLength);
		buffer[strlen(name)] = 0;
	}

	bool SoundFontDevice::selectOutput(s32 index)
	{
		u32 outputCount = getOutputCount();
		if (index < 0 || index >= (s32)outputCount)
		{
			// Find the default, revert to 0 if not found.
			index = 0;
			for (u32 i = 0; i < outputCount; i++)
			{
				if (strcasecmp(c_defaultOutput, m_outputs[i].c_str()) == 0)
				{
					index = i;
					break;
				}
			}
		}
		bool res = false;
		if (index != m_outputId)
		{
			char outputName[TFE_MAX_PATH];
			getOutputName(index, outputName, TFE_MAX_PATH);
			m_outputId = index;

			exit();
			res = beginStream(outputName, SFD_SAMPLE_RATE);
		}
		return res;
	}

	s32 SoundFontDevice::getActiveOutput(void)
	{
		return m_outputId;
	}

	bool SoundFontDevice::beginStream(const char* soundFont, s32 sampleRate)
	{
		getOutputCount();

		char filePath[TFE_MAX_PATH];
		const char* programDir = TFE_Paths::getPath(PATH_PROGRAM);
		sprintf(filePath, "SoundFonts/%s.sf2", soundFont);
		if (!TFE_Paths::mapSystemPath(filePath))
			sprintf(filePath, "%sSoundFonts/%s.sf2", programDir, soundFont);

		m_soundFont = tsf_load_filename(filePath);
		if (m_soundFont)
		{
			// Set the SoundFont rendering output mode
			tsf_set_output(m_soundFont, TSF_STEREO_INTERLEAVED, TSF_INTERP_CUBIC, sampleRate, 0);
			tsf_set_max_voices(m_soundFont, SFD_MAX_VOICES);
			// pre-allocate channels, clear programs or set them to the stored values.
			for (s32 i = 0; i < MIDI_CHANNEL_COUNT; i++)
			{
				tsf_channel_set_presetnumber(m_soundFont, i, 0, i == SFD_DRUM_CHANNEL);
			}
			// Set the drum bank.
			tsf_channel_set_bank_preset(m_soundFont, SFD_DRUM_CHANNEL, SFD_DRUM_BANK, 0);
		}
		return m_soundFont != nullptr;
	}

	void SoundFontDevice::exit()
	{
		if (m_soundFont)
		{
			tsf* soundFont = m_soundFont;
			m_soundFont = nullptr;

			tsf_reset(soundFont);
			tsf_close(soundFont);
		}
	}
		
	const char* SoundFontDevice::getName()
	{
		return c_SFD_Name;
	}

	bool SoundFontDevice::render(f32* buffer, u32 sampleCount)
	{
		if (!m_soundFont) { return false; }
		tsf_render_float(m_soundFont, buffer, sampleCount);
		return true;
	}

	bool SoundFontDevice::canRender()
	{
		return m_soundFont != nullptr;
	}

	void SoundFontDevice::setVolume(f32 volume)
	{
		if (!m_soundFont) { return; }
		tsf_set_volume(m_soundFont, volume);
	}

	// Raw midi commands.
	void SoundFontDevice::message(u8 type, u8 arg1, u8 arg2)
	{
		if (!m_soundFont) { return; }
		const u8 msgType = type & 0xf0;
		const u8 channel = type & 0x0f;

		switch (msgType)
		{
		case MID_NOTE_OFF:
			tsf_channel_note_off(m_soundFont, channel, arg1);
			break;
		case MID_NOTE_ON:
			tsf_channel_note_on(m_soundFont, channel, arg1, f32(arg2) / 127.0f);
			break;
		case MID_CONTROL_CHANGE:
			tsf_channel_midi_control(m_soundFont, channel, arg1, arg2);
			break;
		case MID_PROGRAM_CHANGE:
			tsf_channel_set_presetnumber(m_soundFont, channel, arg1, channel == SFD_DRUM_CHANNEL ? 1 : 0);
			break;
		case MID_PITCH_BEND:
			tsf_channel_set_pitchwheel(m_soundFont, channel, (s32(arg2) << 7) | s32(arg1));
			break;
		}
	}

	void SoundFontDevice::message(const u8* msg, u32 len)
	{
		message(msg[0], msg[1], len >= 2 ? msg[2] : 0);
	}

	void SoundFontDevice::noteAllOff()
	{
		if (!m_soundFont) { return; }
		tsf_note_off_all(m_soundFont);
	}
};
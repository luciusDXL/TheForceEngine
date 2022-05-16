#include "imuse.h"
#include "imSoundFader.h"
#include <TFE_System/system.h>
#include <TFE_Audio/midi.h>
#include <assert.h>

namespace TFE_Jedi
{
	////////////////////////////////////////////////////
	// Structures
	////////////////////////////////////////////////////
	struct ImSoundFader
	{
		s32 active;
		ImSoundId soundId;
		iMuseParameter param;
		s32 curValue;
		s32 timeRem;
		s32 fadeTime;
		s32 signedStep;
		s32 unsignedStep;
		s32 errAccum;
		s32 stepDir;
	};

	/////////////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////////////
	static s32 s_imFadersActive = 0;
	static ImSoundFader s_soundFaders[MIDI_CHANNEL_COUNT];

	/////////////////////////////////////////////////////////// 
	// API
	/////////////////////////////////////////////////////////// 
	void ImUpdateSoundFaders()
	{
		ImSoundFader* fader = s_soundFaders;
		if (s_imFadersActive)
		{
			s_imFadersActive = 0;
			for (s32 i = 0; i < 16; i++, fader++)
			{
				if (fader->active)
				{
					s_imFadersActive = 1;
					fader->timeRem--;
					if (fader->timeRem == 0)
					{
						fader->active = 0;
					}

					s32 nextValue = fader->curValue + fader->signedStep;
					fader->errAccum += fader->unsignedStep;
					if (fader->errAccum >= fader->fadeTime)
					{
						fader->errAccum -= fader->fadeTime;
						nextValue += fader->stepDir;
					}

					if (nextValue != fader->curValue)
					{
						fader->curValue = nextValue;
						// Only update the parameter on the '0' ticks - i.e. once every 6 'frames'.
						// Note this should always hit on the last frame ('0') - so fades should always work.
						if ((fader->timeRem % 6) == 0)
						{
							if (fader->param == soundVol && nextValue == 0)
							{
								ImStopSound(fader->soundId);
							}
							else
							{
								ImSetParam(fader->soundId, fader->param, nextValue);
							}
						}
					}
				}
			}
		}
	}

	s32 ImClearAllSoundFaders()
	{
		for (s32 i = 0; i < MIDI_CHANNEL_COUNT; i++)
		{
			s_soundFaders[i].active = 0;
		}
		s_imFadersActive = 0;
		return imSuccess;
	}

	void ImClearSoundFaders(ImSoundId soundId, s32 param)
	{
		ImSoundFader* fader = s_soundFaders;
		for (s32 i = 0; i < MIDI_CHANNEL_COUNT; i++, fader++)
		{
			if (fader->active)
			{
				if (soundId != fader->soundId) { continue; }

				if (iMuseParameter(param) == fader->param || param == -1)
				{
					fader->active = 0;
				}
			}
		}
	}

	// soundID is the sound (digital or midi) to adjust.
	// param is of type 'iMuseParameter'
	// value is the target value for the parameter (such as volume).
	// time is the amount of time this should take, 0 for instant changes (negative values are invalid).
	//   time is in "iMuse frames" = 1/60th of a sec. per frame.
	s32 ImFadeParam(ImSoundId soundId, s32 param, s32 value, s32 time)
	{
		// Check that the sound and time are valid.
		if (!soundId || time < 0)
		{
			return imArgErr;
		}
		// Check to see if the parameter is fadable.
		if (param != soundPriority && param != soundVol && param != soundPan && param != soundDetune && param != midiSpeed && (param & 0xff00) != midiPartTrim)
		{
			return imArgErr;
		}

		// Clear any matching sound faders.
		ImClearSoundFaders(soundId, param);

		// If the 'time' is 0, the effect should happen instantly.
		if (time == 0)
		{
			if (param == soundVol && value == 0)
			{
				ImStopSound(soundId);
			}
			else
			{
				ImSetParam(soundId, param, value);
			}
			return imSuccess;
		}

		// Otherwise find a free fader and set it up.
		ImSoundFader* fader = s_soundFaders;
		for (s32 i = 0; i < MIDI_CHANNEL_COUNT; i++, fader++)
		{
			if (!fader->active)
			{
				fader->soundId = soundId;
				fader->param = iMuseParameter(param);
				fader->curValue = ImGetParam(soundId, param);
				fader->fadeTime = time;
				fader->timeRem = time;

				s32 valueChange = value - fader->curValue;
				fader->signedStep = valueChange / time;

				s32 stepDir = 1;
				s32 absValueChange = valueChange;
				if (valueChange < 0)
				{
					stepDir = -1;
					absValueChange = -valueChange;
				}
				fader->stepDir = stepDir;
				// Adsolute value, change over time.
				fader->unsignedStep = absValueChange / time;

				fader->errAccum = 0;
				fader->active = 1;
				s_imFadersActive = 1;
				return imSuccess;
			}
		}

		// If we get here, no free fader was found, so return an error.
		IM_LOG_WRN("fd unable to alloc fade.");
		return imAllocErr;
	}

}  // namespace TFE_Jedi

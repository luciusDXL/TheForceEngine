#include "midiPlayer.h"
#include "midiDevice.h"
#include <TFE_Asset/gmidAsset.h>
#include <TFE_System/system.h>
#include <TFE_System/Threads/thread.h>
#include <TFE_Settings/settings.h>
#include <TFE_FrontEndUI/console.h>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#undef min
#undef max
#endif

namespace TFE_MidiPlayer
{
	struct MidiRuntimeTrack
	{
		f32 msPerTick;
		f64 curTick;
		s32 lastEvent;
	};

	struct MidiRuntime
	{
		const GMidiAsset* asset;
		MidiRuntimeTrack tracks[8];
		bool loop;
	};

	enum Transition
	{
		TRANSITION_NONE = 0,
		TRANSITION_TO_FIGHT,
		TRANSITION_TO_BOSS,
		TRANSITION_TO_STALK,
		TRANSITION_COUNT
	};

	static const f32 c_musicVolumeScale = 0.5f;
	static MidiRuntime s_runtime;
	static atomic_bool s_isPlaying;
	static atomic_bool s_changeVolume;
	static atomic_bool s_pauseMusic;
	static atomic_u32  s_transition;
	static f32 s_masterVolume = 1.0f;
	static f32 s_masterVolumeScaled = s_masterVolume * c_musicVolumeScale;
	static Thread* s_thread = nullptr;

	static atomic_bool s_runMusicThread;
	static atomic_bool s_resetThreadLocalTime;

	static u8 s_channelSrcVolume[16] = { 0 };
		
	TFE_THREADRET midiUpdateFunc(void* userData);
	void stopAllNotes();
	void changeVolume();

	// Console Functions
	void setMusicVolumeConsole(const ConsoleArgList& args);
	void getMusicVolumeConsole(const ConsoleArgList& args);

	bool init()
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_MidiPlayer::init");

		bool res = TFE_MidiDevice::init();
		TFE_MidiDevice::selectDevice(0);
		s_runMusicThread.store(true);
		s_isPlaying.store(false);
		s_pauseMusic.store(false);
		s_transition.store(TRANSITION_NONE);
		s_resetThreadLocalTime.store(true);

		s_thread = Thread::create("MidiThread", midiUpdateFunc, nullptr);
		if (s_thread)
		{
			s_thread->run();
		}

		CCMD("setMusicVolume", setMusicVolumeConsole, 1, "Sets the music volume, range is 0.0 to 1.0");
		CCMD("getMusicVolume", getMusicVolumeConsole, 0, "Get the current music volume where 0 = silent, 1 = maximum.");

		TFE_Settings_Sound* soundSettings = TFE_Settings::getSoundSettings();
		setVolume(soundSettings->musicVolume);

		return res && s_thread;
	}

	void destroy()
	{
		TFE_System::logWrite(LOG_MSG, "MidiPlayer", "Shutdown");
		// Destroy the thread before shutting down the Midi Device.
		stop();
		s_runMusicThread.store(false);
		if (s_thread->isPaused())
		{
			s_thread->resume();
		}
		s_thread->waitOnExit();

		delete s_thread;
		TFE_MidiDevice::destroy();
	}

	void playSong(const GMidiAsset* gmidAsset, bool loop)
	{
		s_runtime.asset = gmidAsset;
		s_runtime.loop = loop;
		for (u32 i = 0; i < gmidAsset->trackCount; i++)
		{
			s_runtime.tracks[i].curTick   = 0;
			s_runtime.tracks[i].lastEvent = -1;
			s_runtime.tracks[i].msPerTick = gmidAsset->tracks[i].msPerTick;
		}

		for (u32 i = 0; i < 16; i++)
		{
			s_channelSrcVolume[i] = CHANNEL_MAX_VOLUME;
		}
		changeVolume();

		s_isPlaying.store(true);
		s_resetThreadLocalTime.store(true);

		if (s_thread->isPaused())
		{
			s_thread->resume();
		}
	}
	
	void setVolume(f32 volume)
	{
		s_masterVolume = volume;
		s_masterVolumeScaled = volume * c_musicVolumeScale;
	}

	f32 getVolume()
	{
		return s_masterVolume;
	}

	void pause()
	{
		s_pauseMusic.store(true);
		stopAllNotes();
	}

	void resume()
	{
		s_pauseMusic.store(false);
	}
	
	void stop()
	{
		s_isPlaying.store(false);
		resume();
	}

	void changeVolume()
	{
		for (u32 i = 0; i < 16; i++)
		{
			TFE_MidiDevice::sendMessage(MID_CONTROL_CHANGE + i, MID_VOLUME_MSB, u8(s_channelSrcVolume[i] * s_masterVolumeScaled));
		}
	}

	void stopAllNotes()
	{
		for (u32 i = 0; i < 16; i++)
		{
			TFE_MidiDevice::sendMessage(MID_CONTROL_CHANGE + i, MID_ALL_NOTES_OFF);
		}
	}
		
	// Thread Function
	TFE_THREADRET midiUpdateFunc(void* userData)
	{
		bool runThread = true;
		bool wasPlaying = false;
		s32 loopStart = -1;
		u64 localTime = 0;
		while (runThread)
		{
			if (!s_isPlaying.load())
			{
				if (wasPlaying)
				{
					// Stop all of the notes.
					stopAllNotes();
					wasPlaying = false;
					localTime = 0u;
					loopStart = -1;
				}

				runThread = s_runMusicThread.load();
				if (runThread) { TFE_System::sleep(16); }
				continue;
			}
			wasPlaying = true;

			if (s_changeVolume.exchange(false))
			{
				changeVolume();
			}

			if (s_pauseMusic.load())
			{
				TFE_System::updateThreadLocal(&localTime);
				continue;
			}

			// Returns the current value while atomically updating the variable.
			bool resetLocalTime = s_resetThreadLocalTime.exchange(false);
			if (resetLocalTime)
			{
				stopAllNotes();
				localTime = 0u;
				loopStart = -1;
			}
			const f64 dt = TFE_System::updateThreadLocal(&localTime);
			const u32 transition = s_transition.load();

			const u32 trackCount = s_runtime.asset->trackCount;
			bool allTracksFinished = true;
			if (trackCount)
			{
				const u32 i = 0;
				const Track* track = &s_runtime.asset->tracks[i];
				MidiRuntimeTrack* runtimeTrack = &s_runtime.tracks[i];
				if ((u32)runtimeTrack->curTick >= track->length)
				{
					continue;
				}

				allTracksFinished = false;
				const f64 prevTick = runtimeTrack->curTick;
				f64 nextTick = prevTick + dt * 1000.0 / runtimeTrack->msPerTick;

				u32 start = (u32)prevTick;
				u32 end   = (u32)nextTick;

				const u32 evtCount = (u32)track->eventList.size();
				const MidiTrackEvent* evt = track->eventList.data();
				bool breakFromLoop = false;
				for (u32 e = u32(runtimeTrack->lastEvent + 1); e < evtCount && !breakFromLoop; e++)
				{
					if (evt[e].tick >= start && evt[e].tick <= end)
					{
						runtimeTrack->lastEvent = e;
						switch (evt[e].type)
						{
							case MTK_TEMPO:
								runtimeTrack->msPerTick = track->tempoEvents[evt[e].index].msPerTick;
								break;
							case MTK_MARKER:
							{
								const MidiMarker* marker = &track->markers[evt[e].index];
								TFE_System::logWrite(LOG_MSG, "iMuse", "Marker Track %d, \"%s\"", i, marker->name);
							} break;
							case MTK_MIDI:
							{
								const MidiEvent* midiEvt = &track->midiEvents[evt[e].index];
								const u8 type = midiEvt->channel >= 0 ? midiEvt->type + midiEvt->channel : midiEvt->type;
								// TODO: Track notes on and off so that hanging notes can be handled manually.
								//       Apparently not all midi devices support MID_ALL_NOTES_OFF.
								if ((midiEvt->type&0xf0) == MID_CONTROL_CHANGE && midiEvt->data[0] == MID_VOLUME_MSB)
								{
									const s32 channelIndex = midiEvt->type & 0x0f;
									s_channelSrcVolume[channelIndex] = midiEvt->data[1];
									TFE_MidiDevice::sendMessage(type, midiEvt->data[0], u8(s_channelSrcVolume[channelIndex] * s_masterVolumeScaled));
								}
								else
								{
									TFE_MidiDevice::sendMessage(type, midiEvt->data[0], midiEvt->data[1]);
								}
							} break;
							case MTK_IMUSE:
							{
								const iMuseEvent* imuse = &track->imuseEvents[evt[e].index];
								switch (imuse->cmd)
								{
									case IMUSE_START_NEW:
									{
									} break;
									case IMUSE_STALK_TRANS:
									{
									} break;
									case IMUSE_FIGHT_TRANS:
									{
									} break;
									case IMUSE_ENGAGE_TRANS:
									{
									} break;
									case IMUSE_FROM_FIGHT:
									{
									} break;
									case IMUSE_FROM_STALK:
									{
									} break;
									case IMUSE_FROM_BOSS:
									{
									} break;
									case IMUSE_CLEAR_CALLBACK:
									{
										//clearCallback();
									} break;
									case IMUSE_TO:
									{
										//setCallback();
									} break;
									case IMUSE_LOOP_START:
									{
									} break;
									case IMUSE_LOOP_END:
									{
										nextTick = imuse->arg[0].nArg;
										runtimeTrack->lastEvent = -1;
										breakFromLoop = true;
										stopAllNotes();
									} break;
								};
							} break;
						}
					}
					else if (evt[e].tick > end)
					{
						break;
					}
				}

				runtimeTrack->curTick = nextTick;
			}
			runThread = s_runMusicThread.load();
			// Give other threads a chance to run...
			//if (runThread) { TFE_System::sleep(0); }
		};
		
		return (TFE_THREADRET)0;
	}

	// Console Functions
	void setMusicVolumeConsole(const ConsoleArgList& args)
	{
		if (args.size() < 2) { return; }

		s_masterVolume = TFE_Console::getFloatArg(args[1]);
		s_masterVolumeScaled = s_masterVolume * c_musicVolumeScale;
		s_changeVolume.store(true);

		TFE_Settings_Sound* soundSettings = TFE_Settings::getSoundSettings();
		soundSettings->musicVolume = s_masterVolume;
		TFE_Settings::writeToDisk();
	}

	void getMusicVolumeConsole(const ConsoleArgList& args)
	{
		char res[256];
		sprintf(res, "Sound Volume: %2.3f", s_masterVolume);
		TFE_Console::addToHistory(res);
	}
}

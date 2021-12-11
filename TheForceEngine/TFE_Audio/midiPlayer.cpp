#include "midiPlayer.h"
#include "midiDevice.h"
#include "audioDevice.h"
#include <TFE_Asset/gmidAsset.h>
#include <TFE_System/system.h>
#include <TFE_System/Threads/thread.h>
#include <TFE_Settings/settings.h>
#include <TFE_FrontEndUI/console.h>
#include <algorithm>
#include <assert.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#undef min
#undef max
#endif

namespace TFE_MidiPlayer
{
	enum MidiPlayerCmd
	{
		MIDI_START,
		MIDI_STOP,
		MIDI_PAUSE,
		MIDI_RESUME,
		MIDI_CHANGE_VOL,
		MIDI_JUMP,
		MIDI_STOP_NOTES,
		MIDI_SET_IMUSE_CALLBACK,
		MIDI_COUNT
	};

	struct MidiCmd
	{
		MidiPlayerCmd cmd;
		union
		{
			const GMidiAsset* newAsset;
			iMuseCallback callback;
		};
		u32 measure;
		u32 beat;
		u32 tick;
		u32 newTrack;
		f32 newVolume;
		bool loop;
	};

	enum { MAX_MIDI_CMD = 256 };
	static MidiCmd s_midiCmdBuffer[MAX_MIDI_CMD];
	static u32 s_midiCmdCount = 0;
		
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

	// 1 ms
	static const f64 MIDI_FRAME = (1.0 / 1000.0);

	// Time scale should be exactly 1000 to convert from ms to sec.
	// However, for cutscenes to line up, we seem to have to play at less than realtime (87%) -
	// indicating a bug with iMuse or with the way the midi files are being parsed.
	// TODO: Listen and verify the tempo.
	static f64 s_timeScale = 1000.0;

	static const f32 c_musicVolumeScale = 0.5f;
	static MidiRuntime s_runtime;
	static f32 s_masterVolume = 1.0f;
	static f32 s_masterVolumeScaled = s_masterVolume * c_musicVolumeScale;
	static Thread* s_thread = nullptr;

	static atomic_bool s_runMusicThread;
	static s32 s_trackId = 0;

	static u8 s_channelSrcVolume[16] = { 0 };
	static Mutex s_mutex;
			
	TFE_THREADRET midiUpdateFunc(void* userData);
	void stopAllNotes();
	void changeVolume();

	// Console Functions
	void setMusicVolumeConsole(const ConsoleArgList& args);
	void getMusicVolumeConsole(const ConsoleArgList& args);

	static iMuseCallback s_iMuseCallback = nullptr;

	bool init()
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_MidiPlayer::init");

		bool res = TFE_MidiDevice::init();
		TFE_MidiDevice::selectDevice(0);
		s_runMusicThread.store(true);

		MUTEX_INITIALIZE(&s_mutex);

		s_thread = Thread::create("MidiThread", midiUpdateFunc, nullptr);
		if (s_thread)
		{
			s_thread->run();
		}

		CCMD("setMusicVolume", setMusicVolumeConsole, 1, "Sets the music volume, range is 0.0 to 1.0");
		CCMD("getMusicVolume", getMusicVolumeConsole, 0, "Get the current music volume where 0 = silent, 1 = maximum.");

		TFE_Settings_Sound* soundSettings = TFE_Settings::getSoundSettings();
		setVolume(soundSettings->musicVolume);
		s_timeScale = 1000.0;

		s_iMuseCallback = nullptr;

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

		MUTEX_DESTROY(&s_mutex);
	}
	
	void midiSetTimeScale(f64 scale)
	{
		s_timeScale = 1000.0 * scale;
	}

	//////////////////////////////////////////////////
	// Command Buffer
	//////////////////////////////////////////////////
	MidiCmd* midiAllocCmd()
	{
		if (s_midiCmdCount >= MAX_MIDI_CMD) { return nullptr; }
		MidiCmd* cmd = &s_midiCmdBuffer[s_midiCmdCount];
		s_midiCmdCount++;
		return cmd;
	}

	void midiClearCmdBuffer()
	{
		s_midiCmdCount = 0;
	}
	
	//////////////////////////////////////////////////
	// Command Interface
	//////////////////////////////////////////////////
	void playSong(const GMidiAsset* gmidAsset, bool loop, s32 track)
	{
		if (!gmidAsset) { return; }

		MUTEX_LOCK(&s_mutex);
		MidiCmd* midiCmd = midiAllocCmd();
		if (midiCmd)
		{
			midiCmd->cmd = MIDI_START;
			midiCmd->newAsset  = gmidAsset;
			midiCmd->newTrack  = track;
			midiCmd->newVolume = s_masterVolume;
			midiCmd->loop = loop;
		}
		MUTEX_UNLOCK(&s_mutex);

		if (s_thread->isPaused())
		{
			s_thread->resume();
		}
	}
	
	void setVolume(f32 volume)
	{
		MUTEX_LOCK(&s_mutex);
		MidiCmd* midiCmd = midiAllocCmd();
		if (midiCmd)
		{
			midiCmd->cmd = MIDI_CHANGE_VOL;
			midiCmd->newVolume = volume;
		}
		MUTEX_UNLOCK(&s_mutex);
	}
	
	void pause()
	{
		MUTEX_LOCK(&s_mutex);
		MidiCmd* midiCmd = midiAllocCmd();
		if (midiCmd)
		{
			midiCmd->cmd = MIDI_PAUSE;
		}
		MUTEX_UNLOCK(&s_mutex);
	}

	void resume()
	{
		MUTEX_LOCK(&s_mutex);
		MidiCmd* midiCmd = midiAllocCmd();
		if (midiCmd)
		{
			midiCmd->cmd = MIDI_RESUME;
		}
		MUTEX_UNLOCK(&s_mutex);
	}
	
	void stop()
	{
		MUTEX_LOCK(&s_mutex);
		MidiCmd* midiCmd = midiAllocCmd();
		if (midiCmd)
		{
			midiCmd->cmd = MIDI_STOP;
		}
		MUTEX_UNLOCK(&s_mutex);
	}

	void midiJump(s32 track, s32 measure, s32 beat, s32 tick)
	{
		MUTEX_LOCK(&s_mutex);
		MidiCmd* midiCmd = midiAllocCmd();
		if (midiCmd)
		{
			midiCmd->cmd = MIDI_JUMP;
			// The code calling this is '1' based for tracks, measure, and beat.
			assert(track >= 1 && measure >= 1 && beat >= 1);
			midiCmd->newTrack = track - 1;
			midiCmd->measure = measure - 1;
			midiCmd->beat = beat - 1;
			midiCmd->tick = tick;
		}
		MUTEX_UNLOCK(&s_mutex);
	}

	void midiSet_iMuseCallback(iMuseCallback callback)
	{
		MUTEX_LOCK(&s_mutex);
		MidiCmd* midiCmd = midiAllocCmd();
		if (midiCmd)
		{
			midiCmd->cmd = MIDI_SET_IMUSE_CALLBACK;
			midiCmd->callback = callback;
		}
		MUTEX_UNLOCK(&s_mutex);
	}

	iMuseCallback midiGet_iMuseCallback()
	{
		return s_iMuseCallback;
	}

	f32 getVolume()
	{
		return s_masterVolume;
	}

	u32 getMeasure()
	{
		const s32 trackId = s_trackId;
		const GMidiAsset* asset = s_runtime.asset;
		if (!asset || trackId < 0 || trackId >= (s32)asset->trackCount)
		{
			return 1;
		}

		const u64 curTick = u64(s_runtime.tracks[trackId].curTick);
		return 1 + u32(curTick / (u64)s_runtime.asset->tracks[0].ticksPerMeasure);
	}

	u32 getBeat()
	{
		const s32 trackId = s_trackId;
		const GMidiAsset* asset = s_runtime.asset;
		if (!asset || trackId < 0 || trackId >= (s32)asset->trackCount)
		{
			return 1;
		}

		const u64 curTick = u64(s_runtime.tracks[s_trackId].curTick);
		return 1 + u32((curTick / (u64)s_runtime.asset->tracks[0].ticksPerBeat) & 3);
	}

	u64 getTick()
	{
		const s32 trackId = s_trackId;
		const GMidiAsset* asset = s_runtime.asset;
		if (!asset || trackId < 0 || trackId >= (s32)asset->trackCount)
		{
			return 0;
		}

		return u64(s_runtime.tracks[s_trackId].curTick);
	}

	//////////////////////////////////////////////////
	// Internal
	//////////////////////////////////////////////////
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
	
	void resetLocalTime(s32& loopStart, u64& localTime, f64& dt)
	{
		stopAllNotes();
		localTime = 0u;
		loopStart = -1;
		dt = 0.0;
	}
		
	// Thread Function
	TFE_THREADRET midiUpdateFunc(void* userData)
	{
		bool runThread  = true;
		bool wasPlaying = false;
		bool isPlaying  = false;
		bool isPaused = false;
		s32 loopStart = -1;
		u64 localTime = 0;
		f64 dt = 0.0;
		while (runThread)
		{
			// Read from the command buffer.
			MUTEX_LOCK(&s_mutex);
			MidiCmd* midiCmd = s_midiCmdBuffer;
			for (u32 i = 0; i < s_midiCmdCount; i++, midiCmd++)
			{
				switch (midiCmd->cmd)
				{
					case MIDI_START:
					{
						const GMidiAsset* asset = midiCmd->newAsset;
						const u32 trackCount = asset->trackCount;
						s_runtime.asset = asset;
						s_runtime.loop  = midiCmd->loop;
						for (u32 i = 0; i < trackCount; i++)
						{
							s_runtime.tracks[i].curTick = 0;
							s_runtime.tracks[i].lastEvent = -1;
							s_runtime.tracks[i].msPerTick = asset->tracks[i].msPerTick;
						}

						for (u32 i = 0; i < 16; i++)
						{
							s_channelSrcVolume[i] = CHANNEL_MAX_VOLUME;
						}
						changeVolume();

						isPlaying = true;
						s_trackId = midiCmd->newTrack;
						resetLocalTime(loopStart, localTime, dt);
					} break;
					case MIDI_STOP:
					{
						isPlaying = false;
						isPaused  = false;
						stopAllNotes();
					} break;
					case MIDI_PAUSE:
					{
						isPaused = true;
						stopAllNotes();
					} break;
					case MIDI_RESUME:
					{
						isPaused = false;
					} break;
					case MIDI_CHANGE_VOL:
					{
						s_masterVolume = midiCmd->newVolume;
						s_masterVolumeScaled = s_masterVolume * c_musicVolumeScale;
						changeVolume();
					} break;
					case MIDI_JUMP:
					{
						if (midiCmd->newTrack >= 0 && midiCmd->newTrack < s_runtime.asset->trackCount)
						{
							const u64 tick = midiCmd->tick + midiCmd->measure * s_runtime.asset->tracks[0].ticksPerMeasure + midiCmd->beat * s_runtime.asset->tracks[0].ticksPerBeat;
							s_trackId = midiCmd->newTrack;
							s_runtime.tracks[s_trackId].curTick = (f64)tick;
							resetLocalTime(loopStart, localTime, dt);
						}
					} break;
					case MIDI_STOP_NOTES:
					{
						stopAllNotes();
					} break;
					case MIDI_SET_IMUSE_CALLBACK:
					{
						s_iMuseCallback = midiCmd->callback;
					} break;
				}
			}
			s_midiCmdCount = 0;
			MUTEX_UNLOCK(&s_mutex);

			if (!isPlaying)
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

			if (isPaused)
			{
				TFE_System::updateThreadLocal(&localTime);
				if (runThread) { TFE_System::sleep(1); }
				continue;
			}

			const GMidiAsset* asset = s_runtime.asset;
			const u32 trackCount = s_runtime.asset->trackCount;
			const u32 trackId = s_trackId;
			dt += TFE_System::updateThreadLocal(&localTime);

			bool allTracksFinished = true;
			if (trackCount && dt >= MIDI_FRAME)
			{
				const Track* track = &asset->tracks[trackId];
				MidiRuntimeTrack* runtimeTrack = &s_runtime.tracks[trackId];
				if ((u32)runtimeTrack->curTick >= track->length)
				{
					continue;
				}

				allTracksFinished = false;
				const f64 prevTick = runtimeTrack->curTick;
				f64 nextTick = prevTick + dt * s_timeScale / runtimeTrack->msPerTick;
				dt = 0.0;

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
							{
								runtimeTrack->msPerTick = track->tempoEvents[evt[e].index].msPerTick;
							} break;
							case MTK_MARKER:
							{
								const MidiMarker* marker = &track->markers[evt[e].index];
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
								if (imuse->cmd == IMUSE_LOOP_START)
								{
									loopStart = evt[e].tick;
								}
								else if (imuse->cmd == IMUSE_LOOP_END)
								{
									if (s_runtime.loop)  // TODO: Is this accurate?
									{
										// If the loop start isn't set, use the pre-recorded value.
										nextTick = loopStart >= 0 ? loopStart : imuse->arg[0].nArg;
										runtimeTrack->lastEvent = -1;
										breakFromLoop = true;
										stopAllNotes();
									}
								}
								else if (s_iMuseCallback)
								{
									iMuseCallback callback = s_iMuseCallback;
									// Clear the callback
									s_iMuseCallback = nullptr;
									// Call the original callback.
									callback(imuse);
								}
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
			// if (runThread) { TFE_System::sleep(0); }
		};
		
		return (TFE_THREADRET)0;
	}

	// Console Functions
	void setMusicVolumeConsole(const ConsoleArgList& args)
	{
		if (args.size() < 2) { return; }
		setVolume(TFE_Console::getFloatArg(args[1]));

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

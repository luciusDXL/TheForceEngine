#include "midiPlayer.h"
#include "midiDevice.h"
#include <DXL2_Asset/gmidAsset.h>
#include <DXL2_System/system.h>
#include <DXL2_System/Threads/thread.h>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#undef min
#undef max
#endif

namespace OS
{
#ifdef _WIN32
	void sleep(u32 sleepDeltaMS)
	{
		Sleep(sleepDeltaMS);
	}
#endif
}

namespace DXL2_MidiPlayer
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
		MidiRuntimeTrack tracks[16];
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

	static MidiRuntime s_runtime;
	static atomic_bool s_isPlaying;
	static atomic_u32  s_transition;
	static f32  s_masterVolume = 0.5f;
	static Thread* s_thread = nullptr;

	static atomic_bool s_runMusicThread;
	static atomic_bool s_resetThreadLocalTime;

	DXL2_THREADRET midiUpdateFunc(void* userData);

	bool init()
	{
		bool res = DXL2_MidiDevice::init();
		DXL2_MidiDevice::selectDevice(0);
		s_runMusicThread.store(true);
		s_isPlaying.store(false);
		s_transition.store(TRANSITION_NONE);
		s_resetThreadLocalTime.store(true);

		s_thread = Thread::create("MidiThread", midiUpdateFunc, nullptr);
		if (s_thread)
		{
			s_thread->run();
			s_thread->pause();
		}

		return res && s_thread;
	}

	void destroy()
	{
		// Destroy the thread before shutting down the Midi Device.
		s_runMusicThread.store(false);
		delete s_thread;

		DXL2_MidiDevice::destroy();
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
	}

	void pause()
	{
		if (!s_thread->isPaused())
		{
			s_thread->pause();
		}
	}

	void resume()
	{
		if (s_thread->isPaused())
		{
			s_thread->resume();
		}
	}
	
	void stop()
	{
		s_isPlaying.store(false);
	}

	// Thread Function
	DXL2_THREADRET midiUpdateFunc(void* userData)
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
					for (u32 i = 0; i < 16; i++)
					{
						DXL2_MidiDevice::sendMessage(MID_CONTROL_CHANGE + i, MID_ALL_NOTES_OFF);
					}
					wasPlaying = false;
					loopStart = -1;
				}
				runThread = s_runMusicThread.load();
				if (runThread) { OS::sleep(16); }
				continue;
			}
			wasPlaying = true;

			bool resetLocalTime = s_resetThreadLocalTime.load();
			s_resetThreadLocalTime.store(false);
			if (resetLocalTime)
			{
				localTime = 0u;
				loopStart = -1;
			}
			const f64 dt = DXL2_System::updateThreadLocal(&localTime);
			const u32 transition = s_transition.load();

			const u32 trackCount = s_runtime.asset->trackCount;
			bool allTracksFinished = true;
			u32 i = 0;
			{
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
				for (u32 e = u32(runtimeTrack->lastEvent + 1); e < evtCount; e++)
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
								DXL2_System::logWrite(LOG_MSG, "iMuse", "Marker Track %d, \"%s\"", i, marker->name);
							} break;
							case MTK_MIDI:
							{
								const MidiEvent* midiEvt = &track->midiEvents[evt[e].index];
								const u8 type = midiEvt->channel >= 0 ? midiEvt->type + midiEvt->channel : midiEvt->type;
								// TODO: Track notes on and off so that hanging notes can be handled manually.
								//       Apparently not all midi devices support MID_ALL_NOTES_OFF.
								if (midiEvt->type == MID_CONTROL_CHANGE && midiEvt->data[0] == MID_VOLUME_MSB)
								{
									DXL2_MidiDevice::sendMessage(type, midiEvt->data[0], u8(midiEvt->data[1] * s_masterVolume));
								}
								else
								{
									DXL2_MidiDevice::sendMessage(type, midiEvt->data[0], midiEvt->data[1]);
								}
							} break;
							case MTK_IMUSE:
							{
								const MidiSysExImuse* imuse = &track->imuseEvents[evt[e].index];
								if (imuse->id == 3)
								{
									//DXL2_System::logWrite(LOG_MSG, "iMuse", "Track %d, Msg \"%s\", Tick %u", i, imuse->msg, (u32)runtimeTrack->curTick);
									if (transition != TRANSITION_NONE)
									{
										// is this a transition point?
									}
								}
								else
								{
									//DXL2_System::logWrite(LOG_MSG, "iMuse", "Track %d, ID %d, Tick %u", i, imuse->id, (u32)runtimeTrack->curTick);
									if (end >= track->loopEnd && track->loopEnd > 0 && s_runtime.loop)
									{
										nextTick = track->loopStart;
										runtimeTrack->lastEvent = -1;
										for (u32 i = 0; i < 16; i++)
										{
											DXL2_MidiDevice::sendMessage(MID_CONTROL_CHANGE + i, MID_ALL_NOTES_OFF);
										}
										break;
									}
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
			//if (runThread) { OS::sleep(0); }
		};
		
		return (DXL2_THREADRET)0;
	}
}

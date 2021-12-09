#include "gameMusic.h"
#include "util.h"
#include "random.h"
#include <TFE_Asset/gmidAsset.h>
#include <TFE_Audio/midiPlayer.h>
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_System/parser.h>

namespace TFE_DarkForces
{
	enum MusicConstants
	{
		MUS_LEVEL_COUNT = 14,
		MUS_STATE_COUNT = 5,
		MUS_FIGHT_TIME  = TICKS(3),
	};

	static const char s_levelMusic[MUS_LEVEL_COUNT][MUS_STATE_COUNT][10] =
	{
		// LEVEL 1
		{
			"",
			"fight-01",
			"",
			"stalk-01",
			"",
		},
		// LEVEL 2
		{
			"",
			"fight-02",
			"",
			"stalk-02",
			"",
		},
		// LEVEL 3
		{
			"",
			"fight-03",
			"",
			"stalk-03",
			"",
		},
		// LEVEL 4
		{
			"",
			"fight-04",
			"",
			"stalk-04",
			"",
		},
		// LEVEL 5
		{
			"boss-05",
			"fight-05",
			"",
			"stalk-05",
			"",
		},
		// LEVEL 6
		{
			"",
			"fight-06",
			"",
			"stalk-06",
			"",
		},
		// LEVEL 7
		{
			"",
			"fight-07",
			"",
			"stalk-07",
			"",
		},
		// LEVEL 8
		{
			"boss-08",
			"fight-08",
			"",
			"stalk-08",
			"",
		},
		// LEVEL 9
		{
			"",
			"fight-09",
			"",
			"stalk-09",
			"",
		},
		// LEVEL 10
		{
			"boss-10",
			"fight-10",
			"",
			"stalk-10",
			"",
		},
		// LEVEL 11
		{
			"boss-11",
			"fight-11",
			"",
			"stalk-11",
			"",
		},
		// LEVEL 12
		{
			"",
			"fight-12",
			"",
			"stalk-12",
			"",
		},
		// LEVEL 13
		{
			"",
			"fight-13",
			"",
			"stalk-13",
			"",
		},
		// LEVEL 14
		{
			"boss-14",
			"fight-14",
			"",
			"stalk-14",
			"",
		},
	};

	MusicState s_currentState = MUS_STATE_NULLSTATE;
	MusicState s_oldState = MUS_STATE_NULLSTATE;
	static s32 s_stateEntrances[MUS_STATE_UNDEFINED] = { 0 };
	static s32 s_currentLevel = 0;
	static GMidiAsset* s_oldSong = nullptr;
	static GMidiAsset* s_newSong = nullptr;
	static s32 s_transChunk = 0;
	static u32 s_savedMeasure = 0;
	static u32 s_savedBeat = 0;
	static Task* s_musicTask = nullptr;
	static JBool s_desiredFightState = JFALSE;
	
	s32  gameMusic_setLevel(s32 level);
	void loadMusic(const char* fileName);
	GMidiAsset* getMusic(const char* fileName);
	s32 gameMusic_random(s32 low, s32 high);
	void gameMusic_taskFunc(MessageType msg);
	void iMuseCallback1(const iMuseEvent* evt);
	void iMuseCallback2(const iMuseEvent* evt);
	void iMuseCallback3(const iMuseEvent* evt);

	void gameMusic_start(s32 level)
	{
		memset(s_stateEntrances, 0, MUS_STATE_UNDEFINED * sizeof(s32));
		s_musicTask = createSubTask("iMuse", gameMusic_taskFunc);

		gameMusic_setLevel(level);
		gameMusic_setState(MUS_STATE_STALK);
		s_desiredFightState = JFALSE;
	}

	void gameMusic_stop()
	{
		gameMusic_setState(MUS_STATE_NULLSTATE);
		s_musicTask = nullptr;
	}

	s32 gameMusic_setLevel(s32 level)
	{
		s_oldSong = nullptr;

		if (level <= MUS_LEVEL_COUNT)
		{
			s32 oldLevel = s_currentLevel;
			s_currentLevel = level;

			if (s_currentLevel != oldLevel)
			{
				TFE_MidiPlayer::stop();
				s_currentState = MUS_STATE_NULLSTATE;
				if (s_currentLevel)
				{
					for (s32 i = 1; i < MUS_STATE_UNDEFINED; i++)
					{
						if (s_levelMusic[s_currentLevel - 1][i - 1][0])
						{
							loadMusic(s_levelMusic[s_currentLevel-1][i-1]);
						}
					}
				}
			}
			return s_currentLevel;
		}
		return 0;
	}
		
	void gameMusic_setState(MusicState state)
	{
		if (s_currentLevel && state < MUS_STATE_UNDEFINED && state != s_currentState)
		{
			if (state == MUS_STATE_NULLSTATE)
			{
				TFE_MidiPlayer::stop();
				s_currentState = state;
			}
			else
			{
				const char* newSongName = s_levelMusic[s_currentLevel-1][state-1];
				const char* oldSongName = s_levelMusic[s_currentLevel-1][s_currentState-1];
				GMidiAsset* newSong = getMusic(newSongName);
				if (newSong)
				{
					if (s_currentState != MUS_STATE_NULLSTATE)
					{
						if (!TFE_MidiPlayer::midiGet_iMuseCallback())
						{
							s_oldSong = getMusic(oldSongName);
							TFE_MidiPlayer::midiSet_iMuseCallback(iMuseCallback1);
						}
					}
					else
					{
						TFE_MidiPlayer::playSong(newSong, true);
					}

					s_oldState = s_currentState;
					s_currentState = state;
				}
				else
				{
					TFE_System::logWrite(LOG_ERROR, "GameMusic", "Cannot start song '%s'.", newSongName);
				}
			}
		}
		else if (state == MUS_STATE_NULLSTATE)
		{
			TFE_MidiPlayer::stop();
			s_currentState = state;
		}
	}

	MusicState gameMusic_getState()
	{
		return s_currentState;
	}
				
	void gameMusic_startFight()
	{
		static s32 scriptTimer = 0;
		if (s_curTick - scriptTimer > TICKS(8))
		{
			scriptTimer = s_curTick - TICKS(6);
		}
		else
		{
			scriptTimer += TICKS(3);
		}

		if (scriptTimer > s_curTick)
		{
			// TFE_System::logWrite(LOG_MSG, "iMuse", "Trigger Fight");
			task_setNextTick(s_musicTask, s_curTick + MUS_FIGHT_TIME);
			if (!s_desiredFightState)
			{
				gameMusic_setState(MUS_STATE_FIGHT);
				s_desiredFightState = JTRUE;
			}
		}
	}

	void gameMusic_sustainFight()
	{
		if (s_desiredFightState)
		{
			// TFE_System::logWrite(LOG_MSG, "iMuse", "Sustain Fight");
			gameMusic_startFight();
		}
	}

	void gameMusic_taskFunc(MessageType msg)
	{
		task_begin;
		while (msg != MSG_FREE_TASK)
		{
			task_yield(TASK_SLEEP);

			// TFE_System::logWrite(LOG_MSG, "iMuse", "Trigger Stalk");
			gameMusic_setState(MUS_STATE_STALK);
			s_desiredFightState = JFALSE;
		}
		task_end;
	}

	void loadMusic(const char* fileName)
	{
		char filePath[32];
		sprintf(filePath, "%s.GMD", fileName);
		// Pre-load
		TFE_GmidAsset::get(filePath);
	}

	GMidiAsset* getMusic(const char* fileName)
	{
		char filePath[32];
		sprintf(filePath, "%s.GMD", fileName);
		// This should already be cached.
		return TFE_GmidAsset::get(filePath);
	}
		
	void iMuseCallback1(const iMuseEvent* evt)
	{
		if (evt->cmd == IMUSE_TO)
		{
			s32 punt = 0;
			if (evt->arg[1].nArg)	// slow
			{
				if (s_currentState <= s_oldState || s_currentState == MUS_STATE_FIGHT)
				{
					punt = 1;
				}
			}
			else if ((s_currentState > s_oldState && s_currentState != MUS_STATE_FIGHT) || (s_currentState == s_oldState))
			{
				punt = 1;
			}

			if (!punt)
			{
				s_savedMeasure = TFE_MidiPlayer::getMeasure();
				s_savedBeat = TFE_MidiPlayer::getBeat();

				s_transChunk = evt->arg[0].nArg;
				TFE_MidiPlayer::midiJump(1+s_transChunk, 1, 1);
				TFE_MidiPlayer::midiSet_iMuseCallback(iMuseCallback2);
			}
			else
			{
				TFE_MidiPlayer::midiSet_iMuseCallback(iMuseCallback1);
			}
		}
		else
		{
			TFE_System::logWrite(LOG_WARNING, "GameMusic", "iMuseCallback1 got an invalid marker...");
		}
	}

	s32 iMuseEventArgCount(const iMuseEvent* evt)
	{
		s32 count = 0;
		for (s32 i = 0; i < 3; i++)
		{
			if (evt->arg[i].fArg == 0.0f)
			{
				break;
			}
			count++;
		}
		return count;
	}

	s32 parseEvent(const iMuseEvent* evt)
	{
		if (evt->cmd == IMUSE_FROM_FIGHT && s_oldState == MUS_STATE_FIGHT)
		{
			return iMuseEventArgCount(evt);
		}
		else if (evt->cmd == IMUSE_FROM_STALK && s_oldState == MUS_STATE_STALK)
		{
			return iMuseEventArgCount(evt);
		}
		else if (evt->cmd == IMUSE_FROM_BOSS && s_oldState == MUS_STATE_BOSS)
		{
			return iMuseEventArgCount(evt);
		}
		else if (evt->cmd == IMUSE_FIGHT_TRANS && s_currentState == MUS_STATE_FIGHT)
		{
			return iMuseEventArgCount(evt);
		}
		else if (evt->cmd == IMUSE_STALK_TRANS && s_currentState == MUS_STATE_STALK)
		{
			return iMuseEventArgCount(evt);
		}
	#if 0
		else if (evt->cmd == IMUSE_BOSS_TRANS && s_currentState == MUS_STATE_BOSS)
		{
			return iMuseEventArgCount(evt);
		}
	#endif

		return 0;
	}

	void iMuseCallback2(const iMuseEvent* evt)
	{
		s32 count = parseEvent(evt);
		if (count)
		{
			s32 r = gameMusic_random(0, count - 1);
			TFE_MidiPlayer::midiJump(1+s_transChunk, (s32)evt->arg[r].fArg - 1, 4, 300);
			TFE_MidiPlayer::midiSet_iMuseCallback(iMuseCallback2);
		}
		else if (evt->cmd == IMUSE_START_NEW)
		{
			s_newSong = getMusic(s_levelMusic[s_currentLevel-1][s_currentState-1]);
			if (s_oldSong != s_newSong)
			{
				TFE_MidiPlayer::playSong(s_newSong, true);
				TFE_MidiPlayer::midiSet_iMuseCallback(iMuseCallback3);
			}
			else
			{
				TFE_MidiPlayer::midiJump(1, s_savedMeasure, s_savedBeat, 300);
			}
		}
		else
		{
			TFE_MidiPlayer::midiSet_iMuseCallback(iMuseCallback2);
		}
	}

	void iMuseCallback3(const iMuseEvent* evt)
	{
		s32 count = parseEvent(evt);
		if (count)
		{
			s32 r = gameMusic_random(0, count - 1);

			s32 value = s32(evt->arg[r].fArg);
			if (value == s_stateEntrances[s_currentState])
			{
				r = gameMusic_random(0, count - 1);
				value = s32(evt->arg[r].fArg);
			}
			if (value == s_stateEntrances[s_currentState])
			{
				r = gameMusic_random(0, count - 1);
				value = s32(evt->arg[r].fArg);
			}
			s_stateEntrances[s_currentState] = value;

			TFE_MidiPlayer::midiJump(1, value - 1, 4, 400);
		}
		else if (evt->cmd != IMUSE_CLEAR_CALLBACK)
		{
			TFE_MidiPlayer::midiSet_iMuseCallback(iMuseCallback3);
		}
	}

	s32 gameMusic_random(s32 low, s32 high)
	{
		static s32 rseed1, rseed2, initFlag = 0;
		if (!initFlag)
		{
			initFlag = 1;
			rseed1 =  (s32)(size_t)(&rseed2);
			rseed2 = ~(s32)(size_t)(&rseed1);
		}

		for (s32 i = 0; i < 23; i++)
		{
			s32 c = ((rseed1 & 0x40000000) != 0) ^ ((rseed2 & 0x20000000) == 0);
			rseed1 <<= 1;
			rseed1 += c;
		}
		for (s32 i = 0; i < 37; i++)
		{
			s32 c = ((rseed2 & 0x40000000) != 0) ^ ((rseed1 & 0x20000000) == 0);
			rseed1 <<= 1;
			rseed1 += c;
		}

		s32 range = high - low;
		return min(high, ((u16(rseed1 + rseed2) * s32(range + 1)) >> 16) + low);
	}
}  // TFE_DarkForces
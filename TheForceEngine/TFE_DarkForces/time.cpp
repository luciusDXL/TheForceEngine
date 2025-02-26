#include "time.h"
#include <TFE_System/system.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <cstring>
#include <TFE_Input/replay.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	Tick s_curTick = 0;
	Tick s_prevTick = 0;
	f64 s_timeAccum = 0.0;
	fixed16_16 s_deltaTime;
	fixed16_16 s_frameTicks[13] = { 0 };
	JBool s_pauseTimeUpdate = JFALSE;

	void time_serialize(Stream* stream)
	{
		SERIALIZE(SaveVersionInit, s_curTick, 0);
		SERIALIZE(SaveVersionInit, s_prevTick, 0);
		SERIALIZE(SaveVersionInit, s_timeAccum, 0.0);
		SERIALIZE(SaveVersionInit, s_deltaTime, 0);
		SERIALIZE_BUF(SaveVersionInit, s_frameTicks, sizeof(fixed16_16) * TFE_ARRAYSIZE(s_frameTicks));
	}

	Tick time_frameRateToDelay(u32 frameRate)
	{
		return Tick(SECONDS_TO_TICKS_ROUNDED / f32(frameRate));
	}

	Tick time_frameRateToDelay(s32 frameRate)
	{
		return Tick(SECONDS_TO_TICKS_ROUNDED / f32(frameRate));
	}

	Tick time_frameRateToDelay(f32 frameRate)
	{
		return Tick(SECONDS_TO_TICKS_ROUNDED / frameRate);
	}

	void time_pause(JBool pause)
	{
		s_pauseTimeUpdate = pause;
	}

	void updateTime()
	{		
		if (!s_pauseTimeUpdate)
		{
			s_timeAccum += TFE_System::getDeltaTime() * TIMER_FREQ;
		}

		Tick prevTick = s_curTick;

		// Record Replay Ticks 
		if (TFE_Input::isRecording())
		{
			TFE_Input::saveTick();
		}
		
		// Either Playback Ticks or use the current time.
		if (TFE_Input::isDemoPlayback())
		{			
			TFE_Input::loadTick();
		}
		else
		{
			s_curTick = Tick(s_timeAccum);
		}
		fixed16_16 dt = div16(intToFixed16(s_curTick - prevTick), FIXED(TICKS_PER_SECOND));
		for (s32 i = 0; i < 13; i++)
		{
			s_frameTicks[i] += mul16(dt, intToFixed16(i));
		}
	}
}  // TFE_DarkForces
#include "time.h"
#include <TFE_System/system.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <cstring>
#include <TFE_Input/replay.h>
#include <TFE_Input/inputMapping.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	Tick s_curTick = 0;
	Tick s_prevTick = 0;
	static f64 s_timeAccum = 0.0;
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

	void resetTime()
	{
		s_curTick = 0;
		s_prevTick = 0;
		s_timeAccum = 0.0;
		s_deltaTime;
		s_frameTicks[13] = { 0 };
		s_pauseTimeUpdate = JFALSE;
	}

	void setTimeAccum(f64 timeAccum)
	{
		s_timeAccum = timeAccum;
	}

	f64 getTimeAccum()
	{
		return s_timeAccum;
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
			//TFE_System::logWrite(LOG_MSG, "Progam Flow", "TIME ACCUM %u %d", s_timeAccum, s_timeAccum);

		}

		Tick prevTick = s_curTick;					
		if (TFE_Input::isRecording())
		{
			TFE_Input::saveTick();
			/*if (TFE_Input::getCounter() == 0)
			{
				resetTime();
			}*/
           //  std::tuple < Tick, Tick, f64, fixed16_16> tData = { s_curTick, prevTick, s_timeAccum, s_deltaTime};
			//TFE_System::logWrite(LOG_MSG, "Progam Flow", "TIME curtick = %d prevtick = %d accum = %d delta = %d", s_curTick, s_prevTick, s_timeAccum, s_deltaTime);
			//	TFE_System::logWrite(LOG_MSG, "Progam Flow", "SAVETICK %u %d", s_curTick, s_curTick);
			//TFE_Input::saveTick(tData);
		}
		
		if (TFE_Input::isDemoPlayback())
		{			
			TFE_Input::loadTick();
			
			//std::tuple < Tick, Tick, f64, fixed16_16> tData = TFE_Input::loadTick();
				//s_curTick = std::get<0>(tData);
				/*prevTick = std::get<1>(tData);
				s_timeAccum = std::get<2>(tData);
				s_deltaTime = std::get<3>(tData);*/
			
			//TFE_System::logWrite(LOG_MSG, "Progam Flow", "LOADTICK %u %d", s_curTick, s_curTick);
			//TFE_System::logWrite(LOG_MSG, "Progam Flow", "REPLAYTICK %u %d", replayTick, replayTick);
		}
		else
		{
			s_curTick = Tick(s_timeAccum);
		}
		TFE_System::logWrite(LOG_MSG, "TIME", "Update Tick s_curTick = %d prevTick = %d  s_=revTick = %d updateCounter = %d", s_curTick, prevTick, s_prevTick, TFE_Input::getCounter());
		fixed16_16 dt = div16(intToFixed16(s_curTick - prevTick), FIXED(TICKS_PER_SECOND));
		//TFE_System::logWrite(LOG_MSG, "Progam Flow", "DT %u %d", dt, dt);
		for (s32 i = 0; i < 13; i++)
		{
			s_frameTicks[i] += mul16(dt, intToFixed16(i));
		}
	}
}  // TFE_DarkForces
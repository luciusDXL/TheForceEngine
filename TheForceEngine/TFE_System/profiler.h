#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Profiler
// Simple "zone" based profiler.
// Add TFE_PROFILE_ENABLED to preprocessor defines in the build to enable.
// Currently does not respect the call path, that is TODO.
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "system.h"

#define TFE_PROFILE_ENABLED 1

#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#ifdef  TFE_PROFILE_ENABLED
#define TFE_ZONE(name)  TFE_Profiler_Zone TOKENPASTE2(__localZone, __LINE__)(name, __FUNCTION__, __LINE__)
#define TFE_ZONE_BEGIN(varName, name)  TFE_Profiler_ZoneManual varName(name, __FUNCTION__, __LINE__)
#define TFE_ZONE_END(varName)  varName.end()
#define TFE_FRAME_BEGIN() TFE_Profiler::frameBegin()
#define TFE_FRAME_END() TFE_Profiler::frameEnd()
#define TFE_COUNTER(varName, name) TFE_Profiler::addCounter(name, &varName)
#else
#define TFE_ZONE(name)
#define TFE_ZONE_BEGIN(varName, name)
#define TFE_ZONE_END(varName)
#define TFE_FRAME_BEGIN()
#define TFE_FRAME_END()
#define TFE_COUNTER(varName, name)
#endif

#ifdef TFE_PROFILE_ENABLED
struct TFE_ZoneInfo
{
	char* name;
	char* func;
	u32  lineNumber;
	u32  level;
	f64  timeInZone;
	f64  timeInZoneAve;
};

struct TFE_CounterInfo
{
	char* name;
	s32   value;
};

namespace TFE_Profiler
{
	// The main profiling API is used through Macros which can be disabled based on build flags.
	u32  beginZone(const char* name, const char* func, u32 lineNumber);
	void endZone(u32 id, u64 dt);
		
	void frameBegin();
	void frameEnd();

	void addCounter(const char* name, s32* counter);

	// Profile data API, this is used directly.
	f64  getTimeInFrame();

	u32  getZoneCount();
	void getZoneInfo(u32 index, TFE_ZoneInfo* info);
	
	u32  getCounterCount();
	void getCounterInfo(u32 index, TFE_CounterInfo* info);
}

class TFE_Profiler_Zone
{
public:
	TFE_Profiler_Zone(const char* name, const char* func, u32 lineNumber)
	{
		m_time = TFE_System::getCurrentTimeInTicks();
		m_id = TFE_Profiler::beginZone(name, func, lineNumber);
	}

	~TFE_Profiler_Zone()
	{
		const u64 deltaTime = TFE_System::getCurrentTimeInTicks() - m_time;
		TFE_Profiler::endZone(m_id, deltaTime);
	}
private:
	u64 m_time;
	s32 m_id;
};

class TFE_Profiler_ZoneManual
{
public:
	TFE_Profiler_ZoneManual(const char* name, const char* func, u32 lineNumber)
	{
		m_time = TFE_System::getCurrentTimeInTicks();
		m_id = TFE_Profiler::beginZone(name, func, lineNumber);
	}

	void end()
	{
		const u64 deltaTime = TFE_System::getCurrentTimeInTicks() - m_time;
		TFE_Profiler::endZone(m_id, deltaTime);
	}
private:
	u64 m_time;
	s32 m_id;
};
#endif

#include <cstring>

#include "profiler.h"
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

// TODO: Support call "paths" - with seperate time per path.

namespace TFE_Profiler
{
	#define ZONE_BUFFER_COUNT 2
	#define MAX_ZONE_STACK 256
	
	struct Zone
	{
		u32  id;
		u32  level = 0;
		u32  parent = NULL_ZONE;
		u64  path;
		u64  frame;
		char name[64];
		char func[64];
		u32  lineNumber;

		f64  timeInZone[ZONE_BUFFER_COUNT];
		f64  timeInZoneAve;
		f64  fractOfParentAve;

		u32  child = NULL_ZONE;
		u32  sibling = NULL_ZONE;
	};

	struct Counter
	{
		u32  id;
		s32* ptr;
		s32  prevValue;

		char name[64];
	};

	typedef std::map<std::string, u32> ZoneMap;
	typedef std::vector<Zone> ZoneList;
	typedef std::vector<u32> SortedZoneList;
	typedef std::vector<Counter> CounterList;

	static ZoneMap  s_zoneMap;
	static ZoneList s_zoneList;
	static SortedZoneList s_sortedZoneList;
	static SortedZoneList s_roots;

	static ZoneMap  s_counterMap;
	static CounterList s_counterList;

	static u64 s_frameBegin;
	static f64 s_frameTime;
	static u32 s_readBuffer = 0;
	static u32 s_writeBuffer = 1;
	static u32 s_level;
	static u32 s_maxLevel;
	static u32 s_zoneStack[MAX_ZONE_STACK];
	static u64 s_currentFrame = 1;
	static u64 s_currentPath;

	void addZoneChild(u32 parentId, u32 zoneId)
	{
		Zone& parent = s_zoneList[parentId];
		Zone& zone = s_zoneList[zoneId];
		// This has already been added.
		if (zone.sibling != NULL_ZONE)
		{
			return;
		}

		if (parent.child == zoneId)
		{
			return;
		}
		else if (parent.child == NULL_ZONE)
		{
			parent.child = zoneId;
		}
		else
		{
			Zone* child = &s_zoneList[parent.child];
			while (1)
			{
				if (child->sibling == zoneId)
				{
					return;
				}
				else if (child->sibling == NULL_ZONE)
				{
					child->sibling = zoneId;
					break;
				}
				child = &s_zoneList[child->sibling];
			};
		}
	}

	u32 beginZone(const char* name, const char* func, u32 lineNumber)
	{
		ZoneMap::iterator iZone = s_zoneMap.find(name);
		u32 id = 0;

		if (iZone == s_zoneMap.end())
		{
			id = (u32)s_zoneList.size();

			Zone zone;
			zone.id = id;
			zone.path = s_currentPath;
			zone.timeInZone[s_readBuffer]  = 0;
			zone.timeInZone[s_writeBuffer] = 0;
			zone.timeInZoneAve = 0.0;
			zone.fractOfParentAve = 0.0;
			zone.frame = 0;
			
			s_zoneList.push_back(zone);
			s_zoneMap[name] = id;
		}
		else
		{
			id = iZone->second;
		}

		Zone& zone = s_zoneList[id];
		strcpy(zone.name, name);
		strcpy(zone.func, func);
		zone.lineNumber = lineNumber;
		zone.level = s_level;

		zone.parent = s_level > 0 ? s_zoneStack[s_level - 1] : NULL_ZONE;
		if (zone.parent == NULL_ZONE)
		{
			s_roots.push_back(id);
		}
		else
		{
			addZoneChild(zone.parent, zone.id);
		}

		s_zoneStack[s_level] = id;
		s_level++;

		return id;
	}

	void endZone(u32 id, u64 dt)
	{
		s_zoneList[id].timeInZone[s_writeBuffer] += TFE_System::convertFromTicksToSeconds(dt);
		s_level--;
	}

	void addCounter(const char* name, s32* counter)
	{
		ZoneMap::iterator iCounter = s_counterMap.find(name);
		if (iCounter == s_counterMap.end())
		{
			const u32 id = (u32)s_counterList.size();

			Counter newCounter;
			newCounter.id = id;
			newCounter.prevValue = *counter;
			newCounter.ptr = counter;
			strcpy(newCounter.name, name);

			s_counterList.push_back(newCounter);
			s_counterMap[name] = id;
		}
	}

	void frameBegin()
	{
		std::swap(s_readBuffer, s_writeBuffer);
		s_level = 0;
		s_maxLevel = 0;
		s_roots.clear();

		// Swap buffers, s_readBuffer is safe to read in the middle of the next frame.
		const size_t zoneCount = s_zoneList.size();
		for (size_t i = 0; i < zoneCount; i++)
		{
			s_zoneList[i].timeInZone[s_writeBuffer] = 0;
		}

		// Copy counter values from the frame, so that the results can be used
	    // in the middle of the next frame.
		const size_t counterCount = s_counterList.size();
		for (size_t i = 0; i < counterCount; i++)
		{
			s_counterList[i].prevValue = *s_counterList[i].ptr;
		}

		s_frameBegin = TFE_System::getCurrentTimeInTicks();
	}

	void traverseZoneTree(u32 id)
	{
		if (id == NULL_ZONE) { return; }
		Zone* zone = &s_zoneList[id];
		// Make sure zones are only inserted once for now.
		if (zone->frame != s_currentFrame)
		{
			s_sortedZoneList.push_back(id);
		}
		zone->frame = s_currentFrame;
		
		while (zone->child != NULL_ZONE)
		{
			traverseZoneTree(zone->child);
			zone = &s_zoneList[zone->child];
		}

		zone = &s_zoneList[id];
		while (zone->sibling != NULL_ZONE)
		{
			traverseZoneTree(zone->sibling);
			zone = &s_zoneList[zone->sibling];
		}
	}

	void frameEnd()
	{
		s_frameTime = TFE_System::convertFromTicksToSeconds(TFE_System::getCurrentTimeInTicks() - s_frameBegin);
		const size_t zoneCount = s_zoneList.size();
		const f64 expBlend = 0.99;

		// Sort Zones
		s_sortedZoneList.clear();
		const size_t rootCount = s_roots.size();
		for (size_t r = 0; r < rootCount; r++)
		{
			traverseZoneTree(s_roots[r]);
		}

		// First compute delta times for each zone.
		for (size_t i = 0; i < zoneCount; i++)
		{
			s_zoneList[i].timeInZoneAve = expBlend * s_zoneList[i].timeInZoneAve + (1.0 - expBlend)*s_zoneList[i].timeInZone[s_writeBuffer];
		}

		// Then handle percentage of parent and clear
		for (size_t i = 0; i < zoneCount; i++)
		{
			f64 parentTime = (s_zoneList[i].parent != NULL_ZONE) ? s_zoneList[s_zoneList[i].parent].timeInZone[s_writeBuffer] : s_frameTime;
			s_zoneList[i].fractOfParentAve = expBlend * s_zoneList[i].fractOfParentAve + (1.0 - expBlend)*s_zoneList[i].timeInZone[s_writeBuffer] / parentTime;
			// Handle the rare case the parentTime = 0 causing s_zoneList[i].fractOfParentAve to become NAN. Once that happens it will never fix itself
			// because we are doing an average. So fix it manually.
			if (isnan(s_zoneList[i].fractOfParentAve))
			{
				s_zoneList[i].fractOfParentAve = 0.0;
			}

			s_zoneList[i].child = NULL_ZONE;
			s_zoneList[i].sibling = NULL_ZONE;
		}

		s_currentFrame++;
	}

	u32 getZoneCount()
	{
		return (u32)s_sortedZoneList.size();
	}

	void getZoneInfo(u32 index, TFE_ZoneInfo* info)
	{
		if (index >= (u32)s_sortedZoneList.size()) { return; }

		Zone& zone = s_zoneList[s_sortedZoneList[index]];
		info->name = zone.name;
		info->func = zone.func;
		info->level = zone.level;
		info->lineNumber = zone.lineNumber;
		info->timeInZone = zone.timeInZone[s_readBuffer];
		info->timeInZoneAve = zone.timeInZoneAve;
		info->fractOfParentAve = zone.fractOfParentAve;
		info->parentId = zone.parent;
	}

	f64 getTimeInFrame()
	{
		return s_frameTime;
	}

	u32 getCounterCount()
	{
		return (u32)s_counterList.size();
	}

	void getCounterInfo(u32 index, TFE_CounterInfo* info)
	{
		if (index >= (u32)s_counterList.size()) { return; }

		Counter& counter = s_counterList[index];
		info->name = counter.name;
		info->value = counter.prevValue;
	}
}

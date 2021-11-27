#include "ldraw.h"
#include <TFE_DarkForces/GameUI/delt.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <assert.h>
#include <map>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	// For now cache by pointer.
	typedef std::map<size_t, DeltFrame*> DeltaMap;
	typedef std::map<size_t, DeltFrame*>::iterator DeltaMapIter;
	DeltaMap s_deltFrameMap;
		
	void ldraw_init()
	{
		s_deltFrameMap.clear();
	}

	DeltFrame* getDeltFrame(void* data)
	{
		size_t iPtr = size_t(data);
		DeltaMapIter iFrame = s_deltFrameMap.find(iPtr);
		if (iFrame != s_deltFrameMap.end())
		{
			return iFrame->second;
		}

		DeltFrame* frame = (DeltFrame*)game_alloc(sizeof(DeltFrame));
		loadDeltIntoFrame(frame, (u8*)data, 65536);
		s_deltFrameMap[iPtr] = frame;
		return frame;
	}

	void deltaImage(s16* data, s16 x, s16 y)
	{
		DeltFrame* frame = getDeltFrame(data);
		blitDeltaFrame(frame, x, y, vfb_getCpuBuffer());
	}

	void deltaClip(s16* data, s16 x, s16 y)
	{
		DeltFrame* frame = getDeltFrame(data);
		blitDeltaFrame(frame, x, y, vfb_getCpuBuffer());
	}

	void deltaFlip(s16* data, s16 x, s16 y, s16 w)
	{
		DeltFrame* frame = getDeltFrame(data);
		blitDeltaFrame(frame, x, y, vfb_getCpuBuffer());
	}

	void deltaFlipClip(s16* data, s16 x, s16 y, s16 w)
	{
		DeltFrame* frame = getDeltFrame(data);
		blitDeltaFrame(frame, x, y, vfb_getCpuBuffer());
	}
}
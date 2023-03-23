#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_System/math.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include <TFE_Input/input.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/shaderBuffer.h>

#include "debug.h"
#include "renderDebug.h"
#include "../rcommon.h"

namespace TFE_Jedi
{
	s32 s_maxPortals = 4096;
	s32 s_maxWallSeg = 4096;
	s32 s_portalsTraversed;
	s32 s_wallSegGenerated;
	s32 s_wallPartsGenerated;

	void debug_update()
	{
		if (TFE_Input::keyPressed(KEY_LEFTBRACKET))
		{
			if (TFE_Input::getKeyModifierDown() == KEYMOD_SHIFT)
			{
				s_maxWallSeg = max(0, s_maxWallSeg - 1);
			}
			else
			{
				s_maxPortals = max(0, s_maxPortals - 1);
			}
		}
		else if (TFE_Input::keyPressed(KEY_RIGHTBRACKET))
		{
			if (TFE_Input::getKeyModifierDown() == KEYMOD_SHIFT)
			{
				s_maxWallSeg = min(4096, s_maxWallSeg + 1);
			}
			else
			{
				s_maxPortals = min(4096, s_maxPortals + 1);
			}
		}
		else if (TFE_Input::keyPressed(KEY_C))
		{
			if (TFE_Input::getKeyModifierDown() == KEYMOD_SHIFT)
			{
				s_maxWallSeg = 4096;
			}
			else
			{
				s_maxPortals = 4096;
			}
		}
		else if (TFE_Input::keyPressed(KEY_V))
		{
			if (TFE_Input::getKeyModifierDown() == KEYMOD_SHIFT)
			{
				s_maxWallSeg = 0;
			}
			else
			{
				s_maxPortals = 0;
			}
		}
	}

	void debug_addQuad(Vec2f v0, Vec2f v1, f32 y0, f32 y1, f32 portalY0, f32 portalY1, bool isPortal)
	{
		Vec3f vtx[4];
		vtx[0] = { v0.x, y0, v0.z };
		vtx[1] = { v1.x, y0, v1.z };
		vtx[2] = { v1.x, y1, v1.z };
		vtx[3] = { v0.x, y1, v0.z };

		const Vec4f colorSolid  = { 1.0f, 0.0f, 1.0f, 1.0f };
		const Vec4f colorPortal = { 0.0f, 1.0f, 1.0f, 1.0f };

		renderDebug_addPolygon(4, vtx, colorSolid);
		if (isPortal)
		{
			vtx[0].y = portalY0;
			vtx[1].y = portalY0;
			vtx[2].y = portalY1;
			vtx[3].y = portalY1;
			renderDebug_addPolygon(4, vtx, colorPortal);
		}
	}
}
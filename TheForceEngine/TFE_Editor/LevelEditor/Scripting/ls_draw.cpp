#include "ls_draw.h"
#include "levelEditorScripts.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>
#include <TFE_Editor/LevelEditor/editGeometry.h>
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_ForceScript/forceScript.h>
#include <TFE_ForceScript/float2.h>

#ifdef ENABLE_FORCE_SCRIPT
#include <angelscript.h>

namespace LevelEditor
{
	// TODO: Finish Draw API.

	void LS_Draw::begin(TFE_ForceScript::float2 pos, f32 scale)
	{
		m_startPos = pos;
		m_scale = scale;
	}

	// TODO: Handle centering.
	void LS_Draw::rect(f32 width, f32 height, bool centered)
	{
		f32 x0 = m_startPos.x, x1 = x0 + width;
		f32 z0 = m_startPos.y, z1 = z0 + height;
		if (width < 0.0f)
		{
			std::swap(x0, x1);
		}
		if (height < 0.0f)
		{
			std::swap(z0, z1);
		}

		s_geoEdit.shape.resize(2);
		s_geoEdit.shape[0] = { x0, z0 };
		s_geoEdit.shape[1] = { x1, z1 };
		s_geoEdit.drawHeight[0] = s_grid.height;
		s_geoEdit.drawHeight[1] = s_grid.height + 8.0f;
		createSectorFromRect();
	}

	// TODO: Handle angle and centering.
	void LS_Draw::polygon(f32 radius, s32 sides, f32 angle, bool centered)
	{
		if (sides < 3)
		{
			infoPanelAddMsg(LE_MSG_ERROR, "draw.polygon() - too few sides %d, atleast 3 are required.", sides);
			return;
		}
		s_geoEdit.drawHeight[0] = s_grid.height;
		s_geoEdit.drawHeight[1] = s_grid.height + 8.0f;
		s_geoEdit.shape.resize(sides);

		const f32 cx = m_startPos.x + radius;
		const f32 cz = m_startPos.y + radius;
		const f32 dA = 2.0f * PI / f32(sides);
		f32 curAngle = 0.0f;
		
		for (s32 i = 0; i < sides; i++, curAngle += dA)
		{
			f32 x = cx + cosf(curAngle) * radius;
			f32 z = cz + sinf(curAngle) * radius;
			s_geoEdit.shape[i] = { x, z };
		}
		createSectorFromShape();
	}

	bool LS_Draw::scriptRegister(asIScriptEngine* engine)
	{
		s32 res = 0;
		// Object Type
		res = engine->RegisterObjectType("Draw", sizeof(LS_Draw), asOBJ_VALUE | asOBJ_POD); assert(res >= 0);
		// Properties

		//------------------------------------
		// Functions
		//------------------------------------
		res = engine->RegisterObjectMethod("Draw", "void begin(const float2 &in = 0, float = 1)", asMETHOD(LS_Draw, begin), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Draw", "void rect(float, float, bool = false)", asMETHOD(LS_Draw, rect), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Draw", "void polygon(float, int = 8, float = 0, bool = false)", asMETHOD(LS_Draw, polygon), asCALL_THISCALL);  assert(res >= 0);

		// Script variable.
		res = engine->RegisterGlobalProperty("Draw draw", this);  assert(res >= 0);
		return res >= 0;
	}
}

#endif
#include "ls_draw.h"
#include "levelEditorScripts.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>
#include <TFE_Editor/LevelEditor/editGeometry.h>
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_ForceScript/scriptInterface.h>
#include <TFE_ForceScript/float2.h>
#include <angelscript.h>

using namespace TFE_ForceScript;

namespace LevelEditor
{
	// TODO: Finish Draw API.

	void LS_Draw::begin(float2 pos, f32 scale)
	{
		m_startPos = pos;
		m_cursor = pos;
		m_scale = scale;
		m_angle = 0.0f;

		// Default values.
		m_floorHeight = 0.0f;
		m_ceilHeight = 8.0f;

		m_stepDir = float2(s_grid.axis[1].x, s_grid.axis[1].z);
	}

	void LS_Draw::moveForward(f32 dist)
	{
		m_cursor += m_stepDir * dist;
	}

	// TODO: Handle centering.
	void LS_Draw::rect(f32 width, f32 height, bool centered)
	{
		if (s_grid.axis[0].x == 1.0f && s_grid.axis[1].z == 1.0f)
		{
			// Draw as a rect directly.
			f32 x0 = m_cursor.x, x1 = x0 + width;
			f32 z0 = m_cursor.y, z1 = z0 + height;
			if (width < 0.0f)  { std::swap(x0, x1); }
			if (height < 0.0f) { std::swap(z0, z1); }

			s_geoEdit.shape.resize(2);
			s_geoEdit.shape[0] = { x0, z0 };
			s_geoEdit.shape[1] = { x1, z1 };
			s_geoEdit.drawHeight[0] = s_grid.height + m_floorHeight;
			s_geoEdit.drawHeight[1] = s_grid.height + m_ceilHeight;
			createSectorFromRect();
		}
		else
		{
			// Build the vertices.
			float2 pos = m_cursor;
			s_geoEdit.shape.resize(4);

			s_geoEdit.shape[0] = { pos.x, pos.y };

			pos += float2(s_grid.axis[0].x, s_grid.axis[0].z) * width;
			s_geoEdit.shape[1] = { pos.x, pos.y };

			pos += float2(s_grid.axis[1].x, s_grid.axis[1].z) * height;
			s_geoEdit.shape[2] = { pos.x, pos.y };

			pos -= float2(s_grid.axis[0].x, s_grid.axis[0].z) * width;
			s_geoEdit.shape[3] = { pos.x, pos.y };

			s_geoEdit.drawHeight[0] = s_grid.height + m_floorHeight;
			s_geoEdit.drawHeight[1] = s_grid.height + m_ceilHeight;
			createSectorFromShape();
		}
	}

	// TODO: Handle angle and centering.
	void LS_Draw::polygon(f32 radius, s32 sides, f32 angle, bool centered)
	{
		if (sides < 3)
		{
			infoPanelAddMsg(LE_MSG_ERROR, "draw.polygon() - too few sides %d, atleast 3 are required.", sides);
			return;
		}
		s_geoEdit.drawHeight[0] = s_grid.height + m_floorHeight;
		s_geoEdit.drawHeight[1] = s_grid.height + m_ceilHeight;
		s_geoEdit.shape.resize(sides);

		const f32 cx = m_cursor.x + radius;
		const f32 cz = m_cursor.y + radius;
		const f32 dA = 2.0f * PI / f32(sides);
		f32 curAngle = angle * PI / 180.0f;
		
		for (s32 i = 0; i < sides; i++, curAngle += dA)
		{
			f32 x = cx + cosf(curAngle) * radius;
			f32 z = cz + sinf(curAngle) * radius;
			s_geoEdit.shape[i] = { x, z };
		}
		createSectorFromShape();
	}

	bool LS_Draw::scriptRegister(ScriptAPI api)
	{
		ScriptClassBegin("Draw", "draw", api);
		{
			ScriptObjMethod("void begin(const float2 &in = 0, float = 1)", begin);
			ScriptObjMethod("void moveForward(float)", moveForward);
			ScriptObjMethod("void rect(float, float, bool = false)", rect);
			ScriptObjMethod("void polygon(float, int = 8, float = 0, bool = false)", polygon);
		}
		ScriptClassEnd();
	}
}

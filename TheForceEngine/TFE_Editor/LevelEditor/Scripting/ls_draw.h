#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/system.h>
#include <TFE_System/types.h>
#include <TFE_ForceScript/scriptInterface.h>
#include <string>

namespace LevelEditor
{
	class LS_Draw : public ScriptAPIClass
	{
	public:
		void begin(TFE_ForceScript::float2 pos, f32 scale);
		void moveForward(f32 dist);
		// Skip mergeMode for now.
		// Skip sector draw for now.
		// Skip push/pop for now.
		void rect(f32 width, f32 height, bool centered);
		void polygon(f32 radius, s32 sides, f32 angle, bool centered);
		// Skip sector/wall properties for now.

		// System
		bool scriptRegister(ScriptAPI api) override;
	private:
		TFE_ForceScript::float2 m_startPos;
		TFE_ForceScript::float2 m_cursor;
		TFE_ForceScript::float2 m_stepDir;
		f32 m_scale;
		f32 m_angle;
		f32 m_floorHeight;
		f32 m_ceilHeight;
	};
}

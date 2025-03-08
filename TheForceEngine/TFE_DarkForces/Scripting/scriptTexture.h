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
#include <TFE_Jedi/Level/rtexture.h>

namespace TFE_DarkForces
{
	class ScriptTexture
	{
	public:
		ScriptTexture() : m_id(-1) {};
		ScriptTexture(s32 id) : m_id(id) {};
		void registerType();

		static s32 getTextureIdFromData(TextureData** texData);
		static bool isTextureValid(ScriptTexture* tex);

	public:
		s32 m_id;
	};
}

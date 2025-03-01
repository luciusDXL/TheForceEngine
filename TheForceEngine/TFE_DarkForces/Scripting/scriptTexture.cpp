#include "scriptTexture.h"
#include <TFE_ForceScript/ScriptAPI-Shared/scriptMath.h>
#include <angelscript.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	s32 ScriptTexture::getTextureIdFromData(TextureData** texData)
	{
		if (!texData || !*texData) { return -1; }

		s32 index;
		AssetPool pool;
		if (!bitmap_getTextureIndex(*texData, &index, &pool))
		{
			return -1;
		}
		return index;
	}
	bool ScriptTexture::isTextureValid(ScriptTexture* tex)
	{
		return tex->m_id >= 0;
	}

	void ScriptTexture::registerType()
	{
		s32 res = 0;
		asIScriptEngine* engine = (asIScriptEngine*)TFE_ForceScript::getEngine();

		ScriptValueType("LevelTexture");
		// Variables
		ScriptMemberVariable("int id", m_id);
		// Functions
		ScriptObjFunc("bool isValid()", isTextureValid);
	}
}

#include "ls_selection.h"
#include "levelEditorScripts.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>
#include <TFE_ForceScript/forceScript.h>
#include <TFE_ForceScript/float2.h>

#ifdef ENABLE_FORCE_SCRIPT
#include <angelscript.h>

namespace LevelEditor
{
	// TODO: Finish Selection API once selection editor code is refactored.

	// TODO: Handle properly once selection is refactored.
	TFE_ForceScript::float2 LS_Selection::getPositionXZ(s32 index)
	{
		TFE_ForceScript::float2 pos(0.0f, 0.0f);
		if (s_editMode == LEDIT_VERTEX)
		{
			if (s_featureCur.sector && s_featureCur.featureIndex >= 0 && s_featureCur.featureIndex < (s32)s_featureCur.sector->vtx.size())
			{
				const Vec2f& vtx = s_featureCur.sector->vtx[s_featureCur.featureIndex];
				pos.x = vtx.x;
				pos.y = vtx.z;
			}
		}

		return pos;
	}

	bool LS_Selection::scriptRegister(asIScriptEngine* engine)
	{
		s32 res = 0;
		// Object Type
		res = engine->RegisterObjectType("Selection", sizeof(LS_Selection), asOBJ_VALUE | asOBJ_POD); assert(res >= 0);
		// Properties

		//------------------------------------
		// Functions
		//------------------------------------
		res = engine->RegisterObjectMethod("Selection", "float2 getPositionXZ(int = 0)", asMETHOD(LS_Selection, getPositionXZ), asCALL_THISCALL);  assert(res >= 0);

		// Script variable.
		res = engine->RegisterGlobalProperty("Selection selection", this);  assert(res >= 0);
		return res >= 0;
	}
}

#endif
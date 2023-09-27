#include "editorFrame.h"
#include "editor.h"
#include <TFE_DarkForces/mission.h>
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Renderer/rcommon.h>
#include <algorithm>
#include <vector>
#include <map>

namespace TFE_Editor
{
	u8 s_identityTable[256];
	const u8* s_remapTable = s_identityTable;
	static bool s_rebuildIdentityTable = true;
	
	void buildIdentityTable()
	{
		if (!s_rebuildIdentityTable) { return; }
		s_rebuildIdentityTable = false;

		for (s32 i = 0; i < 256; i++) { s_identityTable[i] = i; }
	}
}
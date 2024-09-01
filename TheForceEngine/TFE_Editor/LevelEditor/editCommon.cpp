#include "editCommon.h"

using namespace TFE_Editor;

namespace LevelEditor
{
	SectorList s_sectorChangeList;

	bool s_moveStarted = false;
	Vec2f s_moveStartPos;
	Vec2f s_moveStartPos1;
	Vec2f* s_moveVertexPivot = nullptr;
}

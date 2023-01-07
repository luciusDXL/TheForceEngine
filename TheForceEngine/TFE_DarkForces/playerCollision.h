#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player Collision
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Jedi/Collision/collision.h>
#include "logic.h"
#include "playerLogic.h"

namespace TFE_DarkForces
{
	typedef JBool(*CollisionObjFunc)(RSector*);
	typedef JBool(*CollisionProxFunc)();

	JBool collision_handleCrushing(RSector* sector);
	JBool collision_checkPickups(RSector* sector);
	JBool handleCollisionFunc(RSector* sector);
	void playerHandleCollisionFunc(RSector* sector, CollisionObjFunc func, CollisionProxFunc proxFunc);

	JBool handlePlayerCollision(PlayerLogic* playerLogic);
	JBool col_computeCollisionResponse(RSector* sector);
	JBool playerMove();
		
	extern fixed16_16 s_colWidth;
	extern fixed16_16 s_colDstPosX;
	extern fixed16_16 s_colDstPosY;
	extern fixed16_16 s_colDstPosZ;

	extern fixed16_16 s_colCurLowestFloor;
	extern fixed16_16 s_colCurHighestCeil;
	extern fixed16_16 s_colCurBot;
	extern fixed16_16 s_colExtCeilHeight;
	extern fixed16_16 s_colExtFloorHeight;

	extern RSector* s_colMinSector;
	extern RSector* s_colMaxSector;
	extern RWall* s_colWall0;
	extern vec2_fixed s_colResponseDir;
	extern JBool s_colResponseStep;
	extern JBool s_objCollisionEnabled;
}  // namespace TFE_DarkForces
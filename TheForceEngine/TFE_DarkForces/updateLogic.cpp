#include "updateLogic.h"
#include "player.h"
#include "time.h"
#include "util.h"
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/Collision/collision.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	struct UpdateLogic
	{
		Logic logic;

		fixed16_16 xVel;
		fixed16_16 yVel;
		fixed16_16 zVel;
		angle14_16 pitchRate;
		angle14_16 yawRate;
		angle14_16 rollRate;
		s16 pad16;
		u32 flags;
	};

	enum UpdateLogicFlags
	{
		ULFLAG_GRAVITY = FLAG_BIT(1),
		ULFLAG_PITCH   = FLAG_BIT(3),
		ULFLAG_YAW     = FLAG_BIT(4),
		ULFLAG_ROLL    = FLAG_BIT(5),
		ULFLAG_ALL = 0xffffffff
	};

	static Task* s_logicUpdateTask = nullptr;
	static Allocator* s_logicUpdateList = nullptr;

	void updateLogicTaskFunc(s32 id);
	void updateLogicCleanupFunc(Logic* logic);
	
	void updateLogic_clearTask()
	{
		s_logicUpdateList = nullptr;
		s_logicUpdateTask = nullptr;
	}

	JBool updateLogic_setupFunc(Logic* logic, KEYWORD key)
	{
		UpdateLogic* updateLogic = (UpdateLogic*)logic;

		char* endPtr = nullptr;
		if (key == KW_FLAGS)
		{
			updateLogic->flags = strToUInt(s_objSeqArg1);
			return JTRUE;
		}
		else if (key == KW_D_PITCH)
		{
			updateLogic->pitchRate = angle14_16(strtof(s_objSeqArg1, &endPtr) * 16384.0f / 360.0f);
			return JTRUE;
		}
		else if (key == KW_D_YAW)
		{
			updateLogic->yawRate = angle14_16(strtof(s_objSeqArg1, &endPtr) * 16384.0f / 360.0f);
			return JTRUE;
		}
		else if (key == KW_D_ROLL)
		{
			updateLogic->rollRate = angle14_16(strtof(s_objSeqArg1, &endPtr) * 16384.0f / 360.0f);
			return JTRUE;
		}

		return JFALSE;
	}
		
	Logic* obj_setUpdate(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		if (!s_logicUpdateList)
		{
			s_logicUpdateList = allocator_create(sizeof(UpdateLogic));
		}
		if (!s_logicUpdateTask)
		{
			s_logicUpdateTask = pushTask("update logic", updateLogicTaskFunc);
		}
		if (setupFunc)
		{
			*setupFunc = updateLogic_setupFunc;
		}

		UpdateLogic* updateLogic = (UpdateLogic*)allocator_newItem(s_logicUpdateList);
		updateLogic->flags = ULFLAG_ALL;
		updateLogic->zVel = 0;
		updateLogic->yVel = 0;
		updateLogic->xVel = 0;
		updateLogic->rollRate = 0;
		updateLogic->pad16 = 0;
		updateLogic->yawRate = 0;
		updateLogic->pitchRate = 0;

		if (!obj->logic)
		{
			obj->logic = allocator_create(sizeof(Logic*));
		}
		Logic** logicItem = (Logic**)allocator_newItem((Allocator*)obj->logic);
		*logicItem = (Logic*)updateLogic;

		updateLogic->logic.obj = obj;
		updateLogic->logic.parent = logicItem;
		updateLogic->logic.cleanupFunc = updateLogicCleanupFunc;
		updateLogic->logic.task = s_logicUpdateTask;
		obj->flags &= ~OBJ_FLAG_HAS_COLLISION;

		return (Logic*)updateLogic;
	}

	void updateLogicTaskFunc(s32 id)
	{
		task_begin;
		while (id != -1)
		{
			// No need for locals to be persistant.
			{
				fixed16_16 gravityDelta = mul16(s_gravityAccel, s_deltaTime);
				UpdateLogic* updateLogic = (UpdateLogic*)allocator_getHead(s_logicUpdateList);
				while (updateLogic)
				{
					SecObject* renderObj = updateLogic->logic.obj;
					fixed16_16 ceilHeight, floorHeight;
					fixed16_16 y = renderObj->posWS.y;
					RSector* sector = renderObj->sector;
					sector_getObjFloorAndCeilHeight(sector, y, &floorHeight, &ceilHeight);

					InfLink* link = (InfLink*)allocator_getHead(sector->infLink);
					if (link && link->type == LTYPE_SECTOR)
					{
						InfElevator* elev = link->elev;
						JBool move = JFALSE;
						/* TODO: How to get this information from the INF system.
						if ((y == sector->floorHeight && (elev->flags & INF_EFLAG_MOVE_FLOOR)) ||
							(y == sector->floorHeight + sector->secHeight && (elev->flags & INF_EFLAG_MOVE_SECHT)))
						{
							// This clears out the ceilHeight.
							s32 speed;
							vec3_fixed vel = { updateLogic->xVel, ceilHeight, updateLogic->zVel };
							inf_getMovingElevatorVelocity(elev, &vel, &speed);
						}
						*/
					}

					if (updateLogic->xVel || updateLogic->zVel)
					{
						fixed16_16 dx = mul16(updateLogic->xVel, s_deltaTime);
						fixed16_16 dz = mul16(updateLogic->zVel, s_deltaTime);
						RSector* curSector = renderObj->sector;
						fixed16_16 x0 = renderObj->posWS.x;
						fixed16_16 z0 = renderObj->posWS.z;
						fixed16_16 x1 = renderObj->posWS.x + dx;
						fixed16_16 z1 = renderObj->posWS.z + dz;
					
						RSector* newSector = collision_tryMove(curSector, x0, z0, x1, z1);
						if (newSector)
						{
							renderObj->posWS.x = x1;
							renderObj->posWS.z = z1;
							if (newSector != curSector)
							{
								sector_addObject(newSector, renderObj);
							}
						}
					}

					if (updateLogic->flags & ULFLAG_GRAVITY)
					{
						fixed16_16 yVel = updateLogic->yVel + gravityDelta;
						updateLogic->yVel = yVel;
						fixed16_16 yNew = renderObj->posWS.y + mul16(s_deltaTime, yVel);
						renderObj->posWS.y = yNew;
						// Below the ground
						if (yNew > floorHeight)
						{
							updateLogic->yVel = 0;
							renderObj->posWS.y = floorHeight;
						}
					}

					if (updateLogic->xVel || updateLogic->zVel)
					{
						fixed16_16 friction = ONE_16 - 2*s_deltaTime;	// This is NOT the same friction model used by the player.
						// xVel *= (1.0 - 2*dt)
						updateLogic->xVel = mul16(updateLogic->xVel, friction);
						updateLogic->zVel = mul16(updateLogic->zVel, friction);
					}

					angle14_16 pitchRate = updateLogic->pitchRate;
					angle14_16 yawRate   = updateLogic->yawRate;
					angle14_16 rollRate  = updateLogic->rollRate;
					if (pitchRate || yawRate || rollRate)
					{
						if (pitchRate && (updateLogic->flags & ULFLAG_PITCH))
						{
							renderObj->pitch += mul16(pitchRate, s_deltaTime);
						}
						if (yawRate && (updateLogic->flags & ULFLAG_YAW))
						{
							renderObj->yaw += mul16(yawRate, s_deltaTime);
						}
						if (rollRate && (updateLogic->flags & ULFLAG_ROLL))
						{
							renderObj->roll += mul16(rollRate, s_deltaTime);
						}
						if (renderObj->type == OBJ_TYPE_3D)
						{
							obj3d_computeTransform(renderObj);
						}
					}

					updateLogic = (UpdateLogic*)allocator_getNext(s_logicUpdateList);
				}
			}
			task_yield(TASK_NO_DELAY);
		}
		task_end;
	}

	void updateLogicCleanupFunc(Logic* logic)
	{
		// TODO(Core Game Loop Release)
	}
}  // namespace TFE_DarkForces
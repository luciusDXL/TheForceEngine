#include <cstring>

#include "mousebot.h"
#include "aiActor.h"
#include "../logic.h"
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/item.h>
#include <TFE_DarkForces/random.h>
#include <TFE_Game/igame.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	enum MouseBotState
	{
		MBSTATE_SLEEPING = 0,
		MBSTATE_ACTIVE,
		MBSTATE_DYING
	};

	struct MouseBotResources
	{
		WaxFrame* deadFrame;
		SoundSourceID sound0;
		SoundSourceID sound1;
		SoundSourceID sound2;
	};
	struct MouseBot
	{
		Logic logic;
		PhysicsActor actor;
	};

	static MouseBotResources s_mouseBotRes = { 0 };
	static MouseBot* s_curMouseBot;
	static s32 s_mouseNum = 0;

	MessageType mousebot_handleDamage(MessageType msg, MouseBot* mouseBot)
	{
		SecObject* obj = mouseBot->logic.obj;
		if (mouseBot->actor.alive)
		{
			ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
			mouseBot->actor.hp -= proj->dmg;
			if (mouseBot->actor.hp <= 0)
			{
				mouseBot->actor.state = MBSTATE_DYING;
				msg = MSG_RUN_TASK;
			}
			else
			{
				playSound3D_oneshot(s_mouseBotRes.sound1, obj->posWS);
				msg = MSG_DAMAGE;
			}
		}
		return msg;
	}

	MessageType mousebot_handleExplosion(MessageType msg, MouseBot* mouseBot)
	{
		PhysicsActor* phyActor = &mouseBot->actor;
		SecObject* obj = mouseBot->logic.obj;
		if (mouseBot->actor.alive)
		{
			fixed16_16 dmg   = s_msgArg1;
			fixed16_16 force = s_msgArg2;

			mouseBot->actor.hp -= dmg;
			vec3_fixed pushDir;
			vec3_fixed pos = { obj->posWS.x, obj->posWS.y - obj->worldHeight, obj->posWS.z };

			computeExplosionPushDir(&pos, &pushDir);
			vec3_fixed vel = { mul16(force, pushDir.x), mul16(force, pushDir.y), mul16(force, pushDir.z) };

			msg = MSG_RUN_TASK;
			if (mouseBot->actor.hp <= 0)
			{
				mouseBot->actor.state = MBSTATE_DYING;
				phyActor->vel = vel;
			}
			else
			{
				phyActor->vel = { vel.x >> 1, vel.y >> 1, vel.z >> 1 };
				playSound3D_oneshot(s_mouseBotRes.sound1, obj->posWS);
			}
		}
		return msg;
	}
		
	void mousebot_handleActiveState(MessageType msg)
	{
		struct LocalContext
		{
			MouseBot* mouseBot;
			SecObject* obj;
			PhysicsActor* phyActor;
			Actor* actor;
			CollisionInfo* colInfo;
			Tick tick;

			JBool odd;
			JBool flip;
		};
		task_begin_ctx;

		local(mouseBot) = s_curMouseBot;
		local(obj)      = local(mouseBot)->logic.obj;
		local(phyActor) = &local(mouseBot)->actor;
		local(actor)    = &local(phyActor)->actor;
		local(colInfo)  = &local(actor)->physics;

		local(actor)->target.pos = local(obj)->posWS;
		local(actor)->target.yaw = local(obj)->yaw;

		local(tick) = s_curTick;
		local(odd)  = JFALSE;
		local(flip) = JFALSE;
		while (local(phyActor)->state == MBSTATE_ACTIVE)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					msg = mousebot_handleExplosion(msg, local(mouseBot));
				}
				else if (msg == MSG_EXPLOSION)
				{
					msg = mousebot_handleDamage(msg, local(mouseBot));
				}
			} while (msg != MSG_RUN_TASK);

			if (local(phyActor)->state != MBSTATE_ACTIVE) { break; }

			// Go to sleep if the player hasn't been spotted in about 5 seconds.
			if (actor_isObjectVisible(local(obj), s_playerObject, 0x4000/*360 degrees*/, FIXED(25)/*closeDist*/))
			{
				local(tick) = s_curTick;
			}
			else
			{
				Tick dt = s_curTick - local(tick);
				// If enough time has past since the player was last spotted, go back to sleep.
				if (dt > 728) // ~5 seconds.
				{
					local(phyActor)->state = MBSTATE_SLEEPING;
					break;
				}
			}

			angle14_32 yaw = local(obj)->yaw & ANGLE_MASK;
			angle14_32 actorYaw = local(actor)->target.yaw & ANGLE_MASK;
			if (actorYaw == yaw)
			{
				local(odd) = (s_curTick & 1) ? JTRUE : JFALSE;
				angle14_32 deltaYaw = random(16338);
				local(actor)->target.yaw = local(odd) ? (local(obj)->yaw + deltaYaw) : (local(obj)->yaw - deltaYaw);

				local(actor)->target.speedRotation = random(0x3555) + 0x555;
				local(actor)->target.flags |= 4;
				local(flip) = JFALSE;
			}

			if (local(colInfo)->wall)
			{
				s32 rnd = random(100);
				if (rnd <= 10)	// ~10% chance of playing a sound effect when hitting a wall.
				{
					playSound3D_oneshot(s_mouseBotRes.sound0, local(obj)->posWS);
				}
				if (local(odd))
				{
					local(actor)->target.yaw += 4096;
				}
				else
				{
					local(actor)->target.yaw -= 4096;
				}
				local(actor)->target.speedRotation = random(0x3555);
				local(flip) = ~local(flip);
			}

			fixed16_16 speed = local(flip) ? -FIXED(22) : FIXED(22);
			fixed16_16 cosYaw, sinYaw;
			sinCosFixed(local(obj)->yaw, &sinYaw, &cosYaw);
			local(actor)->target.pos.x = local(obj)->posWS.x + mul16(sinYaw, speed);
			local(actor)->target.pos.z = local(obj)->posWS.z + mul16(cosYaw, speed);

			local(actor)->target.speed = FIXED(22);
			local(actor)->target.flags |= 1;
		}
		task_end;
	}

	void mousebot_die(MessageType msg)
	{
		struct LocalContext
		{
			MouseBot* mouseBot;
			Actor* actor;
			SecObject* obj;
			RSector* sector;
		};
		task_begin_ctx;
		local(mouseBot) = s_curMouseBot;
		local(obj)      = local(mouseBot)->logic.obj;
		local(actor)    = &local(mouseBot)->actor.actor;
		local(sector)   = local(obj)->sector;

		local(actor)->target.flags |= 8;
		playSound3D_oneshot(s_mouseBotRes.sound2, local(obj)->posWS);

		while (1)
		{
			task_yield(TASK_NO_DELAY);
			if (msg == MSG_RUN_TASK && actor_canDie(&local(mouseBot)->actor))
			{
				break;
			}
		}

		// Spawn a small explosion.
		spawnHitEffect(HEFFECT_SMALL_EXP, local(sector), local(obj)->posWS, local(obj));

		fixed16_16 secHeight = local(sector)->secHeight - 1;
		if (secHeight < 0)  // Not a water sector.
		{
			// Create the dead version of the mousebot.
			SecObject* newObj = allocateObject();
			frame_setData(newObj, s_mouseBotRes.deadFrame);

			newObj->frame = 0;
			newObj->posWS = local(obj)->posWS;
			newObj->entityFlags |= ETFLAG_CORPSE;
			newObj->worldWidth = 0;
			newObj->worldHeight = 0;
			sector_addObject(local(sector), newObj);

			// Spawn a battery.
			SecObject* item = item_create(ITEM_BATTERY);
			item->posWS = local(obj)->posWS;
			sector_addObject(local(sector), item);
		}

		local(mouseBot)->actor.alive = JFALSE;
		task_end;
	}

	void mouseBotLocalMsgFunc(MessageType msg)
	{
		MouseBot* mouseBot = (MouseBot*)task_getUserData();

		if (msg == MSG_DAMAGE)
		{
			mousebot_handleDamage(msg, mouseBot);
		}
		else if (msg == MSG_EXPLOSION)
		{
			mousebot_handleExplosion(msg, mouseBot);
		}
	}

	void mouseBotTaskFunc(MessageType msg)
	{
		struct LocalContext
		{
			MouseBot* mouseBot;
			PhysicsActor* actor;
			SecObject* obj;
		};
		task_begin_ctx;
		local(mouseBot) = (MouseBot*)task_getUserData();
		local(actor) = &local(mouseBot)->actor;

		while (local(mouseBot)->actor.alive)
		{
			if (local(actor)->state == MBSTATE_ACTIVE)
			{
				s_curMouseBot = local(mouseBot);
				task_callTaskFunc(mousebot_handleActiveState);
				continue;
			}
			else if (local(actor)->state == MBSTATE_DYING)
			{
				s_curMouseBot = local(mouseBot);
				task_callTaskFunc(mousebot_die);
				continue;
			}

			local(obj) = local(mouseBot)->logic.obj;
			while (local(actor)->state == MBSTATE_SLEEPING)
			{
				task_yield(145);
				if (msg == MSG_RUN_TASK)
				{
					// Wakeup if the player is visible.
					if (local(actor)->state == MBSTATE_SLEEPING && actor_isObjectVisible(local(obj), s_playerObject, 0x4000/*full 360 degree fov*/, FIXED(25)/*25 units "close distance"*/))
					{
						playSound3D_oneshot(s_mouseBotRes.sound0, local(obj)->posWS);
						local(mouseBot)->actor.state = MBSTATE_ACTIVE;
					}
				}
			}
		}

		// The Mousebot is dead.
		while (msg != MSG_RUN_TASK)
		{
			task_yield(TASK_NO_DELAY);
		}
		actor_removePhysicsActorFromWorld(&local(mouseBot)->actor);
		deleteLogicAndObject(&local(mouseBot)->logic);
		level_free(local(mouseBot));

		task_end;
	}

	void mouseBotLogicCleanupFunc(Logic* logic)
	{
		MouseBot* mouseBot = (MouseBot*)logic;

		Task* task = mouseBot->actor.actorTask;
		actor_removePhysicsActorFromWorld(&mouseBot->actor);
		deleteLogicAndObject(&mouseBot->logic);
		level_free(mouseBot);
		task_free(task);
	}

	Logic* mousebot_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		if (!s_mouseBotRes.deadFrame)
		{
			s_mouseBotRes.deadFrame = TFE_Sprite_Jedi::getFrame("dedmouse.fme");
		}
		if (!s_mouseBotRes.sound0)
		{
			s_mouseBotRes.sound0 = sound_Load("eeek-1.voc");
		}
		if (!s_mouseBotRes.sound1)
		{
			s_mouseBotRes.sound1 = sound_Load("eeek-2.voc");
		}
		if (!s_mouseBotRes.sound2)
		{
			s_mouseBotRes.sound2 = sound_Load("eeek-3.voc");
		}

		MouseBot* mouseBot = (MouseBot*)level_alloc(sizeof(MouseBot));
		memset(mouseBot, 0, sizeof(MouseBot));

		// Give the name of the task a number so I can tell them apart when debugging.
		char name[32];
		sprintf(name, "mouseBot%d", s_mouseNum);
		s_mouseNum++;
		Task* mouseBotTask = createSubTask(name, mouseBotTaskFunc, mouseBotLocalMsgFunc);
		task_setUserData(mouseBotTask, mouseBot);

		obj->flags &= ~OBJ_FLAG_ENEMY;
		obj->entityFlags = ETFLAG_AI_ACTOR;
		mouseBot->logic.obj = obj;

		PhysicsActor* physActor = &mouseBot->actor;
		physActor->alive = JTRUE;
		physActor->hp = FIXED(10);
		physActor->actorTask = mouseBotTask;
		physActor->state = MBSTATE_SLEEPING;
		physActor->actor.header.obj = obj;
		physActor->actor.physics.obj = obj;
		actor_addPhysicsActorToWorld(physActor);
		actor_setupSmartObj(&physActor->actor);

		JediModel* model = obj->model;
		fixed16_16 width = model->radius + ONE_16;
		obj->worldWidth  = width;
		obj->worldHeight = width >> 1;

		physActor->actor.physics.botOffset = 0;
		physActor->actor.physics.yPos = 0;
		physActor->actor.collisionFlags = (physActor->actor.collisionFlags | 3) & 0xfffffffb;
		physActor->actor.physics.height = obj->worldHeight + HALF_16;
		physActor->actor.target.speed = FIXED(22);
		physActor->actor.target.speedRotation = FIXED(3185);
		physActor->actor.target.flags &= 0xfffffff0;
		physActor->actor.target.pitch = obj->pitch;
		physActor->actor.target.yaw   = obj->yaw;
		physActor->actor.target.roll  = obj->roll;

		obj_addLogic(obj, (Logic*)mouseBot, mouseBotTask, mouseBotLogicCleanupFunc);
		if (setupFunc)
		{
			*setupFunc = nullptr;
		}
		return (Logic*)mouseBot;
	}
}  // namespace TFE_DarkForces
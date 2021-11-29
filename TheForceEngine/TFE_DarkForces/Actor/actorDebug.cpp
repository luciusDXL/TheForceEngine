#include <climits>

#include "actorDebug.h"
#include "actor.h"
#include <TFE_Game/igame.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/pickup.h>
#include <TFE_DarkForces/player.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_FrontEndUI/console.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	// TFE specific
	static SecObject* s_selectedActorObj = nullptr;

	void console_selectClosestActor(const ConsoleArgList& args)
	{
		fixed16_16 minDist = INT_MAX;
		SecObject* actorObj = nullptr;
		for (s32 i = 0; i < s_drawnSpriteCount; i++)
		{
			SecObject* obj = s_drawnSprites[i];
			if (obj && (obj->entityFlags & ETFLAG_AI_ACTOR))
			{
				fixed16_16 dx = obj->posWS.x - s_playerObject->posWS.x;
				fixed16_16 dz = obj->posWS.z - s_playerObject->posWS.z;
				fixed16_16 dist = vec2Length(dx, dz);

				if (dist > 0 && dist < minDist)
				{
					minDist = dist;
					actorObj = obj;
				}
			}
		}
		s_selectedActorObj = actorObj;
		if (actorObj)
		{
			Logic** logicList = (Logic**)allocator_getHead((Allocator*)actorObj->logic);
			ActorLogic* baseLogic = (ActorLogic*)*logicList;

			char res[256];
			sprintf(res, "Actor Found: %s", task_getName(baseLogic->logic.task));
			TFE_Console::addToHistory(res);
		}
		else
		{
			TFE_Console::addToHistory("No actor selected.");
		}
	}

	void printTarget(ActorTarget* target)
	{
		char res[256];
		TFE_Console::addToHistory("<Target> {");

		sprintf(res, "Pos: (%.3f, %.3f, %.3f)", fixed16ToFloat(target->pos.x), fixed16ToFloat(target->pos.y), fixed16ToFloat(target->pos.z));
		TFE_Console::addToHistory(res);

		sprintf(res, "Angles: (%.3f, %.3f, %.3f)", f32(target->pitch)*360.0f/16384.0f, f32(target->yaw)*360.0f/16384.0f, f32(target->roll)*360.0f/16384.0f);
		TFE_Console::addToHistory(res);

		sprintf(res, "Speed: %.3f u/s; Vertical Speed: %.3f u/s; Rotation Speed: %.3f deg/sec.", fixed16ToFloat(target->speed), fixed16ToFloat(target->speedVert), f32(target->speedRotation)*360.0f / 16384.0f);
		TFE_Console::addToHistory(res);

		sprintf(res, "Flags: %u", target->flags);
		TFE_Console::addToHistory(res);
		TFE_Console::addToHistory("}");
	}

	void printTiming(ActorTiming* timing)
	{
		char res[256];
		TFE_Console::addToHistory("<Timing> {");

		sprintf(res, "Delay: %u", timing->delay);
		TFE_Console::addToHistory(res);

		sprintf(res, "State Delay: [0] %u, [2] %u, [4] %u, [1] %u", timing->state0Delay, timing->state2Delay, timing->state4Delay, timing->state1Delay);
		TFE_Console::addToHistory(res);

		sprintf(res, "Next Tick: %u", timing->nextTick);
		TFE_Console::addToHistory(res);

		TFE_Console::addToHistory("}");
	}

	void printAnimation(LogicAnimation* anim)
	{
		char res[256];
		TFE_Console::addToHistory("<Animation> {");

		sprintf(res, "Animation ID: %d; State: %d", anim->animId, anim->state);
		TFE_Console::addToHistory(res);

		sprintf(res, "Frame Rate: %u; Frame Count: %.2f", anim->frameRate, fixed16ToFloat(anim->frameCount));
		TFE_Console::addToHistory(res);

		sprintf(res, "Current Frame: %.2f; Start Frame: %.2f", fixed16ToFloat(anim->frame), fixed16ToFloat(anim->startFrame));
		TFE_Console::addToHistory(res);

		TFE_Console::addToHistory("}");
	}

	void printEnemyComponent(ActorEnemy* enemy)
	{
		printTarget(&enemy->target);
		printTiming(&enemy->timing);
		printAnimation(&enemy->anim);

		char res[256];
		sprintf(res, "Fire Spread: %.3f", fixed16ToFloat(enemy->fireSpread));
		TFE_Console::addToHistory(res);

		sprintf(res, "Fire Offset: (%.3f, %.3f, %.3f)", fixed16ToFloat(enemy->fireOffset.x), fixed16ToFloat(enemy->fireOffset.y), fixed16ToFloat(enemy->fireOffset.z));
		TFE_Console::addToHistory(res);

		sprintf(res, "Attack: Flags: %u; Melee Range %.2f, Min/Max Dist: (%.2f, %.2f); Melee Dmg %.2f", enemy->attackFlags, fixed16ToFloat(enemy->meleeRange), fixed16ToFloat(enemy->minDist), fixed16ToFloat(enemy->maxDist), fixed16ToFloat(enemy->meleeDmg));
		TFE_Console::addToHistory(res);
	}

	void printSimpleComponent(ActorSimple* simple)
	{
		printTarget(&simple->target);
		printAnimation(&simple->anim);
		
		char res[256];
		sprintf(res, "Player Last Seen: 0x%x", simple->playerLastSeen);
		TFE_Console::addToHistory(res);

		sprintf(res, "Target Variation: offset %.3f; variation %.3f; approach variation %.3f", fixed16ToFloat(simple->targetOffset), fixed16ToFloat(simple->targetVariation), fixed16ToFloat(simple->approachVariation));
		TFE_Console::addToHistory(res);
	}

	void printFlyerComponent(ActorFlyer* flyer)
	{
		printTarget(&flyer->target);
		
		char res[256];
		sprintf(res, "State %u", flyer->state);
		TFE_Console::addToHistory(res);
	}

	void console_showActorInfo(const ConsoleArgList& args)
	{
		if (!s_selectedActorObj)
		{
			TFE_Console::addToHistory("No actor selected.");
			return;
		}

		Logic** logicList = (Logic**)allocator_getHead((Allocator*)s_selectedActorObj->logic);
		Logic* baseLogic = *logicList;
		const char* taskName = task_getName(baseLogic->task);
			   		
		if (strcasecmp(taskName, "actor") == 0)
		{
			ActorLogic* actorLogic = (ActorLogic*)baseLogic;
			TFE_Console::addToHistory("---Standard Enemy---");

			char res[256];
			sprintf(res, "flags: %u; delay: %u; fov: %.2f degrees", actorLogic->flags, actorLogic->delay, f32(actorLogic->fov) * 360.0f / 16384.0f);
			TFE_Console::addToHistory(res);

			sprintf(res, "nextTick: %u; velocity: (%.3f, %.3f, %.3f) units/sec.", actorLogic->nextTick, fixed16ToFloat(actorLogic->vel.x), fixed16ToFloat(actorLogic->vel.y), fixed16ToFloat(actorLogic->vel.z));
			TFE_Console::addToHistory(res);

			Actor* actor = actorLogic->actor;
			TFE_Console::addToHistory("-------------------------");
			TFE_Console::addToHistory("     Main Actor");
			TFE_Console::addToHistory("-------------------------");
			printTarget(&actor->target);

			sprintf(res, "Movement: (%.3f, %.3f, %.3f)", fixed16ToFloat(actor->delta.x), fixed16ToFloat(actor->delta.y), fixed16ToFloat(actor->delta.z));
			TFE_Console::addToHistory(res);

			sprintf(res, "CollisionInfo: last delta (%.3f, %.3f, %.3f); offsets: (%.3f, %.3f)", fixed16ToFloat(actor->physics.offsetX),
				fixed16ToFloat(actor->physics.offsetY), fixed16ToFloat(actor->physics.offsetZ), fixed16ToFloat(actor->physics.botOffset), fixed16ToFloat(actor->physics.yPos));
			TFE_Console::addToHistory(res);

			sprintf(res, "Col Response: Responding %d; Response Dir (%.3f, %.3f); Response Pos (%.3f, %.3f); Angle %.3f", actor->physics.responseStep ? 1 : 0,
				fixed16ToFloat(actor->physics.responseDir.x), fixed16ToFloat(actor->physics.responseDir.z), 
				fixed16ToFloat(actor->physics.responsePos.x), fixed16ToFloat(actor->physics.responsePos.z), f32(actor->physics.responseAngle) * 360.0f / 16384.0f);
			TFE_Console::addToHistory(res);

			sprintf(res, "CollisionWall %d; flags %u", actor->collisionWall ? 1 : 0, actor->collisionFlags);
			TFE_Console::addToHistory(res);
			
			for (s32 i = 0; i < 6; i++)
			{
				AiActor* actor = actorLogic->aiActors[i];
				if (!actor)
				{
					continue;
				}

				SubActorType type = actorLogic->type[i];
				switch (type)
				{
					case SAT_AI_ACTOR:
					{
						TFE_Console::addToHistory("-------------------------");
						TFE_Console::addToHistory("     AI Component");
						TFE_Console::addToHistory("-------------------------");
						sprintf(res, "HP: %f", fixed16ToFloat(actor->hp));
						TFE_Console::addToHistory(res);
						printEnemyComponent(&actor->enemy);
					} break;
					case SAT_ENEMY:
					{
						TFE_Console::addToHistory("-------------------------");
						TFE_Console::addToHistory("     Enemy Component");
						TFE_Console::addToHistory("-------------------------");
						printEnemyComponent((ActorEnemy*)actor);
					} break;
					case SAT_SIMPLE:
					{
						TFE_Console::addToHistory("-------------------------");
						TFE_Console::addToHistory("     Simple Component");
						TFE_Console::addToHistory("-------------------------");
						printSimpleComponent((ActorSimple*)actor);
					} break;
					case SAT_FLYER:
					{
						TFE_Console::addToHistory("-------------------------");
						TFE_Console::addToHistory("     Flying Component");
						TFE_Console::addToHistory("-------------------------");
						printFlyerComponent((ActorFlyer*)actor);
					} break;
					default:
					{
						TFE_Console::addToHistory("--- Unknown Component ---");
					}
				}
			}
		}
		else if (strncasecmp(taskName, "mouseBot", strlen("mouseBot")) == 0)
		{
			TFE_Console::addToHistory("---Mouse Bot---");
		}
		else if (strncasecmp(taskName, "kellDragon", strlen("kellDragon")) == 0)
		{
			TFE_Console::addToHistory("---Kell Dragon---");
		}
		else if (strncasecmp(taskName, "PhaseOne", strlen("PhaseOne")) == 0)
		{
			TFE_Console::addToHistory("---Phase 1 Dark Trooper---");
		}
		else if (strncasecmp(taskName, "turret", strlen("turret")) == 0)
		{
			TFE_Console::addToHistory("---Turret---");
		}
		else if (strncasecmp(taskName, "Welder", strlen("Welder")) == 0)
		{
			TFE_Console::addToHistory("---Welder---");
		}
	}

	void actorDebug_init()
	{
		// TFE Specific
		CCMD("selectActor", console_selectClosestActor, 0, "Selects the closest actor.");
		CCMD("showActorInfo", console_showActorInfo, 0, "Shows information about the selected actor.");
		s_selectedActorObj = nullptr;
	}

	void actorDebug_free()
	{
	}

	void actorDebug_clear()
	{
		s_selectedActorObj = nullptr;
	}
}  // namespace TFE_DarkForces
#include "infSystem.h"
#include "allocator.h"
#include <TFE_System/system.h>
#include <TFE_System/memoryPool.h>
#include <TFE_System/math.h>
#include <TFE_Game/gameConstants.h>
// Move out shared code.
#include <TFE_JediRenderer/jediRenderer.h>
#include <TFE_JediRenderer/rmath.h>
#include <TFE_JediRenderer/robject.h>
#include <TFE_JediRenderer/rsector.h>
#include <TFE_JediRenderer/rwall.h>
#include "infTypesInternal.h"
// Include update functions
#include "infElevatorUpdateFunc.h"

using namespace TFE_GameConstants;
using namespace TFE_JediRenderer;
using namespace InfAllocator;

// Move to Jedi Renderer.
struct TextureData
{
	s16 width;		// if = 1 then multiple BM in the file
	s16 height;		// EXCEPT if SizeY also = 1, in which case
					// it is a 1x1 BM
	s16 uvWidth;	// portion of texture actually used
	s16 uvHeight;	// portion of texture actually used

	s32 dataSize;	// Data size for compressed BM
					// excluding header and columns starts table
					// If not compressed, DataSize is unused

	u8 logSizeY;	// logSizeY = log2(SizeY)
					// logSizeY = 0 for weapons
	u8 pad1[3];
	u8* image;		// 0x10
	s32 u14;		// 0x14

	// 4 bytes
	u8 flags;
	u8 compressed; // 0 = not compressed, 1 = compressed (RLE), 2 = compressed (RLE0)
	u8 pad3[2];
};

namespace TFE_InfSystem
{
	typedef union { RSector* sector; RWall* wall; } InfTriggerObject;
	static const s32 c_ticksPerSec = 145;

	// Inventory stuff to be moved to gameplay.
	static s32 s_invRedKey = 0;
	static s32 s_invYellowKey = 0;
	static s32 s_invBlueKey = 0;
	// The size of the goals array is arbitrary, I don't know the largest possible size yet.
	static s32 s_goals[16] = { 0 };

	// System -- TODO
	static s32 s_needKeySoundId = 0;	// TODO
	static s32 s_switchDefaultSndId = 0;	// TODO
	
	// INF delta time in ticks.
	static u32 s_curTick;			// current time in "ticks"
	static fixed16_16 s_deltaTime;	// current delta time in seconds.
	static s32 s_triggerCount = 0;
	static Allocator* s_infElevators;

	static s32 s_infMsgArg1;
	static s32 s_infMsgArg2;
	static u32 s_infMsgEvent;
	static void* s_infMsgTarget;
	static void* s_infMsgEntity;

	// Pull from data...
	static RSector* s_sectors;
	static u32 s_sectorCount;
	static SecObject* s_playerObject;
	static u32 s_playerSecMoved = 0;

	void infElevatorMsgFunc(InfMessageType msgType);
	void infTriggerMsgFunc(InfMessageType msgType);
	
	void deleteElevator(InfElevator* elev);
	void deleteTrigger(InfTrigger* trigger);
	s32 updateElevator(InfElevator* elev);
	void elevHandleStopDelay(InfElevator* elev);
	Stop* inf_advanceStops(Allocator* stops, s32 absoluteStop, s32 relativeStop);
	void inf_adjustTextureWallOffsets_Floor(RSector* sector, s32 floorDelta);
	void inf_adjustTextureMirrorOffsets_Floor(RSector* sector, s32 floorDelta);
	void inf_sendSectorMessageInternal(RSector* sector, InfMessageType msgType);

	void inf_stopAdjoinCommands(Stop* stop);
	void inf_stopHandleMessages(Stop* stop);
	void inf_handleMsgLights();
	vec3_fixed inf_getElevSoundPos(InfElevator* elev);
	
	// TODO: Move these to the right place in the renderer or shared location.
	s32 sector_getMaxObjectHeight(RSector* sector) { return 0; }
	void sector_adjustHeights(RSector* sector, s32 floorDelta, s32 ceilDelta, s32 secHeightDelta) {}
	void sector_setupWallDrawFlags(RSector* sector) {}
	void sector_deleteElevatorLink(RSector* sector, InfElevator* elev) {}
	u32 sector_moveWalls(RSector* sector, fixed16_16 delta, fixed16_16 dirX, fixed16_16 dirZ, u32 flags);

	// TODO: System functions, to be connected later.
	void sendTextMessage(s32 msgId) {}
	void playSound2D(s32 soundId) {}
	void playSound3D_oneshot(s32 soundId, vec3_fixed pos) {}
	s32  playSound3D_looping(s32 sourceId, s32 soundId, vec3_fixed pos) { return 0; }
	void stopSound(s32 sourceId) {}

	// TODO: Game side functions
	void game_levelComplete() {}
	void createLevelEndTask() {}

	/////////////////////////////////////////////////////
	// API
	/////////////////////////////////////////////////////
	bool init()
	{
		return false;
	}

	void shutdown()
	{
	}

	// For now load the INF data directly.
	// Move back to asset later.
	void loadINF(const char* levelName)
	{
	}
			
	// Per frame update.
	void update_elevators()
	{
		InfElevator* elev = (InfElevator*)allocator_getHead(s_infElevators);
		while (elev)
		{
			s32 elevDeleted = 0;
			if ((elev->updateFlags & ELEV_MASTER_ON) && elev->nextTick < s_curTick)
			{
				// If not already moving, get started.
				if (elev->updateFlags & ELEV_MOVING)
				{
					// Figure out the source position for the sound effect.
					vec3_fixed sndPos = inf_getElevSoundPos(elev);

					// Play the startup sound effect if the elevator is not already at the next stop.
					Stop* nextStop = elev->nextStop;
					// Play the initial sound as the elevator starts moving.
					if (*elev->value != elev->nextStop->value)
					{
						playSound3D_oneshot(elev->sound0, sndPos);
					}

					// Update the next time, so this will move on the next update.
					elev->nextTick = s_curTick;

					// Flag the elevator as moving.
					elev->updateFlags |= ELEV_MOVING;
				}
				// If the elevator is moving, then play the looping sound.
				if (elev->updateFlags & ELEV_MOVING)
				{
					// Figure out the source position for the sound effect.
					vec3_fixed sndPos = inf_getElevSoundPos(elev);

					// Start up the sound effect, track it since it is looping.
					elev->soundSource1 = playSound3D_looping(elev->soundSource1, elev->sound1, sndPos);
				}

				// if reachedStop = 0, elevator has not reached the next stop.
				s32 reachedStop = updateElevator(elev);
				if (reachedStop)
				{
					elevHandleStopDelay(elev);

					Stop* nextStop = elev->nextStop;
					if (elev->updateFlags & ELEV_MOVING_REVERSE)
					{
						elev->nextTick = s_curTick + c_ticksPerSec;		// this will pause the elevator for one second.
						elev->updateFlags &= ~ELEV_MOVING_REVERSE;		// remove the reverse flag.
					}
					else
					{
						u32 delay = nextStop->delay;
						if (delay == IDELAY_HOLD)
						{
							elev->trigMove = TRIGMOVE_HOLD;
						}
						else if (delay == IDELAY_COMPLETE || delay == IDELAY_TERMINATE)
						{
							// delete the elevator, we're done here.
							deleteElevator(elev);
							elevDeleted = 1;
							if (delay == IDELAY_COMPLETE)
							{
								game_levelComplete();
								createLevelEndTask();
							}
						}
						else  // Timed
						{
							elev->nextTick = s_curTick + nextStop->delay;
						}
					}

					// Process stop messages if the elevator has not been deleted.
					if (!elevDeleted)
					{
						// Messages
						inf_stopHandleMessages(nextStop);

						// Adjoin Commands.
						inf_stopAdjoinCommands(nextStop);

						// Floor texture change.
						TextureData* floorTex = nextStop->floorTex;
						if (floorTex)
						{
							RSector* sector = elev->sector;
							sector->floorTex = &floorTex;
						}

						// Ceiling texture change.
						TextureData* ceilTex = nextStop->ceilTex;
						if (ceilTex)
						{
							RSector* sector = elev->sector;
							sector->ceilTex = &ceilTex;
						}

						// Page (special 2D sound effect that plays, such as voice overs).
						s32 pageId = nextStop->pageId;
						if (pageId)
						{
							playSound2D(pageId);
						}

						// Advance to the next stop.
						elev->nextStop = inf_advanceStops(elev->stops, 0, 1);
					} // (!elevDeleted)
				}
			} // ((elev->updateFlags & ELEV_MASTER_ON) && elev->nextTick < s_curTick)

			// Next elevator.
			elev = (InfElevator*)allocator_getNext(s_infElevators);
		} // while (elev)
	}

	// Per frame animated texture update (this may be moved later).
	void update_animatedTextures()
	{
	}

	// Send messages so that entities and the player can interact with the INF system.
	// If msgType = IMSG_SET/CLEAR_BITS the msg is processed directly for this wall OR
	// this iterates through the valid links and calls their msgFunc.
	void inf_wallSendMessage(RWall* wall, u32 entity, u32 evt, InfMessageType msgType)
	{
		if (msgType == IMSG_SET_BITS)
		{
			s32 flagsIndex = s_infMsgArg1;
			u32 bits = s_infMsgArg2;
			if (flagsIndex == 1)
			{
				wall->flags1 |= bits;

				// If there is a mirror, also set some of the bits there.
				RWall* mirror = wall->mirrorWall;
				if (mirror)
				{
					const u32 allowedMirrorFlags = (WF1_HIDE_ON_MAP | WF1_SHOW_NORMAL_ON_MAP | WF1_DAMAGE_WALL | WF1_SHOW_AS_LEDGE_ON_MAP | WF1_SHOW_AS_DOOR_ON_MAP);
					mirror->flags1 |= (bits & allowedMirrorFlags);
				}
			}
			else if (flagsIndex == 2)
			{
				wall->flags2 |= bits;
			}
			// It looks like if mirror is set for flags2, it also sets them for 3...
			else if (flagsIndex == 3)
			{
				wall->flags3 |= bits;

				// If there is a mirror, also set some of the bits there.
				RWall* mirror = wall->mirrorWall;
				if (mirror)
				{
					mirror->flags3 |= (bits & 0x0f);
				}
			}
		}
		else if (msgType == IMSG_CLEAR_BITS)
		{
			s32 flagsIndex = s_infMsgArg1;
			u32 bits = s_infMsgArg2;
			if (flagsIndex == 1)
			{
				wall->flags1 &= ~bits;

				// If there is a mirror, also clear some of the bits there.
				RWall* mirror = wall->mirrorWall;
				if (mirror)
				{
					const u32 allowedMirrorFlags = WF1_HIDE_ON_MAP | WF1_SHOW_NORMAL_ON_MAP | WF1_DAMAGE_WALL | WF1_SHOW_AS_LEDGE_ON_MAP | WF1_SHOW_AS_DOOR_ON_MAP;
					mirror->flags1 &= ~(bits & allowedMirrorFlags);
				}
			}
			else if (flagsIndex == 2)
			{
				wall->flags2 &= ~bits;
			}
			else if (flagsIndex == 3)
			{
				wall->flags3 &= ~bits;

				// If there is a mirror, also set some of the bits there.
				RWall* mirror = wall->mirrorWall;
				if (mirror)
				{
					mirror->flags3 &= ~(bits & 0x0f);
				}
			}
		}
		else
		{
			Allocator* infLink = wall->infLink;
			if (infLink)
			{
				InfLink* link = (InfLink*)allocator_getHead(infLink);
				while (link)
				{
					// Fire off the link task if the event and entity match the requirements.
					if ((evt == 0 || (link->eventMask & evt)) && (entity == 0 || (link->entityMask & entity)) && link->msgFunc)
					{
						s_infMsgEntity = (void*)entity;
						s_infMsgTarget = link->target;
						s_infMsgEvent = evt;

						allocator_addRef(infLink);
						link->msgFunc(msgType);
						allocator_release(infLink);
					}
					link = (InfLink*)allocator_getNext(infLink);
				}
			}
		}
	}

	// Send messages so that entities and the player can interact with the INF system.
	// If msgType = IMSG_SET/CLEAR_BITS, IMSG_MASTER_ON/OFF, IMSG_WAKEUP it is processed directly for this sector AND
	// this iterates through the valid links and calls their msgFunc.
	void inf_sectorSendMessage(RSector* sector, SecObject* obj, u32 evt, InfMessageType msgType)
	{
		inf_sendSectorMessageInternal(sector, msgType);

		Allocator* infLink = sector->infLink;
		if (infLink)
		{
			InfLink* link = (InfLink*)allocator_getHead(infLink);
			while (link)
			{
				if ((evt == 0 || (link->eventMask & evt)) && (!obj || (link->entityMask & obj->typeFlags)) && link->msgFunc)
				{
					s_infMsgEntity = obj;
					s_infMsgTarget = link->target;
					s_infMsgEvent = evt;

					allocator_addRef(infLink);
					link->msgFunc(msgType);
					allocator_release(infLink);
				}
				link = (InfLink*)allocator_getNext(infLink);
			}
		}
	}

	/////////////////////////////////////////////////////
	// Internal
	/////////////////////////////////////////////////////
	
	// Update an elevator.
	// Returns -1 if the elevator has reached the next stop, else 0.
	s32 updateElevator(InfElevator* elev)
	{
		fixed16_16* value = elev->value;
		Stop* nextStop = elev->nextStop;

		fixed16_16 v0 = *value;
		fixed16_16 v1 = v0;
		if (nextStop)
		{
			v1 = nextStop->value;
		}

		// The elevator has reached the next stop.
		if (v1 == v0 && nextStop)
		{
			return -1;
		}

		InfElevatorType type = elev->type;
		InfUpdateFunc updateFunc = nullptr;
		switch (type)
		{
			case IELEV_MOVE_CEILING:
			case IELEV_MOVE_FLOOR:
			case IELEV_MOVE_OFFSET:
			case IELEV_MOVE_FC:
				updateFunc = infUpdate_moveHeights;
				break;
			case IELEV_MOVE_WALL:
				updateFunc = infUpdate_moveWall;
				break;
			case IELEV_ROTATE_WALL:
				updateFunc = infUpdate_rotateWall;
				break;
			case IELEV_SCROLL_WALL:
				updateFunc = infUpdate_scrollWall;
				break;
			case IELEV_SCROLL_FLOOR:
			case IELEV_SCROLL_CEILING:
				updateFunc = infUpdate_scrollFlat;
				break;
			case IELEV_CHANGE_LIGHT:
				updateFunc = infUpdate_changeAmbient;
				break;
			case IELEV_CHANGE_WALL_LIGHT:
				updateFunc = infUpdate_changeWallLight;
				break;
		}

		fixed16_16 frameDelta = 0;
		if (!nextStop)
		{
			// If there are no stops, then the elevator keeps going forever...
			// Useful for scrolling textures, constantly spinning gears, etc.
			frameDelta = mul16(elev->speed, s_deltaTime);
		}
		else
		{
			fixed16_16 delta = v1 - v0;
			frameDelta = delta;
			if (elev->speed)
			{
				if (!elev->fixedStep)
				{
					fixed16_16 change = mul16(elev->speed, s_deltaTime);
					if (delta > change)
					{
						frameDelta = change;
					}
					else if (delta < -change)
					{
						frameDelta = -change;
					}
					else
					{
						frameDelta = delta;
					}
				}
				else
				{
					// This is a little strange, this could lead to framerate based speeds...
					frameDelta = elev->speed;
				}
			}
		}

		fixed16_16 newValue;
		if (updateFunc)
		{
			newValue = updateFunc(elev, frameDelta);
		}
		else
		{
			// If there is no update function, just increment the value based and move on.
			*elev->value += frameDelta;
			newValue = *elev->value;
		}

		// Returns -1 if the elevator has reached the next stop, otherwise returns 0.
		return (v1 == newValue && elev->nextStop) ? -1 : 0;
	}

	void infElevatorMessageInternal(InfMessageType msgType)
	{
		u32 event = s_infMsgEvent;
		InfElevator* elev = (InfElevator*)s_infMsgTarget;
		SecObject* entity = (SecObject*)s_infMsgEntity;
		RSector* sector = elev->sector;
		s32 arg1 = s_infMsgArg1;

		// TODO: Determine which flag bit 11 is.
		if (entity && (entity->typeFlags&FLAG(11)))
		{
			if (sector->flags1 & SEC_FLAGS1_NO_SMART_OBJ)
			{
				return;
			}
			else if (!(sector->flags1 & SEC_FLAGS1_SMART_OBJ) && !(elev->flags & INF_EFLAG_DOOR))
			{
				return;
			}
		}

		// Master On message.
		if (msgType == IMSG_MASTER_ON)
		{
			// Turn master on.
			elev->updateFlags |= ELEV_MASTER_ON;
			return;
		}

		// For other messages, make sure the correct key is held.
		if (event != INF_EVENT_31)
		{
			// Non-player entities cannot use this because it requires a key.
			if (entity && (entity->typeFlags & FLAG(11)) && elev->key != 0)
			{
				return;
			}

			// Does the player have the key?
			s32 key = elev->key;
			if (key == KEY_RED && !s_invRedKey)
			{
				// "You need the red key."
				sendTextMessage(6);
				playSound2D(s_needKeySoundId);
				return;
			}
			else if (key == KEY_YELLOW && !s_invYellowKey)
			{
				// "You need the yellow key."
				sendTextMessage(7);
				playSound2D(s_needKeySoundId);
				return;
			}
			else if (key == KEY_BLUE && !s_invBlueKey)
			{
				// "You need the blue key."
				sendTextMessage(8);
				playSound2D(s_needKeySoundId);
				return;
			}
		}

		// Other messages.
		switch (msgType)
		{
			case IMSG_TRIGGER:
			{
				switch (elev->trigMove)
				{
					case TRIGMOVE_CONT:
					{
						if (!(elev->updateFlags & ELEV_MOVING))
						{
							// If the elevator is not already at the next stop, play the start sound.
							if (*elev->value != elev->nextStop->value)
							{
								// Get the sound location.
								vec3_fixed pos = inf_getElevSoundPos(elev);
								playSound3D_oneshot(elev->sound0, pos);
							}

							// Update the next time so the elevator will move on the next update.
							elev->nextTick = s_curTick;
							elev->updateFlags |= ELEV_MOVING;
						}
					} break;
					case TRIGMOVE_LAST:
					{
						// Goto the last stop.
						elev->nextStop = inf_advanceStops(elev->stops, -1, 0);
						if (*elev->value != elev->nextStop->value)
						{
							// Get the sound location.
							vec3_fixed pos = inf_getElevSoundPos(elev);
							playSound3D_oneshot(elev->sound0, pos);
						}
						// Update the next time so the elevator will move on the next update.
						elev->nextTick = s_curTick;
						elev->updateFlags |= ELEV_MOVING;
					} break;
					case TRIGMOVE_PREV:
					{
						if (elev->nextTick < s_curTick)
						{
							elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
						}
						elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
						if (!(elev->updateFlags & ELEV_MOVING))
						{
							if (*elev->value != elev->nextStop->value)
							{
								vec3_fixed pos = inf_getElevSoundPos(elev);
								playSound3D_oneshot(elev->sound0, pos);
							}
							elev->nextTick = s_curTick;
							elev->updateFlags |= ELEV_MOVING;
						}
					} break;
					case TRIGMOVE_NEXT:
					default:
					{
						if (elev->nextTick < s_curTick)
						{
							elev->nextStop = inf_advanceStops(elev->stops, 0, 1);
						}
						if (!(elev->updateFlags & ELEV_MOVING))
						{
							if (*elev->value != elev->nextStop->value)
							{
								vec3_fixed pos = inf_getElevSoundPos(elev);
								playSound3D_oneshot(elev->sound0, pos);
							}
							elev->nextTick = s_curTick;
							elev->updateFlags |= ELEV_MOVING;
						}
					}
				}

				// Handle the explosion event.
				if (event == INF_EVENT_EXPLOSION)
				{
					RSector* sector = elev->sector;
					RWall* wall = sector->walls;
					for (s32 i = 0; i < sector->wallCount; i++, wall++)
					{
						wall->flags1 &= ~(WF1_HIDE_ON_MAP | WF1_SHOW_NORMAL_ON_MAP);
					}
				}
			} break;
			case IMSG_NEXT_STOP:
			{
				// This will not fire if the elevator is in the HOLD or delay state.
				// This is because nextStop should already be set in that case, so firing it again
				// will cause the elevator to skip a stop.
				if (elev->nextTick <= s_curTick)
				{
					elev->nextStop = inf_advanceStops(elev->stops, 0, 1);
				}
				// Play the start sound and update the flags / next update time.
				if (!(elev->updateFlags & ELEV_MOVING))
				{
					if (*elev->value != elev->nextStop->value)
					{
						vec3_fixed pos = inf_getElevSoundPos(elev);
						playSound3D_oneshot(elev->sound0, pos);
					}
					elev->nextTick = s_curTick;
					elev->updateFlags |= ELEV_MOVING;
				}
			} break;
			case IMSG_PREV_STOP:
			{
				// If the elevator is in the HOLD state (i.e. the next time is in the future), go ahead and move back an additional time.
				// Why? Since nextStop is the next stop, this sets nextStop to the *current* stop.
				if (elev->nextTick > s_curTick)
				{
					// What this does is move the nextStop back to the "current" stop, so when calling inf_advanceStops() a second time we don't wind up 
					// back where we started.
					elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
				}
				// Back to the previous stop (see the note above on why this can happen twice).
				elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
				// Play the start sound and update the flags / next update time.
				if (!(elev->updateFlags & ELEV_MOVING))
				{
					if (*elev->value != elev->nextStop->value)
					{
						vec3_fixed pos = inf_getElevSoundPos(elev);
						playSound3D_oneshot(elev->sound0, pos);
					}
					elev->updateFlags |= ELEV_MOVING;
					elev->nextTick = s_curTick;
				}
			} break;
			case IMSG_GOTO_STOP:
			{
				Stop* stop = inf_advanceStops(elev->stops, arg1, 0);
				if (stop->value != *elev->value)
				{
					elev->nextStop = stop;
					vec3_fixed pos = inf_getElevSoundPos(elev);

					// In this case the original code will play the start sound if the elevator isn't moving
					// and then get it moving...
					if (!(elev->updateFlags & ELEV_MOVING))
					{
						playSound3D_oneshot(elev->sound0, pos);
						elev->updateFlags |= ELEV_MOVING;
						elev->nextTick = s_curTick;
					}
					// ... and then stop which - which always happens because updateFlags is or will be set to moving
					// and then stops the looping sound if it is playing and then play the stop sound.
					// So in some cases you will get the start sound and the stop sound and then the start sound again slightly later.
					// This seems like buggy behavior... but I'm going to leave it alone for now.
					if (elev->updateFlags & ELEV_MOVING)
					{
						stopSound(elev->soundSource1);
						elev->soundSource1 = 0;

						playSound3D_oneshot(elev->sound2, pos);
						elev->updateFlags &= ~ELEV_MOVING;
					}
				}
			} break;
			case IMSG_REVERSE_MOVE:
			{
				RSector* sector = elev->sector;
				if (!(sector->flags1 & SEC_FLAGS1_CRUSHING))
				{
					if (elev->trigMove <= TRIGMOVE_CONT)
					{
						// This will go to the previous stop.
						elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
						elev->updateFlags |= ELEV_MOVING_REVERSE;
					}
					else  // TRIGMOVE_LAST or TRIGMOVE_NEXT or TRIGMOVE_PREV
					{
						// This will get the last stop.
						elev->nextStop = inf_advanceStops(elev->stops, -1, 0);
						elev->updateFlags |= ELEV_MOVING_REVERSE;
					}
					elev->nextTick = 0;
				}
			} break;
			case IMSG_MASTER_OFF:
			{
				// Disable the elevator.
				elev->updateFlags &= ~ELEV_MASTER_ON;

				// Handle the case when the elevator is moving and needs to stop.
				if (elev->updateFlags & ELEV_MOVING)
				{
					// Get the sound position.
					vec3_fixed pos = inf_getElevSoundPos(elev);

					// Stop the looping sound and then play the stopping sound.
					stopSound(elev->soundSource1);
					elev->soundSource1 = 0;
					// Play the stop one shot.
					playSound3D_oneshot(elev->sound2, pos);

					// Remove the "moving" flag.
					elev->updateFlags &= ~ELEV_MOVING;
				}
			} break;
			case IMSG_COMPLETE:
			{
				// Fill in the goal specified by 'arg1'
				s_goals[arg1] = 0xffffffff;
				// Move the elevator to the stop specified by 'arg1' if it is NOT holding.
				if (elev->nextTick < s_curTick)
				{
					elev->nextStop = inf_advanceStops(elev->stops, arg1, 0);
				}
				// Play the sound effect, update the flags, update the next update time if NOT moving.
				if (!(elev->updateFlags & ELEV_MOVING))
				{
					if (*elev->value != elev->nextStop->value)
					{
						vec3_fixed pos = inf_getElevSoundPos(elev);
						playSound3D_oneshot(elev->sound0, pos);
					}
					elev->nextTick = s_curTick;
					elev->updateFlags |= ELEV_MOVING;
				}
			} break;
		}
	}

	void infElevatorMsgFunc(InfMessageType msgType)
	{
		if (msgType == IMSG_FREE)
		{
			deleteElevator((InfElevator*)s_infMsgTarget);
			return;
		}
		infElevatorMessageInternal(msgType);
	}
		
	void infTriggerMsgFunc(InfMessageType msgType)
	{
		InfTrigger* trigger = (InfTrigger*)s_infMsgTarget;
		switch (msgType)
		{
			case IMSG_FREE:
			{
				deleteTrigger(trigger);
				// In the original code, it this was the last trigger the "task" would be deallocated.
				// For TFE, this is just a callback so that is no longer necessary.
			} break;
			case IMSG_TRIGGER:
			{
				if (trigger->master)
				{
					// Play trigger sound.
					playSound2D(trigger->soundId);

					// Trigger targets (clients).
					TriggerTarget* target = (TriggerTarget*)allocator_getHead(trigger->targets);
					u32 event = s_infMsgEvent;
					while (target)
					{
						if (target->eventMask & event)
						{
							s_infMsgArg1 = trigger->arg0;
							s_infMsgArg2 = trigger->arg1;

							if (target->wall)
							{
								inf_wallSendMessage(target->wall, 0, trigger->event, InfMessageType(trigger->cmd));
							}
							else if (target->sector)
							{
								inf_sectorSendMessage(target->sector, nullptr, trigger->event, InfMessageType(trigger->cmd));
							}
							else  // the target is a trigger, recursively call the msg func.
							{
								infTriggerMsgFunc(trigger->cmd);
							}
						}
						target = (TriggerTarget*)allocator_getNext(trigger->targets);
					}
				}

				// Send "TEXT" message.
				if (trigger->textId)
				{
					sendTextMessage(trigger->textId);
				}

				// Update the trigger state (including switch textures).
				TriggerType type = trigger->type;
				switch (type)
				{
					case ITRIGGER_TOGGLE:
					{
						AnimatedTexture* animTex = trigger->animTex;
						if (animTex)
						{
							trigger->state++;
							if (trigger->state >= animTex->count)
							{
								trigger->state = 0;
							}

							s32 state = trigger->state;
							trigger->tex = animTex->frameList[state];
						}
					} break;
					case ITRIGGER_SINGLE:
					{
						AnimatedTexture* animTex = trigger->animTex;
						if (animTex)
						{
							if (animTex->count)
							{
								trigger->state = 1;
							}
							s32 state = trigger->state;
							trigger->tex = animTex->frameList[state];
							// Single can only be triggered once.
							trigger->master = 0;
						}
					} break;
					case ITRIGGER_SWITCH1:
					{
						AnimatedTexture* animTex = trigger->animTex;
						if (animTex)
						{
							if (animTex->count)
							{
								trigger->state = 1;
							}
							s32 state = trigger->state;
							trigger->tex = animTex->frameList[state];
						}
						// Switch1 can only be triggered once until it recieves the "DONE" message.
						trigger->master = 0;
					} break;
				}
			} break;  // case IMSG_TRIGGER
			case IMSG_MASTER_OFF:
			{
				trigger->master = 0;
			} break;
			case IMSG_DONE:
			{
				if (trigger->type == ITRIGGER_SWITCH1)
				{
					AnimatedTexture* animTex = trigger->animTex;
					// If the trigger is still in its original state, nothing to do.
					if (animTex)
					{
						trigger->state = 0;
						trigger->tex = animTex->frameList[0];
					}
					// reactive the trigger.
					trigger->master = -1;
				}
			} break;
			case IMSG_MASTER_ON:
			{
				trigger->master = -1;
			} break;
		}
	}

	void elevHandleStopDelay(InfElevator* elev)
	{
		Stop* nextStop = elev->nextStop;
		if ((elev->updateFlags & ELEV_MOVING) && nextStop->delay)
		{
			// Get the position to play the stop sound.
			vec3_fixed pos = inf_getElevSoundPos(elev);

			// Stop the looping middle sound and then play the stop sound.
			stopSound(elev->soundSource1);
			elev->soundSource1 = 0;
			// Play the stop one shot.
			playSound3D_oneshot(elev->sound2, pos);
		}
		// If there is a delay, then the elevator is not moving.
		if (nextStop->delay)
		{
			elev->updateFlags &= ~ELEV_MOVING;
		}
		else
		{
			elev->updateFlags |= ELEV_MOVING;
		}
	}

	Stop* inf_advanceStops(Allocator* stops, s32 absoluteStop, s32 relativeStop)
	{
		Stop* stop = nullptr;
		// advance to a future stop
		if (relativeStop > 0)
		{
			for (; relativeStop > 0; relativeStop--)
			{
				stop = (Stop*)allocator_getNext(stops);
				// Loop around.
				if (!stop)
				{
					stop = (Stop*)allocator_getHead(stops);
				}
			}
		}
		// advance to a previous stop
		else if (relativeStop < 0)
		{
			relativeStop = -relativeStop;
			for (; relativeStop > 0; relativeStop--)
			{
				stop = (Stop*)allocator_getPrev(stops);
				// Loop around.
				if (!stop)
				{
					stop = (Stop*)allocator_getTail(stops);
				}
			}
		}
		// relativeStop == 0
		else if (absoluteStop >= 0)
		{
			// Loop from head instead of the current stop.
			stop = (Stop*)allocator_getHead(stops);
			for (; absoluteStop > 0; absoluteStop--)
			{
				stop = (Stop*)allocator_getNext(stops);
				if (!stop)
				{
					stop = (Stop*)allocator_getHead(stops);
				}
			}
		}
		// relativeStop == 0 && absoluteStop < 0 -- this basically will send us to the last stop.
		else
		{
			stop = (Stop*)allocator_getTail_noIterUpdate(stops);
		}

		return stop;
	}

	void inf_adjustTextureWallOffsets_Floor(RSector* sector, s32 floorDelta)
	{
	#if 0	// This should be moved to the sector implementation
		RWall* wall = sector->walls;
		s32 wallCount = sector->wallCount;
		for (s32 i = 0; i < wallCount; i++, wall++)
		{
			s32 textureOffset = floorDelta << 3;
			if (wall->flags1 & WF1_TEX_ANCHORED)
			{
				if (wall->nextSector)
				{
					wall->botVOffset -= textureOffset;
				}
				else
				{
					wall->midVOffset -= textureOffset;
				}
			}
			if (wall->flags1 & WF1_SIGN_ANCHORED)
			{
				wall->signVOffset -= textureOffset;
			}
		}
	#endif
	}

	void inf_adjustTextureMirrorOffsets_Floor(RSector* sector, s32 floorDelta)
	{
	#if 0	// This should be moved to the sector implementation
		RWall* wall = sector->walls;
		s32 wallCount = sector->wallCount;
		for (s32 i = 0; i < wallCount; i++, wall++)
		{
			RWall* mirror = wall->mirrorWall;
			if (mirror)
			{
				s32 textureOffset = -floorDelta * 8;
				if (mirror->flags1 & WF1_TEX_ANCHORED)
				{
					if (mirror->nextSector)
					{
						mirror->botVOffset -= textureOffset;
					}
					else
					{
						mirror->midVOffset -= textureOffset;
					}
				}
				if (mirror->flags1 & WF1_SIGN_ANCHORED)
				{
					mirror->signVOffset -= textureOffset;
				}
			}
		}
	#endif
	}

	void deleteElevator(InfElevator* elev)
	{
		if (elev->slaves)
		{
			allocator_free(elev->slaves);
		}
		// Free the stops.
		if (elev->stops)
		{
			Stop* stop = (Stop*)allocator_getHead(elev->stops);
			while (stop)
			{
				if (stop->messages)
				{
					allocator_free(stop->messages);
				}
				if (stop->adjoinCmds)
				{
					allocator_free(stop->adjoinCmds);
				}
				stop = (Stop*)allocator_getNext(elev->stops);
			}
			allocator_free(elev->stops);
		}
		sector_deleteElevatorLink(elev->sector, elev);
		allocator_deleteItem(s_infElevators, elev);
	}

	void deleteTrigger(InfTrigger* trigger)
	{
		InfLink* link = trigger->link;
		allocator_deleteItem(link->parent, link);
		allocator_free(trigger->targets);

		// TODO: what is trigger->u48?
		if (trigger->u48)
		{
			free(trigger->u48);
		}
		free(trigger);

		s_triggerCount--;
	}

	void inf_stopAdjoinCommands(Stop* stop)
	{
		Allocator* adjoinCmds = stop->adjoinCmds;
		if (adjoinCmds)
		{
			AdjoinCmd* cmd = (AdjoinCmd*)allocator_getHead(adjoinCmds);
			while (cmd)
			{
				RWall* wall0 = cmd->wall0;
				RWall* wall1 = cmd->wall1;
				RSector* sector0 = cmd->sector0;
				RSector* sector1 = cmd->sector1;

				wall0->nextSector = sector1;
				wall0->mirrorWall = wall1;

				wall1->nextSector = sector0;
				wall1->mirrorWall = wall0;

				sector_setupWallDrawFlags(sector0);
				sector_setupWallDrawFlags(sector1);

				cmd = (AdjoinCmd*)allocator_getNext(adjoinCmds);
			}
		}
	}

	void inf_sendObjMessage(SecObject* obj, s32 a, s32 msgType)
	{
		// TODO
	}

	void inf_sendSectorMessageInternal(RSector* sector, InfMessageType msgType)
	{
		switch (msgType)
		{
			case IMSG_WAKEUP:
			{
				s32 objCount = sector->objectCount;
				SecObject** objList = sector->objectList;

				for (s32 i = 0; i < objCount; objList++)
				{
					SecObject* obj = *objList;
					if (obj)
					{
						if (obj->typeFlags & 0x40)
						{
							inf_sendObjMessage(obj, 0, IMSG_WAKEUP);
						}
						i++;
					}
				}
			}
			// IMSG_WAKEUP drops through to IMSG_MASTER_ON/IMSG_MASTER_OFF
			case IMSG_MASTER_ON:
			case IMSG_MASTER_OFF:
			{
				s32 objCount = sector->objectCount;
				SecObject** objList = sector->objectList;

				for (s32 i = 0; i < objCount; objList++)
				{
					SecObject* obj = *objList;
					if (obj)
					{
						if (obj->typeFlags & 0x400)
						{
							inf_sendObjMessage(obj, 0, msgType);
						}
						i++;
					}
				}
			} break;
			case IMSG_SET_BITS:
			{
				s32 flagsIndex = s_infMsgArg1;
				u32 bits = s_infMsgArg2;

				if (flagsIndex == 1)
				{
					sector->flags1 |= bits;
				}
				else if (flagsIndex == 2)
				{
					sector->flags2 |= bits;
				}
				else if (flagsIndex == 3)
				{
					sector->flags3 |= bits;
				}
			} break;
			case IMSG_CLEAR_BITS:
			{
				s32 flagsIndex = s_infMsgArg1;
				u32 bits = s_infMsgArg2;

				if (flagsIndex == 1)
				{
					sector->flags1 &= ~bits;
				}
				else if (flagsIndex == 2)
				{
					sector->flags2 &= ~bits;
				}
				else if (flagsIndex == 3)
				{
					sector->flags3 &= ~bits;
				}
			} break;
		};
	}

	void inf_stopHandleMessages(Stop* stop)
	{
		Allocator* msgList = stop->messages;
		if (!msgList) { return; }

		InfMessage* msg = (InfMessage*)allocator_getHead(msgList);
		while (msg)
		{
			s_infMsgArg1 = msg->arg1;
			s_infMsgArg2 = msg->arg2;
			RWall* wall = msg->wall;
			if (wall)
			{
				inf_wallSendMessage(wall, 0, msg->event, msg->msgType);
			}
			else if (msg->sector)
			{
				RSector* sector = msg->sector;
				Allocator* infLink = sector->infLink;
				if (infLink)
				{
					InfLink* link = (InfLink*)allocator_getHead(infLink);
					while (link)
					{
						if (link->msgFunc && (msg->event <= 0 || (link->eventMask & msg->event)))
						{
							allocator_addRef(msgList);

							s_infMsgTarget = link->target;
							s_infMsgEntity = nullptr;
							s_infMsgArg1 = msg->arg1;
							s_infMsgArg2 = msg->arg2;
							s_infMsgEvent = msg->event;
							inf_sendSectorMessageInternal(sector, msg->msgType);

							link->msgFunc(msg->msgType);
							allocator_release(msgList);
						}
						
						link = (InfLink*)allocator_getNext(infLink);
					}
				}
				else  // infLink
				{
					inf_sectorSendMessage(sector, nullptr, msg->event, msg->msgType);
				}
			}
			else  // world
			{
				// In the original game, this will call a specific function based on the type
				// But the only type is IMSG_LIGHTS
				if (msg->msgType == IMSG_LIGHTS)
				{
					inf_handleMsgLights();
				}
			}

			msg = (InfMessage*)allocator_getNext(msgList);
		} // while (msg)
	}
		
	void inf_handleMsgLights()
	{
		RSector* sector = s_sectors;
		for (u32 i = 0; i < s_sectorCount; i++, sector++)
		{
			fixed16_16 newAmbient = intToFixed16(sector->flags3);
			// Store the old value in flags3 so the lights can be toggled.
			sector->flags3 = floor16(sector->ambient.f16_16);
			sector->ambient.f16_16 = newAmbient;
		}
	}

	vec3_fixed inf_getElevSoundPos(InfElevator* elev)
	{
		vec3_fixed pos;
		RSector* sector = elev->sector;

		// Figure out the source position for the sound effect.
		pos.y = sector->secHeight.f16_16 + sector->floorHeight.f16_16;
		if (elev->type != IELEV_ROTATE_WALL)
		{
			// First vertex position.
			vec2_fixed* vtx = (vec2_fixed*)sector->verticesWS;
			pos.x = vtx->x;
			pos.z = vtx->z;
		}
		else
		{
			// Rotation center.
			pos.x = elev->dirOrCenter.x;
			pos.z = elev->dirOrCenter.z;
		}

		return pos;
	}

	///////////////////////////////////////////
	// Trigger creation and setup
	///////////////////////////////////////////
	void inf_triggerFreeFunc(void* data)
	{
		// TODO
	}

	// Creates a trigger and adds a link to the sector or wall where it is located.
	// For example, if a player "nudges" a wall, then all of the wall's links will be
	// cycled through, calling the "msgFunc" on each to determine what happens, which than
	// sends the message to the appropriate "InfTrigger" or "InfElevator"
	InfTrigger* inf_createTrigger(TriggerType type, InfTriggerObject obj)
	{
		// TODO: Change to the zone allocator.
		InfTrigger* trigger = (InfTrigger*)malloc(sizeof(InfTrigger));
		s_triggerCount++;

		InfLink* link = nullptr;
		trigger->soundId = 0;
		trigger->targets = allocator_create(sizeof(TriggerTarget));

		switch (type)
		{
			case ITRIGGER_WALL:
			{
				RWall* wall = obj.wall;
				if (!wall->infLink)
				{
					wall->infLink = allocator_create(sizeof(InfLink));
				}
				link = (InfLink*)allocator_newItem(wall->infLink);
				link->type = LTYPE_TRIGGER;
				link->entityMask = INF_ENTITY_PLAYER;
				link->eventMask = INF_EVENT_ANY;
				link->trigger = trigger;
				link->msgFunc = infTriggerMsgFunc;
				link->parent = wall->infLink;
			} break;
			case ITRIGGER_SECTOR:
			{
				RSector* sector = obj.sector;
				if (!sector->infLink)
				{
					sector->infLink = allocator_create(sizeof(InfLink));
				}
				link = (InfLink*)allocator_newItem(sector->infLink);
				link->type = LTYPE_TRIGGER;
				link->entityMask = INF_ENTITY_PLAYER;
				link->eventMask = INF_EVENT_ANY;
				link->trigger = trigger;
				link->msgFunc = infTriggerMsgFunc;
				link->parent = sector->infLink;
			} break;
			case ITRIGGER_SWITCH1:
			case ITRIGGER_TOGGLE:
			case ITRIGGER_SINGLE:
			{
				RWall* wall = obj.wall;
				trigger->soundId = s_switchDefaultSndId;
				if (!wall->infLink)
				{
					wall->infLink = allocator_create(sizeof(InfLink));
				}
				InfLink* link = (InfLink*)allocator_newItem(wall->infLink);
				link->type = LTYPE_TRIGGER;
				link->entityMask = INF_ENTITY_PLAYER;
				link->eventMask = INF_EVENT_ANY;
				link->msgFunc = infTriggerMsgFunc;
				link->trigger = trigger;
				link->parent = wall->infLink;
				trigger->animTex = nullptr;

				// Handle trigger sign texture.
				// TODO: Update wall textures to use pointer to pointer and deference as needed to make animated textures work.
				TextureData** signTex = (TextureData**)wall->signTex;
				if (signTex && (*signTex)->uvWidth == -2)
				{
					// In this case, image points to the AnimatedTexture rather than the actual image.
					AnimatedTexture* animTex = (AnimatedTexture*)(*signTex)->image;
					trigger->animTex = animTex;
					trigger->tex = animTex->frameList[0];
					wall->signTex = &trigger->tex;
					// Removes "cross line" events
					link->eventMask &= ~(INF_EVENT_CROSS_LINE_FRONT | INF_EVENT_CROSS_LINE_BACK);
				}
			} break;
		}
		trigger->cmd = IMSG_DONE;
		trigger->event = 0;
		trigger->arg1 = 0;
		trigger->u30 = 0xffffffff;
		trigger->u34 = 21;
		trigger->master = 0xffffffff;
		trigger->state = 0;
		trigger->u48 = nullptr;
		trigger->textId = 0;
		trigger->link = link;
		trigger->type = type;
		link->freeFunc = inf_triggerFreeFunc;

		return trigger;
	}


	///////////////////////////////////////////////////
	// Elevator Update Functions
	///////////////////////////////////////////////////
	fixed16_16 infUpdate_moveHeights(InfElevator* elev, fixed16_16 delta)
	{
		RSector* sector = elev->sector;
		fixed16_16 secHeightDelta = 0;
		fixed16_16 ceilDelta = 0;
		fixed16_16 floorDelta = 0;

		switch (elev->type)
		{
		case IELEV_MOVE_CEILING:
			ceilDelta = delta;
			break;
		case IELEV_MOVE_FLOOR:
			floorDelta = delta;
			break;
		case IELEV_MOVE_OFFSET:
			secHeightDelta = delta;
			break;
		case IELEV_MOVE_FC:
			floorDelta = delta;
			ceilDelta = delta;
			break;
		}

		fixed16_16 maxObjHeight = sector_getMaxObjectHeight(sector);
		if (maxObjHeight)
		{
			maxObjHeight += 0x4000;	// maxObjHeight + 0.25
			if (secHeightDelta)
			{
				fixed16_16 height = sector->floorHeight.f16_16 + sector->secHeight.f16_16;
				fixed16_16 maxObjHeightAbove = 0;
				fixed16_16 maxObjectHeightBelow = 0;
				s32 objCount = sector->objectCount;
				SecObject** objList = sector->objectList;
				for (; objCount > 0; objList++)
				{
					SecObject* obj = *objList;
					if (!obj)
					{
						continue;
					}

					objCount--;
					fixed16_16 objHeight = obj->worldHeight + ONE_16;
					// Object is below the second height
					if (obj->posWS.y.f16_16 > height)
					{
						if (objHeight > maxObjectHeightBelow)
						{
							maxObjectHeightBelow = objHeight;
						}
					}
					// Object is on or above the second height
					else
					{
						if (objHeight > maxObjHeightAbove)
						{
							maxObjHeightAbove = objHeight;
						}
					}
				}

				// The new second height after adjustment (only second height so far).
				height += secHeightDelta;
				// Difference between the base floor height and the adjusted second height.
				fixed16_16 floorDelta = sector->floorHeight.f16_16 - (height + ONE_16);
				// Difference betwen the adjusted second height and the ceiling.
				fixed16_16 ceilDelta = height - sector->ceilingHeight.f16_16;
				if (floorDelta < maxObjectHeightBelow || ceilDelta < maxObjHeightAbove)
				{
					// If there are objects in the way, set the next stop as the previous.
					elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
					floorDelta = 0;
					secHeightDelta = 0;
					ceilDelta = 0;
				}
			}
			else
			{
				fixed16_16 floorHeight = sector->floorHeight.f16_16 + floorDelta;
				fixed16_16 ceilHeight = sector->ceilingHeight.f16_16 + ceilDelta;
				fixed16_16 height = floorHeight - ceilHeight;
				if (height < maxObjHeight)
				{
					// Not sure why it needs to check again...
					fixed16_16 maxObjHeight = sector_getMaxObjectHeight(sector);
					fixed16_16 spaceNeeded = maxObjHeight + 0x4000;	// maxObjHeight + 0.25
					floorHeight = sector->floorHeight.f16_16 + floorDelta;
					ceilHeight = sector->ceilingHeight.f16_16 + ceilDelta;
					fixed16_16 spaceAvail = floorHeight - ceilHeight;

					// If the height between floor and ceiling is too small for the tallest object AND
					// If the floor is moving up or the ceiling is moving down and this is NOT a crushing sector.
					if (maxObjHeight > 0 && spaceAvail < spaceNeeded && (floorDelta < 0 || ceilDelta > 0) && !(sector->flags1 & SEC_FLAGS1_CRUSHING))
					{
						Stop* stop = inf_advanceStops(elev->stops, 0, -1);
						floorDelta = 0;
						secHeightDelta = 0;
						ceilDelta = 0;
						elev->nextStop = stop;
					}
				}
			}
		}

		if (floorDelta)
		{
			inf_adjustTextureWallOffsets_Floor(sector, floorDelta);
			inf_adjustTextureMirrorOffsets_Floor(sector, floorDelta);
		}
		sector_adjustHeights(sector, floorDelta, ceilDelta, secHeightDelta);

		// Apply adjustments to "slave" sectors.
		Slave* child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			if (floorDelta)
			{
				inf_adjustTextureWallOffsets_Floor(child->sector, floorDelta);
				inf_adjustTextureMirrorOffsets_Floor(child->sector, floorDelta);
			}
			sector_adjustHeights(child->sector, floorDelta, ceilDelta, secHeightDelta);
			child = (Slave*)allocator_getNext(elev->slaves);
		}
		return *elev->value;
	}

	fixed16_16 infUpdate_moveWall(InfElevator* elev, fixed16_16 delta)
	{
		// First attempt to move walls in the sector.
		if (!sector_moveWalls(elev->sector, delta, elev->dirOrCenter.x, elev->dirOrCenter.z, elev->flags))
		{
			return elev->iValue;
		}

		// If successful move its slaves.
		Slave* child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			sector_moveWalls(child->sector, delta, elev->dirOrCenter.x, elev->dirOrCenter.z, elev->flags);
			child = (Slave*)allocator_getNext(elev->slaves);
		}

		// Finally update the elevator value.
		elev->iValue += delta;
		return elev->iValue;
	}

	fixed16_16 infUpdate_rotateWall(InfElevator* elev, fixed16_16 delta)
	{
		// TODO
		return 0;
	}

	fixed16_16 infUpdate_scrollWall(InfElevator* elev, fixed16_16 delta)
	{
		// TODO
		return 0;
	}

	fixed16_16 infUpdate_scrollFlat(InfElevator* elev, fixed16_16 delta)
	{
		// TODO
		return 0;
	}

	fixed16_16 infUpdate_changeAmbient(InfElevator* elev, fixed16_16 delta)
	{
		RSector* sector = elev->sector;
		sector->ambient.f16_16 += delta;

		Slave* child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			child->sector->ambient.f16_16 += delta;
			child = (Slave*)allocator_getNext(elev->slaves);
		}
		return sector->ambient.f16_16;
	}

	fixed16_16 infUpdate_changeWallLight(InfElevator* elev, fixed16_16 delta)
	{
		// TODO
		return 0;
	}

	////////////////////////////////////////////////////
	// Sector stuff (move me)
	////////////////////////////////////////////////////
	// returns 0 if the object does NOT overlap, otherwise non-zero.
	// objSide: 0 = no overlap, -1/+1 front or behind.
	u32 sector_objOverlapsWall(RWall* wall, SecObject* obj, s32* objSide)
	{
		s32 halfWidth = (obj->worldWidth - SIGN_BIT(obj->worldWidth)) >> 1;
		s32 quarterWidth = (obj->worldWidth - SIGN_BIT(obj->worldWidth)) >> 2;
		s32 threeQuartWidth = halfWidth + quarterWidth;
		*objSide = 0;

		RSector* next = wall->nextSector;
		if (next)
		{
			RSector* sector = wall->sector;
			if (obj->posWS.y.f16_16 <= sector->floorHeight.f16_16)
			{
				fixed16_16 objTop = obj->posWS.y.f16_16 - obj->worldHeight;
				if (objTop > sector->ceilingHeight.f16_16)
				{
					return 0;
				}
			}
		}

		vec2* w0 = wall->w0;
		fixed16_16 x0 = w0->x.f16_16;
		fixed16_16 dirX = wall->wallDirZ.f16_16;
		fixed16_16 z0 = w0->z.f16_16;
		fixed16_16 dirZ = wall->wallDirX.f16_16;
		fixed16_16 len = wall->length.f16_16;
		fixed16_16 dx = obj->posWS.x.f16_16 - x0;
		fixed16_16 dz = obj->posWS.z.f16_16 - z0;

		fixed16_16 proj = mul16(dx, dirZ) + mul16(dz, dirX);
		fixed16_16 maxS = threeQuartWidth * 2 + len;	// 1.5 * width + length
		fixed16_16 s = threeQuartWidth + proj;
		if (s <= maxS)
		{
			s32 diff = mul16(dx, dirX) - mul16(dz, dirZ);
			s32 side = (diff >= 0) ? 1 : -1;

			*objSide = side;
			if (side < 0)
			{
				diff = -diff;
			}
			if (diff <= threeQuartWidth)
			{
				return 0xffffffff;
			}
		}
		return 0;
	}

	static fixed16_16 s_dist;

	fixed16_16 sector_computeDirAndLength(fixed16_16 dx, fixed16_16 dz, fixed16_16* dirX, fixed16_16* dirZ)
	{
		// The original code reduce the values by half, did the sqaure root and than multiplied by 2 (which the sqrt table adjusted for the scale).
		// TFE just calls the sqrt function directly.
		fixed16_16 dxSq = mul16(dx, dx);
		fixed16_16 dzSq = mul16(dz, dz);
		s_dist = TFE_JediRenderer::fixedSqrt(dxSq + dzSq);
		
		if (s_dist)
		{
			*dirX = div16(dx, s_dist);
			*dirZ = div16(dz, s_dist);
		}
		else
		{
			*dirZ = 0;
			*dirX = 0;
		}
		return s_dist;
	}

	void sector_computeWallDirAndLength(RWall* wall)
	{
		vec2* w0 = wall->w0;
		vec2* w1 = wall->w1;
		fixed16_16 dx = w1->x.f16_16 - w0->x.f16_16;
		fixed16_16 dz = w1->z.f16_16 - w0->z.f16_16;
		wall->length.f16_16 = sector_computeDirAndLength(dx, dz, &wall->wallDirX.f16_16, &wall->wallDirZ.f16_16);
		wall->angle = TFE_JediRenderer::vec2ToAngle(dx, dz);
	}

	void sector_moveWallVertex(RWall* wall, fixed16_16 offsetX, fixed16_16 offsetZ)
	{
		// Offset vertex 0.
		wall->w0->x.f16_16 += offsetX;
		wall->w0->z.f16_16 += offsetZ;
		// Update the wall direction and length.
		sector_computeWallDirAndLength(wall);

		// Set the appriate game value if the player is inside the sector.
		RSector* sector = wall->sector;
		if (sector->flags1 & SEC_FLAGS1_PLAYER)
		{
			s_playerSecMoved = 0xffffffff;
		}

		// Update the previous wall, since it would have changed as well.
		RWall* nextWall = nullptr;
		if (wall->id == 0)
		{
			s32 last = sector->wallCount - 1;
			nextWall = &sector->walls[last];
		}
		else
		{
			nextWall = wall - 1;
		}
		// Compute the wall direction and length of the previous wall.
		sector_computeWallDirAndLength(nextWall);
	}

	// returns 0 if the wall is free to move, else non-zero.
	u32 sector_canWallMove(RWall* wall, fixed16_16 offsetX, fixed16_16 offsetZ)
	{
		s32 objSide0;
		// Test the initial position, the code assumes there is no collision at this point.
		sector_objOverlapsWall(wall, s_playerObject, &objSide0);

		vec2* w0 = wall->w0;
		fixed16_16 z0 = w0->z.f16_16;
		fixed16_16 x0 = w0->x.f16_16;
		w0->x.f16_16 += offsetX;
		w0->z.f16_16 += offsetZ;

		s32 objSide1;
		// Then move the wall and test the new position.
		s32 col = sector_objOverlapsWall(wall, s_playerObject, &objSide1);

		// Restore the wall.
		w0->x.f16_16 = x0;
		w0->z.f16_16 = z0;

		if (!col)
		{
			if (objSide0 == 0 || objSide1 == 0 || objSide0 == objSide1)
			{
				return 0;
			}
		}
		return 0xffffffff;
	}

	void sector_moveObjects(RSector* sector, u32 flags, fixed16_16 offsetX, fixed16_16 offsetZ)
	{
		// TODO
	}

	void sector_computeBounds(RSector* sector)
	{
		// TODO
	}

	u32 sector_moveWalls(RSector* sector, fixed16_16 delta, fixed16_16 dirX, fixed16_16 dirZ, u32 flags)
	{
		fixed16_16 offsetX = mul16(delta, dirX);
		fixed16_16 offsetZ = mul16(delta, dirZ);

		u32 sectorBlocked = 0;
		s32 wallCount = sector->wallCount;
		RWall* wall = sector->walls;
		for (u32 i = 0; i < wallCount && !sectorBlocked; i++, wall++)
		{
			if (wall->flags1 & WF1_WALL_MORPHS)
			{
				sectorBlocked |= sector_canWallMove(wall, offsetX, offsetZ);

				RWall* mirror = wall->mirrorWall;
				if (wall->mirrorWall && (mirror->flags1 & WF1_WALL_MORPHS))
				{
					sectorBlocked |= sector_canWallMove(mirror, offsetX, offsetZ);
				}
			}
		}

		if (!sectorBlocked)
		{
			wall = sector->walls;
			for (u32 i = 0; i < wallCount; i++, wall++)
			{
				if (wall->flags1 & WF1_WALL_MORPHS)
				{
					sector_moveWallVertex(wall, offsetX, offsetZ);
					RWall* mirror = wall->mirrorWall;
					if (mirror && (mirror->flags1 & WF1_WALL_MORPHS))
					{
						sector_moveWallVertex(mirror, offsetX, offsetZ);
					}
				}
			}
			sector_moveObjects(sector, flags, offsetX, offsetZ);
			sector_computeBounds(sector);
		}

		return !sectorBlocked ? 0xffffffff : 0;
	}
}

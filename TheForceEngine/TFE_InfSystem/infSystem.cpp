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
#include "infTypesInternal.h"

using namespace TFE_GameConstants;
using namespace TFE_JediRenderer;
using namespace InfAllocator;
struct TextureData;

namespace TFE_InfSystem
{
	// INF delta time in ticks.
	static s32 s_curTime;
	static s32 s_deltaTime;
	static Allocator* s_infElevators;

	static s32 s_infMsgArg1;
	static s32 s_infMsgArg2;
	static u32 s_infMsgEvent;
	static void* s_infMsgTarget;
	static void* s_infMsgEntity;
	
	void deleteElevator(InfElevator* elev);
	s32 updateElevator(InfElevator* elev);
	void elevHandleStopDelay(InfElevator* elev);
	Stop* inf_advanceStops(Allocator* stops, s32 absoluteStop, s32 relativeStop);
	void inf_adjustTextureWallOffsets_Floor(RSector* sector, s32 floorDelta);
	void inf_adjustTextureMirrorOffsets_Floor(RSector* sector, s32 floorDelta);

	// TODO: Move these to the right place in the renderer or shared location.
	s32 sector_getMaxObjectHeight(RSector* sector);
	void sector_adjustHeights(RSector* sector, s32 floorDelta, s32 ceilDelta, s32 secHeightDelta);

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
			s32 dt = 0;
			if (elev->updateFlags & ELEV_MASTER_ON)
			{
				if (elev->nextTime < s_curTime)
				{
					// If not already moving, get started.
					if (!(elev->updateFlags & ELEV_MOVING) && !elevDeleted)
					{
						RSector* sector = elev->sector;

						// Figure out the source position for the sound effect.
						s32 y = sector->secHeight.f16_16 + sector->floorHeight.f16_16;
						s32 x, z;
						if (elev->type != IELEV_ROTATE_WALL)
						{
							// First vertex position.
							vec2_fixed* vtx = (vec2_fixed*)sector->verticesWS;
							x = vtx->x;
							z = vtx->z;
						}
						else
						{
							// Rotation center.
							x = elev->dirOrCenter.x;
							z = elev->dirOrCenter.z;
						}

						// Play the startup sound effect if the elevator is not already at the next stop.
						s32* value = elev->value;
						Stop* nextStop = elev->nextStop;
						// Play the initial sound as the elevator starts moving.
						if (*elev->value != elev->nextStop->value)
						{
							playSound3D_oneshot(elev->sound0, x, y, z);
						}

						// Update the next time, so this will move on the next update.
						elev->nextTime = s_curTime;

						// Flag the elevator as moving.
						elev->updateFlags |= ELEV_MOVING;
					}

					if ((elev->updateFlags & ELEV_MOVING))
					{
						RSector* sector = elev->sector;

						// Get the sound effect position.
						s32 y = sector->floorHeight.f16_16 + sector->secHeight.f16_16;
						s32 x, z;
						if (elev->type != IELEV_ROTATE_WALL)
						{
							vec2_fixed* vtx = (vec2_fixed*)sector->verticesWS;
							x = vtx->x;
							z = vtx->z;
						}
						else
						{
							x = elev->dirOrCenter.x;
							z = elev->dirOrCenter.z;
						}

						// Start up the sound effect, track it since it is looping.
						elev->soundSource1 = playSound3D_looping(elev->soundSource1, elev->sound1, x, y, z);
					}

					// if reachedStop = 0, elevator has not reached the next stop.
					s32 reachedStop = updateElevator(elev);
					if (reachedStop)
					{
						elevHandleStopDelay(elev);

						Stop* nextStop = elev->nextStop;
						if (elev->updateFlags & ELEV_MOVING_REVERSE)
						{
							// TODO
						}
						else
						{
							s32 delay = nextStop->delay;
							if (delay == IDELAY_HOLD)
							{
								elev->moveType = MOVE_STOPPED;
							}
							else if (delay == IDELAY_COMPLETE || delay == IDELAY_TERMINATE)
							{
								// delete the elevator, we're done here.
								deleteElevator(elev);
								elevDeleted = 1;
								if (delay == IDELAY_COMPLETE)
								{
									// TODO: Two additional functions handle the "complete" element
								}
							}
							else  // Timed
							{
								elev->nextTime = s_curTime + nextStop->delay;
							}
						}

						// Process stop messages if the elevator has not been deleted.
						if (!elevDeleted)
						{
							// Messages
							Allocator* msgList = nextStop->messages;
							if (msgList)
							{
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
									else  // sector
									{
										RSector* sector = msg->sector;
										if (msg->sector)
										{
											Allocator* infLink = sector->infLink;
											if (infLink)
											{
												InfLink* link = (InfLink*)allocator_getHead(infLink);
												if (link)
												{
													if (link->task && (msg->event <= 0 || (link->eventMask & msg->event)))
													{
														allocator_addRef(msgList);

														s_infMsgTarget = link->target;
														s_infMsgEntity = 0;
														s_infMsgArg1 = msg->arg1;
														s_infMsgArg2 = msg->arg2;
														s_infMsgEvent = msg->event;
														inf_sendSectorMessageInternal(sector, msg->msgType);

														link->task(msg->msgType);

														allocator_release(msgList);
													}
													else
													{
														// TODO
													}
												}
											}
											else  // (inkLink)
											{
												// TODO
											}
										}
										else  // (msg->sector)
										{
											// TODO
										}
									}
									msg = (InfMessage*)allocator_getNext(msgList);
								} // while (msg)
							} // (messages)

							// Adjoin Commands.
							Allocator* adjoinCmds = nextStop->adjoinCmds;
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

									cmd = (AdjoinCmd*)allocator_GetNext(adjoinCmds);
								}
							}

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
				} // (elev->nextTime < s_curTime)
			} // (elev->updateFlags & 2)

			// Next elevator.
			elev = (InfElevator*)allocator_getNext(s_infElevators);
		} // while (elev)
	}

	// Per frame animated texture update (this may be moved later).
	void update_animatedTextures()
	{
	}

	// Send messages so that entities and the player can interact with the INF system.
	void inf_wallSendMessage(RWall* wall, s32 entity, u32 evt, InfMessageType msgType)
	{
	}

	void inf_sectorSendMessage(RSector* sector, SecObject* obj, u32 evt, InfMessageType msgType)
	{
	}

	/////////////////////////////////////////////////////
	// Internal
	/////////////////////////////////////////////////////
	// Include update functions
	#include "infElevatorUpdateFunc.h"

	// Update an elevator.
	// Returns -1 if the elevator has reached the next stop, else 0.
	s32 updateElevator(InfElevator* elev)
	{
		s32* value = elev->value;
		Stop* nextStop = elev->nextStop;

		s32 v0 = *value;
		s32 v1 = v0;
		s32 hasStop = -1;
		if (nextStop)
		{
			v1 = nextStop->value;
			hasStop = 0;
		}

		// The elevator has reached the next stop.
		if (v1 == v0 && nextStop)
		{
			return -1;
		}

		s32 type = elev->type;
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

		s32 frameDelta = 0;
		if (!nextStop)
		{
			// If there are no stops, then the elevator keeps going forever...
			// Useful for scrolling textures, constantly spinning gears, etc.
			frameDelta = mul16(elev->speed, s_deltaTime);
		}
		else
		{
			s32 delta = v1 - v0;
			frameDelta = delta;
			if (elev->speed)
			{
				if (!elev->fixedStep)
				{
					s32 change = mul16(elev->speed, s_deltaTime);
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

		s32 newValue;
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

		// Returns -1 if the elevator has reached the next stop.
		if (v1 == newValue && elev->nextStop)
		{
			return -1;
		}
		// Otherwise return 0.
		return 0;
	}

	void elevHandleStopDelay(InfElevator* elev)
	{
		Stop* nextStop = elev->nextStop;
		if ((elev->updateFlags & ELEV_MOVING) && nextStop->delay)
		{
			RSector* sector = elev->sector;

			// Get the position to play the stop sound.
			s32 y = sector->floorHeight.f16_16 + sector->secHeight.f16_16;
			s32 x, z;
			if (elev->type != IELEV_ROTATE_WALL)
			{
				vec2_fixed* vtx = (vec2_fixed*)sector->verticesWS;
				x = vtx->x;
				z = vtx->z;
			}
			else
			{
				x = elev->dirOrCenter.x;
				z = elev->dirOrCenter.y;
			}

			// Stop the looping middle sound and then play the stop sound.
			stopSound(elev->soundSource1);
			playSound3D_oneshot(elev->sound2, x, y, z);
			elev->soundSource1 = 0;
		}
		// If there is a delay, then the elevator is not moving.
		if (nextStop->delay)
		{
			elev->updateFlags &= ~ELEV_MOVING;
		}
		else
		{
			// 1e1418:
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

	// TODO: Figure out where to handle these...
	s32 sector_getMaxObjectHeight(RSector* sector)
	{
		return 0;
	}

	void sector_adjustHeights(RSector* sector, s32 floorDelta, s32 ceilDelta, s32 secHeightDelta)
	{
	}
}

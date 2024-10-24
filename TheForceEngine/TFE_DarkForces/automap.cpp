#include "automap.h"
#include "player.h"
#include "hud.h"
#include <TFE_DarkForces/sound.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/levelData.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Renderer/screenDraw.h>
#include <TFE_Jedi/Serialization/serialization.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	enum MapWallColor
	{
		WCOLOR_INVISIBLE  = 0,
		WCOLOR_NORMAL     = 10,
		WCOLOR_LEDGE      = 12,
		WCOLOR_GRAYED_OUT = 13,
		WCOLOR_DOOR       = 19,
	};

	enum MapObjectColor
	{
		MOBJCOLOR_DEFAULT  = 19,
		MOBJCOLOR_PROJ     = 6,
		MOBJCOLOR_CORPSE   = 48,
		MOBJCOLOR_LANDMINE = 1,
		MOBJCOLOR_PICKUP   = 152,
		MOBJCOLOR_SCENERY  = 21,
	};

	enum MapConstants
	{
		MOBJSPRITE_DRAW_LEN = FIXED(2)
	};

	static fixed16_16 s_screenScale = 0xc000;	// 0.75
	static fixed16_16 s_scrLeftScaled;
	static fixed16_16 s_scrRightScaled;
	static fixed16_16 s_scrTopScaled;
	static fixed16_16 s_scrBotScaled;

	static JBool s_automapAutoCenter = JTRUE;
	static fixed16_16 s_mapXCenterInPixels = 159;
	static fixed16_16 s_mapZCenterInPixels = 99;
	
	static s32 s_mapTop;
	static s32 s_mapLeft;
	static s32 s_mapRight;
	static s32 s_mapBot;
	static s32 s_mapShowSectorMode = 0;
	static JBool s_mapShowAllLayers = JFALSE;
	
	static s32 s_mapPrevPlayerX;
	static s32 s_mapPrevPlayerZ;
	static u8* s_mapFramebuffer;
	
	JBool s_pdaActive = JFALSE;
	JBool s_drawAutomap = JFALSE;
	JBool s_automapLocked = JTRUE;
	fixed16_16 s_mapX0;
	fixed16_16 s_mapX1;
	fixed16_16 s_mapZ0;
	fixed16_16 s_mapZ1;
	s32 s_mapLayer;

	void automap_projectPosition(fixed16_16* x, fixed16_16* z);
	void automap_drawPointWithRadius(fixed16_16 x, fixed16_16 z, fixed16_16 r, u8 color);
	void automap_drawPointWithDirection(fixed16_16 x, fixed16_16 z, angle14_32 angle, fixed16_16 len, u8 color);
	void automap_drawPoint(fixed16_16 x, fixed16_16 z, u8 color);
	void automap_drawLine(fixed16_16 px1, fixed16_16 pz1, fixed16_16 px2, fixed16_16 pz2, u8 color);
	void automap_drawWall(RWall* wall, u8 color);
	void automap_drawObject(SecObject* obj);
	void automap_drawSector(RSector* sector);
	void automap_drawPlayer(s32 layer);
	void automap_drawSectors();
	void automap_updateDeltaCoords(s32 x, s32 z);

	void automap_serialize(Stream* stream)
	{
		SERIALIZE(SaveVersionInit, s_screenScale, 0xc000);
		SERIALIZE(SaveVersionInit, s_scrLeftScaled, 0);
		SERIALIZE(SaveVersionInit, s_scrRightScaled, 0);
		SERIALIZE(SaveVersionInit, s_scrTopScaled, 0);
		SERIALIZE(SaveVersionInit, s_scrBotScaled, 0);

		SERIALIZE(SaveVersionInit, s_automapAutoCenter, JTRUE);
		SERIALIZE(SaveVersionInit, s_mapXCenterInPixels, 159);
		SERIALIZE(SaveVersionInit, s_mapZCenterInPixels, 99);

		SERIALIZE(SaveVersionInit, s_mapTop, 0);
		SERIALIZE(SaveVersionInit, s_mapLeft, 0);
		SERIALIZE(SaveVersionInit, s_mapRight, 0);
		SERIALIZE(SaveVersionInit, s_mapBot, 0);
		SERIALIZE(SaveVersionInit, s_mapShowSectorMode, 0);
		SERIALIZE(SaveVersionInit, s_mapShowAllLayers, JFALSE);

		SERIALIZE(SaveVersionInit, s_mapPrevPlayerX, 0);
		SERIALIZE(SaveVersionInit, s_mapPrevPlayerZ, 0);

		SERIALIZE(SaveVersionInit, s_pdaActive, JFALSE);
		SERIALIZE(SaveVersionInit, s_drawAutomap, JFALSE);
		SERIALIZE(SaveVersionInit, s_automapLocked, JTRUE);
		SERIALIZE(SaveVersionInit, s_mapX0, 0);
		SERIALIZE(SaveVersionInit, s_mapX1, 0);
		SERIALIZE(SaveVersionInit, s_mapZ0, 0);
		SERIALIZE(SaveVersionInit, s_mapZ1, 0);
		SERIALIZE(SaveVersionInit, s_mapLayer, 0);
	}

	// _computeScreenBounds() and computeScaledScreenBounds() in the original source:
	// computeScaledScreenBounds() calls _computeScreenBounds() - so merged here.
	void automap_computeScreenBounds()
	{
		ScreenRect* screenRect = vfb_getScreenRect(VFB_RECT_RENDER);

		{
			u32 dispWidth, dispHeight;
			vfb_getResolution(&dispWidth, &dispHeight);

			fixed16_16 leftEdge = intToFixed16(screenRect->left - s32(dispWidth) + 1);
			s_scrLeftScaled = div16(leftEdge, s_screenScale);

			fixed16_16 rightEdge = intToFixed16(screenRect->right - s32(dispWidth) + 1);
			s_scrRightScaled = div16(rightEdge, s_screenScale);

			fixed16_16 topEdge = intToFixed16(screenRect->top - s32(dispHeight) + 1);
			s_scrTopScaled = -div16(topEdge, s_screenScale);

			fixed16_16 botEdge = intToFixed16(screenRect->bot - s32(dispHeight) + 1);
			s_scrBotScaled = -div16(botEdge, s_screenScale);
		}
	}

	void automap_resetScale()
	{
		// default scale.
		s_screenScale = 0xc000;	// 0.75

		// Adjust for resolution.
		u32 width, height;
		vfb_getResolution(&width, &height);

		if (height != 200)
		{
			fixed16_16 scaleFactor = div16(intToFixed16(height), FIXED(200));
			s_screenScale = mul16(s_screenScale, scaleFactor);
		}
	}

	void automap_updateDeltaCoords(s32 x, s32 z)
	{
		s_mapX1 -= div16(x, s_screenScale);
		s_mapZ1 += div16(z, s_screenScale);
	}

	void automap_updateMapData(MapUpdateID id)
	{
		switch (id)
		{
			case MAP_CENTER_PLAYER:
			{
				RSector* sector = s_playerEye ? s_playerEye->sector : nullptr;
				if (sector) { s_mapLayer = sector->layer; }
				s_mapX1 = s_mapX0 = s_eyePos.x;
				s_mapZ1 = s_mapZ0 = s_eyePos.z;
			} break;
			case MAP_MOVE1_UP:
			{
				if (s_pdaActive)
				{
					s_mapZ1 += div16(FIXED(20), s_screenScale);
				}
				else if (!s_automapAutoCenter)
				{
					s32 speed = FIXED(60) << (s_playerRun * 2);
					speed >>= s_playerSlow;
					s_mapZ1 += mul16(speed, s_deltaTime);
				}
			} break;
			case MAP_MOVE1_DN:
			{
				if (s_pdaActive)
				{
					s_mapZ1 -= div16(FIXED(20), s_screenScale);
				}
				else if (!s_automapAutoCenter)
				{
					s32 speed = FIXED(60) << (s_playerRun * 2);
					speed >>= s_playerSlow;
					s_mapZ1 -= mul16(speed, s_deltaTime);
				}
			} break;
			case MAP_MOVE1_LEFT:
			{
				if (s_pdaActive)
				{
					s_mapX1 -= div16(FIXED(20), s_screenScale);
				}
				else if (!s_automapAutoCenter)
				{
					s32 speed = FIXED(60) << (s_playerRun * 2);
					speed >>= s_playerSlow;
					s_mapX1 -= mul16(speed, s_deltaTime);
				}
			} break;
			case MAP_MOVE1_RIGHT:
			{
				if (s_pdaActive)
				{
					s_mapX1 += div16(FIXED(20), s_screenScale);
				}
				else if (!s_automapAutoCenter)
				{
					s32 speed = FIXED(60) << (s_playerRun * 2);
					speed >>= s_playerSlow;
					s_mapX1 += mul16(speed, s_deltaTime);
				}
			} break;
			case MAP_ZOOM_IN:
			{
				if (s_pdaActive)
				{
					// screenScale *= 0.8
					s_screenScale = mul16(0xcccc, s_screenScale);
					s_screenScale = max(0x666, s_screenScale);
				}
				else
				{
					s32 speed = s_screenScale << s_playerRun;
					speed >>= s_playerSlow;
					s_screenScale += mul16(speed, s_deltaTime);
				}
				s_screenScale = min(FIXED(32), s_screenScale);
				automap_computeScreenBounds();
			} break;
			case MAP_ZOOM_OUT:
			{
				if (s_pdaActive)
				{
					// screenScale /= 0.8
					s_screenScale = div16(s_screenScale, 0xcccc);
					s_screenScale = min(FIXED(32), s_screenScale);
				}
				else
				{
					s32 speed = s_screenScale << s_playerRun;
					speed >>= s_playerSlow;
					s_screenScale -= mul16(speed, s_deltaTime);
				}
				s_screenScale = max(0x666, s_screenScale);
				automap_computeScreenBounds();
			} break;
			case MAP_LAYER_UP:
			{
				s_mapLayer = min(s_levelState.maxLayer, s_mapLayer + 1);
			} break;
			case MAP_LAYER_DOWN:
			{
				s_mapLayer = max(s_levelState.minLayer, s_mapLayer - 1);
			} break;
			case MAP_ENABLE_AUTOCENTER:
			{
				s_automapAutoCenter = JTRUE;
			} break;
			case MAP_DISABLE_AUTOCENTER:
			{
				s_automapAutoCenter = JFALSE;
			} break;
			case MAP_TOGGLE_ALL_LAYERS:
			{
				s_mapShowAllLayers = !s_mapShowAllLayers;
				if (!s_pdaActive)
				{
					if (s_mapShowAllLayers)
					{
						hud_sendTextMessage(17);
					}
					else
					{
						hud_sendTextMessage(18);
					}
				}
			} break;
			case MAP_INCR_SECTOR_MODE:
			{
				s_mapShowSectorMode++;
				if (s_mapShowSectorMode >= 3)
				{
					s_mapShowSectorMode = 0;
				}
			} break;
			case MAP_TELEPORT:
			{
				// TODO
			} break;
		}
	}
				
	void automap_disableLock()
	{
		s_automapLocked = JFALSE;
		automap_updateMapData(MAP_DISABLE_AUTOCENTER);  // This splits out map position 0 and 1.
	}

	void automap_enableLock()
	{
		s_automapLocked = JTRUE;
	}

	void automap_setPdaActive(JBool enable)
	{
		s_pdaActive = enable;
	}

	void automap_draw(u8* framebuffer)
	{
		u32 dispWidth, dispHeight;
		vfb_getResolution(&dispWidth, &dispHeight);
		screenDraw_beginLines(dispWidth, dispHeight);

		s_mapXCenterInPixels = dispWidth/2 - 1;
		s_mapZCenterInPixels = dispHeight/2 - 1;
		s_mapFramebuffer = framebuffer;
		automap_drawSectors();
		s_pdaActive = JFALSE;
	}

	s32 automap_getLayer()
	{
		return s_mapLayer;
	}

	void automap_setLayer(s32 layer)
	{
		s_mapLayer = layer;
	}

	void automap_drawSectors()
	{
		if (s_automapAutoCenter && !s_pdaActive)
		{
			s_mapX0 = s_eyePos.x;
			s_mapX1 = s_mapX0;
			s_mapZ0 = s_eyePos.z;
			s_mapZ1 = s_mapZ0;
		}
		else
		{
			s_mapX0 = s_mapX1;
			s_mapZ0 = s_mapZ1;
		}

		// Compute the screen bounds and map corners.
		automap_computeScreenBounds();
		s_mapLeft  = s_scrLeftScaled + s_mapX0;
		s_mapRight = s_scrRightScaled + s_mapX0;
		s_mapBot   = s_scrBotScaled + s_mapZ0;
		s_mapTop   = s_scrTopScaled + s_mapZ0;

		// Draw the sectors.
		RSector* sector = s_levelState.sectors;
		for (u32 i = 0; i < s_levelState.sectorCount; i++, sector++)
		{
			if (!s_mapShowAllLayers)
			{
				s32 layer = sector->layer;
				if (layer != s_mapLayer)
				{
					continue;
				}
			}
			automap_drawSector(sector);
		}

		SecObject* player = s_playerObject;
		sector = player->sector;
		if (!s_automapAutoCenter || s_mapLayer != sector->layer)
		{
			automap_drawPoint(s_mapX1, s_mapZ1, 6);
		}
		automap_drawPlayer(s_mapLayer);
		if (!s_automapAutoCenter)
		{
			// Re-center the automap if the player moves.
			if (player->posWS.x != s_mapPrevPlayerX || player->posWS.z != s_mapPrevPlayerZ)
			{
				s_automapAutoCenter = JTRUE;
			}
			s_mapPrevPlayerX = player->posWS.x;
			s_mapPrevPlayerZ = player->posWS.z;
		}
	}
		
	void automap_projectPosition(fixed16_16* x, fixed16_16* z)
	{
		*x -= s_mapX0;
		*z -= s_mapZ0;
		*x = mul16(*x, s_screenScale);
		*z = -mul16(*z, s_screenScale);

		*x = floor16(*x) + s_mapXCenterInPixels;
		*z = floor16(*z) + s_mapZCenterInPixels;
	}

	void automap_drawPointWithRadius(fixed16_16 x, fixed16_16 z, fixed16_16 r, u8 color)
	{
		ScreenRect* screenRect = vfb_getScreenRect(VFB_RECT_RENDER);
		automap_projectPosition(&x, &z);
		fixed16_16 rScreen = mul16(r, s_screenScale);
		screen_drawCircle(screenRect, x, z, rScreen, 0x1c7, color, s_mapFramebuffer);
	}

	void automap_drawPointWithDirection(fixed16_16 x, fixed16_16 z, angle14_32 angle, fixed16_16 len, u8 color)
	{
		fixed16_16 x0 = x;
		fixed16_16 z0 = z;

		fixed16_16 sinAngle, cosAngle;
		sinCosFixed(angle, &sinAngle, &cosAngle);
		x0 -= mul16(sinAngle, len) >> 2;
		z0 -= mul16(cosAngle, len) >> 2;
		fixed16_16 x1 = x0 + mul16(sinAngle, len);
		fixed16_16 z1 = z0 + mul16(cosAngle, len);

		// Add 90 degrees to the angle.
		sinCosFixed(angle + 0xfff, &sinAngle, &cosAngle);
		fixed16_16 x2 = x0 + mul16(sinAngle, len);
		fixed16_16 z2 = z0 + mul16(cosAngle, len);
		fixed16_16 x3 = x0 - mul16(sinAngle, len);
		fixed16_16 z3 = z0 - mul16(cosAngle, len);

		fixed16_16 px1 = x1;
		fixed16_16 px2 = x2;
		fixed16_16 pz1 = z1;
		fixed16_16 pz2 = z2;
		automap_projectPosition(&px1, &pz1);
		automap_projectPosition(&px2, &pz2);
		automap_drawLine(px1, pz1, px2, pz2, color);

		px1 = x1;
		px2 = x3;
		pz1 = z1;
		pz2 = z3;
		automap_projectPosition(&px1, &pz1);
		automap_projectPosition(&px2, &pz2);
		automap_drawLine(px1, pz1, px2, pz2, color);

		px1 = x2;
		px2 = x3;
		pz1 = z2;
		pz2 = z3;
		automap_projectPosition(&px1, &pz1);
		automap_projectPosition(&px2, &pz2);
		automap_drawLine(px1, pz1, px2, pz2, color);
	}

	void automap_drawPoint(fixed16_16 x, fixed16_16 z, u8 color)
	{
		ScreenRect* screenRect = vfb_getScreenRect(VFB_RECT_RENDER);
		automap_projectPosition(&x, &z);
		screen_drawPoint(screenRect, x, z, color, s_mapFramebuffer);
	}

	void automap_drawLine(fixed16_16 x0, fixed16_16 z0, fixed16_16 x1, fixed16_16 z1, u8 color)
	{
		ScreenRect* screenRect = vfb_getScreenRect(VFB_RECT_RENDER);
		screen_drawLine(screenRect, x0, z0, x1, z1, color, s_mapFramebuffer);
	}

	void automap_drawWall(RWall* wall, u8 color)
	{
		vec2_fixed* w0 = wall->w0;
		vec2_fixed* w1 = wall->w1;
		fixed16_16 x0 = w0->x;
		fixed16_16 x1 = w1->x;
		fixed16_16 z0 = w0->z;
		fixed16_16 z1 = w1->z;

		// The DOS code clips the line here, but it also gets clipped again in the underlying code - no point clipping twice.
		automap_projectPosition(&x0, &z0);
		automap_projectPosition(&x1, &z1);
		automap_drawLine(x0, z0, x1, z1, color);
	}

	u8 automap_getWallColor(RWall* wall)
	{
		u8 color;
		if (wall->flags1 & WF1_HIDE_ON_MAP)
		{
			color = WCOLOR_INVISIBLE;
		}
		else if (wall->flags1 & WF1_SHOW_AS_LEDGE_ON_MAP)
		{
			color = WCOLOR_LEDGE;
		}
		else if (wall->flags1 & WF1_SHOW_AS_DOOR_ON_MAP)
		{
			color = WCOLOR_DOOR;
		}
		else if ((wall->flags1 & WF1_SHOW_NORMAL_ON_MAP) || !wall->nextSector)
		{
			color = WCOLOR_NORMAL;
		}
		else if (sector_isDoor(wall->sector) || sector_isDoor(wall->nextSector))
		{
			color = WCOLOR_DOOR;
		}
		else if (s_mapShowSectorMode == 2)
		{
			color = WCOLOR_GRAYED_OUT;
		}
		else
		{
			RSector* curSector = wall->sector;
			RSector* nextSector = wall->nextSector;
			fixed16_16 curFloorHeight = curSector->floorHeight;
			fixed16_16 nextFloorHeight = nextSector->floorHeight;
			fixed16_16 floorDelta = TFE_Jedi::abs(curFloorHeight - nextFloorHeight);
			if (floorDelta >= 0x4000)	// 0.25 units
			{
				color = WCOLOR_LEDGE;
			}
			else
			{
				color = WCOLOR_INVISIBLE;
			}
		}

		return color;
	}

	void automap_drawSector(RSector* sector)
	{
		if (!s_mapShowSectorMode && !(sector->flags1 & SEC_FLAGS1_RENDERED))
		{
			return;
		}

		RWall* wall = sector->walls;
		for (s32 i = 0; i < sector->wallCount; i++, wall++)
		{
			if (!s_mapShowSectorMode && !wall->seen)
			{
				continue;
			}

			u8 color = automap_getWallColor(wall);
			if (color != WCOLOR_INVISIBLE)
			{
				automap_drawWall(wall, color);
			}
		}
		if (s_mapShowSectorMode)
		{
			SecObject** objIter = sector->objectList;
			for (s32 i = 0; i < sector->objectCount; objIter++)
			{
				SecObject* obj = *objIter;
				while (!obj)
				{
					objIter++;
					obj = *objIter;
				}
				if (obj)
				{
					automap_drawObject(obj);
					i++;
				}
			}
		}
	}
		
	void automap_drawObject(SecObject* obj)
	{
		u8 color = MOBJCOLOR_DEFAULT;
		if (obj->flags & OBJ_FLAG_NEEDS_TRANSFORM)
		{
			if (obj->entityFlags & ETFLAG_PROJECTILE)
			{
				color = MOBJCOLOR_PROJ;
			}
			else if (obj->entityFlags & ETFLAG_CORPSE)
			{
				color = MOBJCOLOR_CORPSE;
			}
			else if (obj->entityFlags & ETFLAG_LANDMINE)
			{
				color = MOBJCOLOR_LANDMINE;
			}
			else if (obj->entityFlags & ETFLAG_PICKUP)
			{
				color = MOBJCOLOR_PICKUP;
			}
			else if (obj->entityFlags & ETFLAG_SCENERY)
			{
				color = MOBJCOLOR_SCENERY;
			}

			ObjectType type = obj->type;
			if (type == OBJ_TYPE_3D || type == OBJ_TYPE_FRAME)
			{
				if (obj->worldWidth)
				{
					automap_drawPointWithRadius(obj->posWS.x, obj->posWS.z, obj->worldWidth, color);
				}
				else
				{
					automap_drawPoint(obj->posWS.x, obj->posWS.z, color);
				}
			}
			else if (type == OBJ_TYPE_SPRITE)
			{
				automap_drawPointWithDirection(obj->posWS.x, obj->posWS.z, obj->yaw, MOBJSPRITE_DRAW_LEN, color);
			}
		}
	}

	void automap_drawPlayer(s32 layer)
	{
		SecObject* player = s_playerObject;
		fixed16_16 x0 = player->posWS.x;
		fixed16_16 z0 = player->posWS.z;
		RSector* sector = player->sector;

		if (sector->layer == layer)
		{
			fixed16_16 sinYaw, cosYaw;
			sinCosFixed(player->yaw, &sinYaw, &cosYaw);

			fixed16_16 x1 = x0 + mul16(sinYaw, FIXED(4));
			fixed16_16 z1 = z0 + mul16(cosYaw, FIXED(4));

			automap_projectPosition(&x0, &z0);
			automap_projectPosition(&x1, &z1);
			automap_drawLine(x0, z0, x1, z1, 8);

			fixed16_16 r = mul16(player->worldWidth, s_screenScale);
			if (r >= 0x18000)	// 1.5
			{
				automap_drawPointWithRadius(player->posWS.x, player->posWS.z, player->worldWidth, 7);
			}
			automap_drawPoint(player->posWS.x, player->posWS.z, 6);
		}
	}
}  // namespace TFE_DarkForces
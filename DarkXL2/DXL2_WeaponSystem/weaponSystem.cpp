#include "weaponSystem.h"
#include <DXL2_Asset/spriteAsset.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/modelAsset.h>
#include <DXL2_LogicSystem/logicSystem.h>
#include <DXL2_ScriptSystem/scriptSystem.h>
#include <DXL2_System/system.h>
#include <DXL2_System/math.h>
#include <DXL2_Game/physics.h>
#include <DXL2_InfSystem/infSystem.h>
#include <DXL2_FileSystem/filestream.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_Game/player.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

// TEMP
#include <DXL2_Game/modelRendering.h>
#include <DXL2_Game/renderCommon.h>

namespace DXL2_WeaponSystem
{
	static const char* c_weaponScriptName[WEAPON_COUNT] =
	{
		"weapon_punch",		// WEAPON_PUNCH
		"weapon_pistol",	// WEAPON_PISTOL
		"weapon_rifle",		// WEAPON_RIFLE
	};
	#define MAX_WEAPON_FRAMES 16

	struct WeaponObject
	{
		s32 x, y;
		s32 yOffset;	//yOffset for things like pitch that shouldn't affect animations.
		s32 state;
		s32 frame;
		s32 scaledWidth;
		s32 scaledHeight;

		f32 motion;
		f32 pitch;
		f32 time;

		const TextureFrame* frames[MAX_WEAPON_FRAMES];
	};

	struct ScreenObject
	{
		s32 width;
		s32 height;
		s32 scaleX;
		s32 scaleY;
	};

	struct ScriptWeapon
	{
		// Logic Name
		std::string name;

		// Script functions (may be NULL)
		SCRIPT_FUNC_PTR switchTo;
		SCRIPT_FUNC_PTR switchFrom;
		SCRIPT_FUNC_PTR tick;
		SCRIPT_FUNC_PTR firePrimary;
		SCRIPT_FUNC_PTR fireSecondary;
	};

	static WeaponObject s_weapon = { 0 };
	static ScreenObject s_screen = { 0 };
	static WeaponObject* s_weaponPtr = &s_weapon;
	static ScreenObject* s_screenPtr = &s_screen;
	static bool s_initialized = false;
	static std::vector<ScriptWeapon> s_scriptWeapons;

	static f32 s_accum = 0.0f;
	static f32 c_step = 1.0f / 60.0f;

	static s32 s_activeWeapon = -1;
	static s32 s_nextWeapon = -1;
	static bool s_callSwitchTo = false;

	static DXL2_Renderer* s_renderer;

	static bool s_shot = false;
	static s32 s_primeTimer = 0;
	static Vec3f s_shotLoc;
	static s32 s_shotSectorId;
	static f32 s_shotYaw;
	static f32 s_shotPitch;
	static Model* s_model;
	static Player* s_player;

	static bool s_showExplosion = false;
	static Vec3f s_explosionLoc;
	static f32 s_explosionAnimTime;
	static Sprite* s_explosionSprite = nullptr;

	static Vec3f s_shootDir;

	void DXL2_WeaponFire();
	void DXL2_WeaponPrimed(s32 primeCount);
	void DXL2_WeaponLightEnd();

	void clearWeapons()
	{
		s_scriptWeapons.clear();
		s_activeWeapon = -1;
		s_nextWeapon = -1;
		s_callSwitchTo = false;
		s_showExplosion = false;
		s_primeTimer = 0;
		s_model = nullptr;
		s_explosionSprite = nullptr;
		s_explosionAnimTime = 0.0f;
	}

	bool registerWeapon(Weapon weapon, s32 id)
	{
		if (s_scriptWeapons.size() <= id)
		{
			s_scriptWeapons.resize(id + 1);
		}

		const size_t index = id;
		ScriptWeapon& newWeapon = s_scriptWeapons[index];

		newWeapon.name = c_weaponScriptName[id];
		if (DXL2_ScriptSystem::loadScriptModule(SCRIPT_TYPE_WEAPON, newWeapon.name.c_str()))
		{
			newWeapon.switchTo      = DXL2_ScriptSystem::getScriptFunction(newWeapon.name.c_str(), "void switchTo()");
			newWeapon.switchFrom    = DXL2_ScriptSystem::getScriptFunction(newWeapon.name.c_str(), "void switchFrom()");
			newWeapon.tick		    = DXL2_ScriptSystem::getScriptFunction(newWeapon.name.c_str(), "void tick()");
			newWeapon.firePrimary   = DXL2_ScriptSystem::getScriptFunction(newWeapon.name.c_str(), "void firePrimary()");
			newWeapon.fireSecondary = DXL2_ScriptSystem::getScriptFunction(newWeapon.name.c_str(), "void fireSecondary()");
		}
		
		return true;
	}
	
	void DXL2_LoadWeaponFrame(s32 frameIndex, std::string& textureName)
	{
		if (frameIndex < 0 || frameIndex >= MAX_WEAPON_FRAMES) { return; }

		const Texture* texture = DXL2_Texture::get(textureName.c_str());
		if (!texture) { return; }
		s_weapon.frames[frameIndex] = &texture->frames[0];
	}
		
	void DXL2_NextWeapon()
	{
		memset(&s_weapon, 0, sizeof(WeaponObject));
		s_weapon.state = 0; // had a previous weapon.

		s_callSwitchTo = true;
		s_activeWeapon = s_nextWeapon;
		s_nextWeapon = -1;
	}

	f32 DXL2_GetSineMotion(f32 time)
	{
		return sinf(time * 2.0f * PI);
	}

	f32 DXL2_GetCosMotion(f32 time)
	{
		return cosf(time * 2.0f * PI);
	}

	bool init(DXL2_Renderer* renderer)
	{
		s_renderer = renderer;
		clearWeapons();
		s_accum = 0.0f;

		if (s_initialized)
		{
			registerWeapon(WEAPON_PUNCH, 0);
			registerWeapon(WEAPON_PISTOL, 1);
			registerWeapon(WEAPON_RIFLE, 2);

			return true;
		}
		s_initialized = true;
		// register script functions.
				
		// WeaponObject type
		std::vector<ScriptTypeProp> prop;
		prop.reserve(16);

		prop.clear();
		prop.push_back({ "int x",				offsetof(WeaponObject, x) });
		prop.push_back({ "int y",				offsetof(WeaponObject, y) });
		prop.push_back({ "int yOffset",         offsetof(WeaponObject, yOffset) });
		prop.push_back({ "int state",			offsetof(WeaponObject, state) });
		prop.push_back({ "int frame",			offsetof(WeaponObject, frame) });
		prop.push_back({ "int scaledWidth",		offsetof(WeaponObject, scaledWidth) });
		prop.push_back({ "int scaledHeight",	offsetof(WeaponObject, scaledHeight) });
		prop.push_back({ "const float motion",	offsetof(WeaponObject, motion) });
		prop.push_back({ "const float pitch",	offsetof(WeaponObject, pitch) });
		prop.push_back({ "float time",			offsetof(WeaponObject, time) });
		DXL2_ScriptSystem::registerRefType("WeaponObject", prop);

		// Screen Object
		prop.clear();
		prop.push_back({"const int width",  offsetof(ScreenObject, width) });
		prop.push_back({"const int height", offsetof(ScreenObject, height) });
		prop.push_back({"const int scaleX", offsetof(ScreenObject, scaleX) });
		prop.push_back({"const int scaleY", offsetof(ScreenObject, scaleY) });
		DXL2_ScriptSystem::registerRefType("ScreenObject", prop);

		// Functions
		DXL2_ScriptSystem::registerFunction("void DXL2_LoadWeaponFrame(int frameIndex, const string &in)", SCRIPT_FUNCTION(DXL2_LoadWeaponFrame));
		DXL2_ScriptSystem::registerFunction("void DXL2_NextWeapon()", SCRIPT_FUNCTION(DXL2_NextWeapon));
		DXL2_ScriptSystem::registerFunction("float DXL2_GetSineMotion(float time)", SCRIPT_FUNCTION(DXL2_GetSineMotion));
		DXL2_ScriptSystem::registerFunction("float DXL2_GetCosMotion(float time)", SCRIPT_FUNCTION(DXL2_GetCosMotion));
		DXL2_ScriptSystem::registerFunction("void DXL2_WeaponFire()", SCRIPT_FUNCTION(DXL2_WeaponFire));
		DXL2_ScriptSystem::registerFunction("void DXL2_WeaponPrimed(int primeCount)", SCRIPT_FUNCTION(DXL2_WeaponPrimed));
		DXL2_ScriptSystem::registerFunction("void DXL2_WeaponLightEnd()", SCRIPT_FUNCTION(DXL2_WeaponLightEnd));

		// Register "self" and Logic parameters.
		DXL2_ScriptSystem::registerGlobalProperty("WeaponObject @weapon", &s_weaponPtr);
		DXL2_ScriptSystem::registerGlobalProperty("ScreenObject @screen", &s_screenPtr);

		// Register global constants.

		u32 width, height;
		s_renderer->getResolution(&width, &height);
		s_screen.width = width;
		s_screen.height = height;
		s_screen.scaleX = 256 * width / 320;
		s_screen.scaleY = 256 * height / 200;

		registerWeapon(WEAPON_PUNCH,  0);
		registerWeapon(WEAPON_PISTOL, 1);
		registerWeapon(WEAPON_RIFLE,  2);

		return true;
	}

	void shutdown()
	{
	}
		
	void switchToWeapon(Weapon weapon)
	{
		if (weapon == s_activeWeapon) { return; }

		if (s_activeWeapon < 0)
		{
			memset(&s_weapon, 0, sizeof(WeaponObject));
			s_weapon.state = -1; // no previous weapon.

			if (s_scriptWeapons[weapon].switchTo)
			{
				DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_WEAPON, s_scriptWeapons[weapon].switchTo);
			}
			s_activeWeapon = weapon;
			s_nextWeapon = -1;
		}
		else
		{
			s_nextWeapon = weapon;
			if (s_scriptWeapons[s_activeWeapon].switchFrom)
			{
				DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_WEAPON, s_scriptWeapons[s_activeWeapon].switchFrom);
			}
			else
			{
				DXL2_NextWeapon();
			}
		}
	}

	void update(f32 motion, Player* player)
	{
		if (!player) { return; }

		s_weapon.motion = motion;
		s_weapon.pitch = player->m_pitch / PITCH_LIMIT;

		// First call the "switch to" function for the next weapon if needed.
		if (s_callSwitchTo && s_activeWeapon >= 0 && s_scriptWeapons[s_activeWeapon].switchTo)
		{
			DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_WEAPON, s_scriptWeapons[s_activeWeapon].switchTo);
		}
		s_callSwitchTo = false;

		// Then call the fixed-rate tick().
		s_accum += (f32)DXL2_System::getDeltaTime();
		while (s_accum >= c_step)
		{
			if (s_scriptWeapons[s_activeWeapon].tick)
			{
				DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_WEAPON, s_scriptWeapons[s_activeWeapon].tick);
			}
			if (s_primeTimer) { s_primeTimer--; }
			s_accum -= c_step;
		}
	}
	
	void DXL2_WeaponFire()
	{
		s_shot = true;
	}

	void DXL2_WeaponPrimed(s32 primeCount)
	{
		s_primeTimer = primeCount;
	}

	void DXL2_WeaponLightEnd()
	{
		if (s_player) { s_player->m_shooting = false; }
	}

	// HACKY DEBUG CODE, REPLACE WITH REAL SHOOTING CODE!
	void shoot(Player* player, const Vec2f* dir)
	{
		if (s_activeWeapon != 1 || s_nextWeapon >= 0) { return; }
		if (s_primeTimer > 0) { return; }

		u32 w, h;
		s_renderer->getResolution(&w, &h);

		Vec3f origin = DXL2_RenderCommon::unproject({ s_weapon.x + 16 * s_screen.scaleX / 256, s_weapon.y + s_weapon.yOffset }, 1.0f);
		Vec3f target = DXL2_RenderCommon::unproject({ s32(w) / 2, s32(h) / 2 }, 10000.0f);

		s_player = player;
		s_shotLoc = origin;
		s_shotSectorId = player->m_sectorId;
		s_shotYaw = -player->m_yaw * 180.0f / PI;
		s_shotPitch = 1.0f * player->m_pitch * 180.0f / PI;
		s_model = DXL2_Model::get("WRBOLT.3DO");

		s_shootDir = { target.x - origin.x, target.y - origin.y, target.z - origin.z };
				
		f32 magSq = s_shootDir.x*s_shootDir.x + s_shootDir.z*s_shootDir.z + s_shootDir.y*s_shootDir.y;
		f32 dirScale = 1.0f / sqrtf(magSq);
		s_shootDir.x *= dirScale;
		s_shootDir.y *= dirScale;
		s_shootDir.z *= dirScale;

		// get Yaw and pitch from direction.
		s_shotYaw = -atan2f(s_shootDir.z, s_shootDir.x) * 180.0f / PI + 90.0f;
		//s_shotPitch = -atan2f(s_shootDir.y, s_shootDir.z) * 180.0f / PI * 0.5f;

		if (s_scriptWeapons[s_activeWeapon].firePrimary)
		{
			DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_WEAPON, s_scriptWeapons[s_activeWeapon].firePrimary);
		}
		if (s_player) { s_player->m_shooting = true; }
		s_primeTimer = INT_MAX;

		// Do an initial trace from the player to the weapon start, to catch cases where sectors should change.
		Vec3f playerOrigin = { s_player->pos.x, origin.y, s_player->pos.z };
		f32 dist = DXL2_Math::distance(&playerOrigin, &s_shotLoc);
		Vec3f initDir = { s_shotLoc.x - playerOrigin.x, s_shotLoc.y - playerOrigin.y, s_shotLoc.z - playerOrigin.z };
		initDir = DXL2_Math::normalize(&initDir);

		const Ray ray =
		{
			playerOrigin,
			initDir,
			s_shotSectorId,
			dist
		};
		RayHitInfo hitInfo;
		if (DXL2_Physics::traceRay(&ray, &hitInfo))
		{
			// Stop the projectile.
			s_shot = false;
			s_showExplosion = true;
			s_explosionLoc = hitInfo.hitPoint;
			s_explosionAnimTime = 0.0f;
			s_explosionSprite = DXL2_Sprite::getSprite("EXPTINY.WAX");

			if (hitInfo.obj)
			{
				DXL2_LogicSystem::damageObject(hitInfo.obj, 1);
			}
			else if (hitInfo.hitWallId >= 0)
			{
				DXL2_InfSystem::shootWall(&hitInfo.hitPoint, hitInfo.hitSectorId, hitInfo.hitWallId);
			}
		}
		else
		{
			s_shotSectorId = hitInfo.hitSectorId;
		}
	}

	void draw(Player* player, Vec3f* cameraPos, u8 ambient)
	{
		if (s_activeWeapon < 0) { return; }

		const TextureFrame* frame = s_weapon.frames[s_weapon.frame];
		if (!frame) { return; }

		// TEST RAYTRACE
		u32 w, h;
		s_renderer->getResolution(&w, &h);
		Ray ray =
		{
			*cameraPos,
			DXL2_RenderCommon::unproject({ s32(w) / 2, s32(h) / 2 }, 10000.0f),
			player->m_sectorId,
			1000.0f
		};
		f32 rayLen = sqrtf(ray.dir.x*ray.dir.x + ray.dir.y*ray.dir.y + ray.dir.z*ray.dir.z);
		f32 rayScale = 1.0f / rayLen;
		ray.dir.x *= rayScale; ray.dir.y *= rayScale; ray.dir.z *= rayScale;

		RayHitInfo hitInfo;
		if (0 && DXL2_Physics::traceRay(&ray, &hitInfo))
		{
			Vec3f screenPos = DXL2_RenderCommon::project(&hitInfo.hitPoint);
			if (screenPos.x >= 0 && screenPos.y >= 0 && screenPos.x < w && screenPos.y < h)
			{
				s32 sz = (s32)std::max(screenPos.z * 100.0f, 1.0f);
				s_renderer->drawColoredQuad(s32(screenPos.x)-sz/2, s32(screenPos.y)-sz/2, sz, sz, 6);
			}
		}

		// DEBUG
		if (s_shot && s_model)
		{
			Vec3f angles = { 0.0f, s_shotYaw, s_shotPitch };
			const f32 ca = cosf(s_player->m_yaw);
			const f32 sa = sinf(s_player->m_yaw);
			f32 rot[] = {ca, sa};
			s32 pitchOffset = s32(f32(h/2) * s_player->m_pitch * PITCH_PIXEL_SCALE / PITCH_LIMIT) + (h/2);
						
			Vec3f modelScale = { 1.0f, 1.0f, 0.05f }, defaultScale = { 1.0f, 1.0f, 1.0f };
			DXL2_ModelRender::setModelScale(&modelScale);
			DXL2_ModelRender::setClip(0, s32(w)-1, nullptr, nullptr);
			DXL2_ModelRender::draw(s_model, &angles, &s_shotLoc, cameraPos, rot, (f32)pitchOffset, nullptr);
			DXL2_ModelRender::setModelScale(&defaultScale);

			// Trace a ray between the current location and the next. Then check if it hit anything.
			const Ray ray =
			{
				s_shotLoc,
				s_shootDir,
				s_shotSectorId,
				10.0f
			};
			RayHitInfo hitInfo;
			if (DXL2_Physics::traceRay(&ray, &hitInfo))
			{
				// Stop the projectile.
				s_shot = false;
				s_showExplosion = true;
				s_explosionLoc = hitInfo.hitPoint;
				s_explosionAnimTime = 0.0f;
				s_explosionSprite = DXL2_Sprite::getSprite("EXPTINY.WAX");

				if (hitInfo.obj)
				{
					DXL2_LogicSystem::damageObject(hitInfo.obj, 1);
				}
				else if (hitInfo.hitWallId >= 0)
				{
					DXL2_InfSystem::shootWall(&hitInfo.hitPoint, hitInfo.hitSectorId, hitInfo.hitWallId);
				}
			}
			else
			{
				s_shotSectorId = hitInfo.hitSectorId;
				s_shotLoc.x += s_shootDir.x * 10.0f;
				s_shotLoc.y += s_shootDir.y * 10.0f;
				s_shotLoc.z += s_shootDir.z * 10.0f;
			}
		}
		else if (s_showExplosion && s_explosionSprite)
		{
			f32 frameTime = s_explosionSprite->anim[0].frameRate * s_explosionAnimTime;
			s32 frame = s32(frameTime);
			if (frame >= s_explosionSprite->anim[0].angles[0].frameCount)
			{
				s_showExplosion = false;
				s_explosionAnimTime = 0.0f;
			}
			else
			{
				s32 frameIndex = s_explosionSprite->anim[0].angles[0].frameIndex[frame];
				Frame* frame = &s_explosionSprite->frames[frameIndex];

				s_explosionAnimTime += (f32)DXL2_System::getDeltaTime();
				TextureFrame texFrame = { 0 };
				texFrame.width = frame->width;
				texFrame.height = frame->height;
				texFrame.image = frame->image;

				Vec3f eProj = DXL2_RenderCommon::project(&s_explosionLoc);
				if (eProj.x >= 0.0f && eProj.y >= 0.0f)
				{
					f32 focalLength = DXL2_RenderCommon::getFocalLength() / 8.0f;
					f32 heightScale = DXL2_RenderCommon::getHeightScale() / 8.0f;

					f32 ew = frame->width  * focalLength * eProj.z;
					f32 eh = frame->height * heightScale * eProj.z;
					s32 sx = s32(256 * focalLength * eProj.z);
					s32 sy = s32(256 * heightScale * eProj.z);

					s_renderer->blitImage(&texFrame, s32(eProj.x - ew/2), s32(eProj.y - eh/2), sx, sy, 31);
				}
			}
		}

		if (s_player && s_player->m_shooting) { ambient = 31; }
		s_renderer->blitImage(frame, s_weapon.x, s_weapon.y + s_weapon.yOffset, s_screen.scaleX, s_screen.scaleY, ambient);
	}
}

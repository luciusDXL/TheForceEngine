#include "gameLoop.h"
#include "modelRendering.h"
#include "renderCommon.h"
#include "player.h"
#include "physics.h"
#include "view.h"
#include "gameHud.h"
#include <TFE_Game/GameUI/gameUi.h>
#include <TFE_Game/gameConstants.h>
#include <TFE_Game/geometry.h>
#include <TFE_Game/player.h>
#include <TFE_Game/level.h>
#include <TFE_Game/gameObject.h>
#include <TFE_System/system.h>
#include <TFE_System/math.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Input/input.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/levelObjectsAsset.h>
#include <TFE_Asset/infAsset.h>
#include <TFE_Asset/modelAsset.h>
#include <TFE_Asset/vocAsset.h>
#include <TFE_Audio/audioSystem.h>
#include <TFE_InfSystem/infSystem.h>
#include <TFE_LogicSystem/logicSystem.h>
#include <TFE_WeaponSystem/weaponSystem.h>
#include <assert.h>
#include <vector>
#include <algorithm>

using namespace TFE_GameConstants;

namespace TFE_GameLoop
{
	struct PlayerSounds
	{
		const SoundBuffer* jump;
		const SoundBuffer* land;
		const SoundBuffer* landWater;
		const SoundBuffer* fall;
	};

	static LevelData* s_level = nullptr;
	static LevelObjectData* s_levelObjects = nullptr;
	static Player s_player;
	static bool s_crouching = false;
	static f32 s_heightVisual;
	static f32 s_eyeHeight;
	static f32 s_height;
	static f32 s_motion;
	static f32 s_motionTime;
	static f32 s_fallHeight;
	static f32 s_landDist = 0.0f;
	static f32 s_landAnim = 0.0f;
	static s32 s_inputDelay = 0;	// A small delay to avoid "click through" and similar effects.
	static bool s_land = false;
	static bool s_jump = false;
	static PlayerSounds s_playerSounds;
	
	static f32 s_accum = 0.0f;
	
	void updateObjects();
	void updateSoundObjects(const Vec3f* listenerPos);

	void startRenderer(TFE_Renderer* renderer, s32 w, s32 h)
	{
		TFE_View::init(nullptr, renderer, w, h, false);
	}

	bool startLevelFromExisting(const Vec3f* startPos, f32 yaw, s32 startSectorId, const Palette256* pal, LevelObjectData* levelObj, TFE_Renderer* renderer, s32 w, s32 h)
	{
		if (!startPos || !renderer || startSectorId < 0) { return false; }
		TFE_Audio::stopAllSounds();
		TFE_GameUi::reset();

		LevelData* level = TFE_LevelAsset::getLevelData();
		InfData* infData = TFE_InfAsset::getInfData();
		if (!level || !infData) { return false; }
		s_level = level;

		// Initialize the Logic system.
		TFE_LogicSystem::init(&s_player);

		TFE_Physics::init(s_level);
		TFE_Level::startLevel(level, levelObj);
		s_motion = 0.0f;
		s_motionTime = 0.0f;
		s_fallHeight = 0.0f;
		s_landDist = 0.0f;
		s_landAnim = 0.0f;
		s_land = false;
		s_jump = false;
				
		// Current no objects...
		s_levelObjects = levelObj;

		TFE_InfSystem::setupLevel(TFE_InfAsset::getInfData(), level);

		// Setup the player.
		s_player.m_sectorId = startSectorId;
		s_player.pos = *startPos;
		s_player.vel = { 0 };
		s_player.m_yaw = yaw;
		s_player.m_pitch = 0;
		s_player.m_headlampOn = false;
		s_player.m_nightVisionOn = false;
		s_player.m_keys = 0;
		s_heightVisual = s_player.pos.y;
		TFE_RenderCommon::enableNightVision(false);

		TFE_View::init(level, renderer, w, h, false);
		renderer->changeResolution(w, h);

		s_eyeHeight = c_standingEyeHeight;
		s_height = c_standingHeight;
		s_crouching = false;

		TFE_GameHud::init(renderer);
		TFE_GameUi::init(renderer);
		TFE_WeaponSystem::init(renderer);
		TFE_GameUi::toggleNextMission(false);
		TFE_GameHud::clearMessage();

		// For now switch over to the pistol.
		TFE_WeaponSystem::switchToWeapon(WEAPON_PISTOL);
		s_accum = 0.0f;

		s_playerSounds.jump = TFE_VocAsset::get("JUMP-1.VOC");
		s_playerSounds.land = TFE_VocAsset::get("LAND-1.VOC");
		s_playerSounds.fall = TFE_VocAsset::get("FALL.VOC");
		s_playerSounds.landWater = TFE_VocAsset::get("SWIM-IN.VOC");

		s_inputDelay = c_levelLoadInputDelay;

		return true;
	}

	bool startLevel(LevelData* level, const StartLocation& start, TFE_Renderer* renderer, s32 w, s32 h, bool enableViewStats)
	{
		if (!level) { return false; }
		TFE_Audio::stopAllSounds();
		TFE_GameUi::reset();
		
		s_level = level;
		s_motion = 0.0f;
		s_motionTime = 0.0f;
		s_fallHeight = 0.0f;
		s_landDist = 0.0f;
		s_landAnim = 0.0f;
		s_land = false;
		s_jump = false;

		// Initialize the Logic system.
		TFE_LogicSystem::init(&s_player);

		// Load INF
		char infFile[TFE_MAX_PATH];
		size_t nameLen = strlen(level->palette);
		strcpy(infFile, level->palette);
		infFile[nameLen - 3] = 'i';
		infFile[nameLen - 2] = 'n';
		infFile[nameLen - 1] = 'f';
		infFile[nameLen - 0] = 0;
		TFE_InfAsset::load(infFile);
		
		// Load objects.
		char oFile[TFE_MAX_PATH];
		strcpy(oFile, level->palette);
		oFile[nameLen - 3] = 'O';
		oFile[nameLen - 2] = 0;

		s_levelObjects = nullptr;
		if (TFE_LevelObjects::load(oFile))
		{
			s_levelObjects = TFE_LevelObjects::getLevelObjectData();
		}
		if (!s_levelObjects) { return false; }

		TFE_Physics::init(s_level);
		TFE_Level::startLevel(level, s_levelObjects);

		TFE_InfSystem::setupLevel(TFE_InfAsset::getInfData(), level);

		// for now only overriding the start position is supported.
		// TODO: Load objects.
		Vec3f startPos = { start.pos.x, 0.0f, start.pos.z };
		f32 yaw = 0.0f;
		s32 sectorId = -1;
		if (!start.overrideStart)
		{ 
			const u32 count = s_levelObjects->objectCount;
			for (u32 i = 0; i < count; i++)
			{
				if (!s_levelObjects->objects[i].logics.empty() && s_levelObjects->objects[i].logics[0].type == LOGIC_PLAYER)
				{
					startPos.x = s_levelObjects->objects[i].pos.x;
					startPos.y = s_levelObjects->objects[i].pos.y;
					startPos.z = s_levelObjects->objects[i].pos.z;

					sectorId = TFE_Physics::findSector(&startPos);

					yaw = 2.0f * PI - 2.0f * PI * s_levelObjects->objects[i].orientation.y / 360.0f;
					break;
				}
			}
		}
		else
		{
			sectorId = TFE_Physics::findSector(start.layer, &start.pos);
			if (sectorId >= 0)
				startPos.y = s_level->sectors[sectorId].floorAlt;
		}
		if (sectorId < 0) { return false; }

		// Setup the player.
		s_player.m_sectorId = sectorId;
		s_player.pos = startPos;
		s_player.vel = { 0 };
		s_player.m_yaw = yaw;
		s_player.m_pitch = 0;
		s_player.m_headlampOn = false;
		s_player.m_nightVisionOn = false;
		s_player.m_keys = 0;
		s_heightVisual = s_player.pos.y;
		TFE_RenderCommon::enableNightVision(false);
			   			   
		TFE_View::init(level, renderer, w, h, enableViewStats);
		renderer->changeResolution(w, h);

		s_eyeHeight = c_standingEyeHeight;
		s_height = c_standingHeight;
		s_crouching = false;

		TFE_GameHud::init(renderer);
		TFE_GameUi::init(renderer);
		TFE_WeaponSystem::init(renderer);
		TFE_GameUi::toggleNextMission(false);
		TFE_GameHud::clearMessage();

		// For now switch over to the pistol.
		TFE_WeaponSystem::switchToWeapon(WEAPON_PISTOL);
		s_accum = 0.0f;

		s_playerSounds.jump = TFE_VocAsset::get("JUMP-1.VOC");
		s_playerSounds.land = TFE_VocAsset::get("LAND-1.VOC");
		s_playerSounds.fall = TFE_VocAsset::get("FALL.VOC");
		s_playerSounds.landWater = TFE_VocAsset::get("SWIM-IN.VOC");

		s_inputDelay = c_levelLoadInputDelay;
		
		return true;
	}

	void endLevel()
	{
		s_level = nullptr;
		TFE_Level::endLevel();
	}

	const ViewStats* getViewStats()
	{
		return TFE_View::getViewStats();
	}

	static s32 s_iterOverride = 0;
	static const f32 c_pitchLimit = 0.78539816339744830962f;	// +/- 45 degrees.
	static Vec3f s_cameraPos;
	static bool s_forceCrouch = false;
	static bool s_slowToggle = false;
	
	f32 getSpeed(bool* inAir)
	{
		f32 floorHeight, ceilHeight, visualFloorHeight;
		TFE_Physics::getValidHeightRange(&s_player.pos, s_player.m_sectorId, &floorHeight, &visualFloorHeight, &ceilHeight);

		// Running?
		bool running = TFE_Input::keyDown(KEY_LSHIFT) || TFE_Input::keyDown(KEY_RSHIFT);
		if (TFE_Input::keyPressed(KEY_CAPSLOCK))
		{
			s_slowToggle = !s_slowToggle;
		}

		*inAir = s_player.pos.y < floorHeight - 0.001f;
		f32 speed;
		if (s_jump)
		{
			if (s_slowToggle)
				speed = s_crouching ? c_crouchWalkJump*0.5f : c_walkSpeedJump*0.5f;
			else if (running)
				speed = s_crouching ? c_crouchRunJump : c_runSpeedJump;
			else
				speed = s_crouching ? c_crouchWalkJump : c_walkSpeedJump;
		}
		else
		{
			// On ground
			f32 adjSpeed = (s_player.pos.y > visualFloorHeight) ? 0.75f : 1.0f;
			if (s_slowToggle)
				speed = (s_crouching ? c_crouchWalk : c_walkSpeed) * adjSpeed * 0.5f;
			else if (running)
				speed = (s_crouching ? c_crouchRun : c_runSpeed) * adjSpeed;
			else
				speed = (s_crouching ? c_crouchWalk : c_walkSpeed) * adjSpeed;
		}

		if (s_player.m_onScrollFloor)
		{
			speed *= 0.5f;
		}

		return speed;
	}

	Vec3f changeVelocity(const Vec3f* curVel, const Vec3f* newVel, bool inAir, f32 dt)
	{
		if (s_jump)
		{
			return { newVel->x, curVel->y, newVel->z };
		}
		const f32 c_limit = 90.0f * dt;	// 1.5 @ 60 fps

		// move curVel towards newVel
		Vec2f diff = { newVel->x - curVel->x, newVel->z - curVel->z };
		const f32 lenSq = TFE_Math::dot(&diff, &diff);
		if (lenSq > FLT_EPSILON)
		{
			const f32 len = sqrtf(lenSq);
			const f32 scale = std::min(len, c_limit) / len;
			diff.x *= scale;
			diff.z *= scale;
		}

		Vec3f limitedVel = { curVel->x + diff.x, curVel->y, curVel->z + diff.z };
		return limitedVel;
	}
		
	f32 handleLandAnim(f32 dt)
	{
		static const f32 c_landAccel = 6.0f;
		f32 dY = 0.0f;

		if (s_landDist > 0.0f && s_land)
		{
			s_landAnim -= c_landAccel * dt;
			dY = -s_landAnim;
			if (s_landAnim <= -s_landDist)
			{
				s_land = false;
			}
		}
		else if (s_landAnim < 0.0f)
		{
			s_landAnim += c_landAccel * dt;
			if (s_landAnim > 0.0f) { s_landAnim = 0.0f; }
			dY = -s_landAnim;
		}
		else
		{
			s_landAnim = 0.0f;
		}

		return dY;
	}
		
	GameTransition update(GameState curState, GameOverlay curOverlay)
	{
		// Handle Game UI
		GameTransition trans = TRANS_NONE;
				
		const bool escMenuOpen = TFE_GameUi::isEscMenuOpen();
		const bool shouldDrawGame = TFE_GameUi::shouldDrawGame();
		const bool shouldUpdateGame = TFE_GameUi::shouldUpdateGame();
		const GameUiResult result = TFE_GameUi::update(&s_player);
		if (result == GAME_UI_QUIT)
		{
			return TRANS_QUIT;
		}
		else if (result == GAME_UI_ABORT)
		{
			return TRANS_TO_AGENT_MENU;
		}
		else if (result == GAME_UI_NEXT_LEVEL)
		{
			return TRANS_NEXT_LEVEL;
		}
		else if (result == GAME_UI_SELECT_LEVEL)
		{
			return TRANS_START_LEVEL;
		}

		// If draw game isn't set, then we cannot use the escape menu.
		if (!escMenuOpen && shouldDrawGame && TFE_Input::keyPressed(KEY_ESCAPE) && s_inputDelay <= 0)
		{
			TFE_GameUi::openEscMenu();
		}

		LightMode mode = LIGHT_OFF;
		if (shouldDrawGame)
		{
			if (s_player.m_shooting) { mode = LIGHT_BRIGHT; }
			else if (s_player.m_headlampOn) { mode = LIGHT_NORMAL; }
			// If the game should not be updated, the view still needs updating to avoid visual glitches.
			// Otherwise it will be updated after the player input and physics.
			if (!shouldUpdateGame)
			{
				TFE_View::update(&s_cameraPos, s_player.m_yaw, s_player.m_pitch, s_player.m_sectorId, mode);
			}
		}
		if (!shouldUpdateGame)
		{
			return trans;
		}

		// Then handle player update and prepare to draw.
		s_player.m_onScrollFloor = false;
		s32 playerFloorCount = 0, playerSecAltCount = 0;
		s32 playerFloor[256], playerSecAlt[256];
		// On the floor or second height.
		// Go through sectors that the player is touching and figure out which ones they are standing on.
		u32 overlapSectorCount = 0;
		const Sector** overlapSectors = TFE_Physics::getOverlappingSectors(&s_player.pos, s_player.m_sectorId, c_playerRadius, &overlapSectorCount);
		for (u32 i = 0; i < overlapSectorCount; i++)
		{
			if (fabsf(s_player.pos.y - TFE_Level::getFloorHeight(overlapSectors[i]->id)) <= 0.001f && playerFloorCount < 256)
			{
				playerFloor[playerFloorCount++] = overlapSectors[i]->id;
			}
			if (fabsf(s_player.pos.y - TFE_Level::getSecondHeight(overlapSectors[i]->id) - TFE_Level::getFloorHeight(overlapSectors[i]->id)) <= 0.001f && playerSecAltCount < 256)
			{
				playerSecAlt[playerSecAltCount++] = overlapSectors[i]->id;
			}
		}
		TFE_Level::setPlayerSector(&s_player, playerFloorCount, playerSecAltCount, playerFloor, playerSecAlt);
		// Update INF 
		TFE_InfSystem::tick();
		// Before handling player movement - if the environment has changed correct the player position.
		s32 prevSectorId = s_player.m_sectorId;
		if (TFE_Physics::correctPosition(&s_player.pos, &s_player.m_sectorId, c_playerRadius))
		{
			if (prevSectorId != s_player.m_sectorId && s_player.m_sectorId >= 0)
			{
				TFE_InfSystem::firePlayerEvent(INF_EVENT_ENTER_SECTOR, s_player.m_sectorId, &s_player);
				TFE_InfSystem::firePlayerEvent(INF_EVENT_LEAVE_SECTOR, prevSectorId, &s_player);
			}
		}
		if (prevSectorId >= 0 && s_player.m_sectorId < 0)
		{
			s_player.m_sectorId = prevSectorId;
		}

		const f32 dt = (f32)TFE_System::getDeltaTime();
		const f32 turnSpeed = 2.1f;

		// hack in some controls.
		if (TFE_Input::keyDown(KEY_LEFT) && s_inputDelay <= 0)
		{
			s_player.m_yaw += turnSpeed * dt;
			if (s_player.m_yaw >= 2.0f*PI) { s_player.m_yaw -= 2.0f*PI; }
		}
		else if (TFE_Input::keyDown(KEY_RIGHT) && s_inputDelay <= 0)
		{
			s_player.m_yaw -= turnSpeed * dt;
			if (s_player.m_yaw < 0.0f) { s_player.m_yaw += 2.0f*PI; }
		}

		if (TFE_Input::relativeModeEnabled() && s_inputDelay <= 0)
		{
			s32 mdx, mdy;
			TFE_Input::getMouseMove(&mdx, &mdy);
			if (mdx)
			{
				const f32 mouseYawSpeed = PI * 0.5f / 640.0f;
				s_player.m_yaw -= mouseYawSpeed * f32(mdx);

				if (s_player.m_yaw < 0.0f) { s_player.m_yaw += 2.0f*PI; }
				if(s_player.m_yaw >= 2.0f*PI) { s_player.m_yaw -= 2.0f*PI; }
			}

			// If mouse look enabled.
			if (mdy)
			{
				const f32 mousePitchSpeed = PI * 0.25f / 640.0f;
				s_player.m_pitch -= mousePitchSpeed * f32(mdy);

				if (s_player.m_pitch < -c_pitchLimit) { s_player.m_pitch = -c_pitchLimit; }
				if (s_player.m_pitch >= c_pitchLimit) { s_player.m_pitch = c_pitchLimit; }
			}
		}

		if (TFE_Input::keyDown(KEY_PAGEUP) && s_inputDelay <= 0)
		{
			s_player.m_pitch += turnSpeed * dt;
			if (s_player.m_pitch >= c_pitchLimit) { s_player.m_pitch = c_pitchLimit; }
		}
		else if (TFE_Input::keyDown(KEY_PAGEDOWN) && s_inputDelay <= 0)
		{
			s_player.m_pitch -= turnSpeed * dt;
			if (s_player.m_pitch < -c_pitchLimit) { s_player.m_pitch = -c_pitchLimit; }
		}
		else if (TFE_Input::keyDown(KEY_END) && s_inputDelay <= 0)
		{
			s_player.m_pitch = 0.0f;
		}

		s_crouching = TFE_Input::keyDown(KEY_LCTRL) || s_forceCrouch;
		if (s_crouching)
		{
			s_eyeHeight += c_crouchOnSpeed * dt;
			if (s_eyeHeight > c_crouchingEyeHeight) { s_eyeHeight = c_crouchingEyeHeight; }
			s_height = c_crouchingHeight;
		}
		else
		{
			s_eyeHeight -= c_crouchOffSpeed * dt;
			if (s_eyeHeight < c_standingEyeHeight) { s_eyeHeight = c_standingEyeHeight; }
			s_height = c_standingHeight;
		}

		// Use
		if (TFE_Input::keyPressed(KEY_E) && s_inputDelay <= 0)
		{
			const Vec3f forwardDir = { -sinf(s_player.m_yaw), 0.0f, cosf(s_player.m_yaw) };
			// Fire a short ray into the world and gather all of the lines it hits until a solid wall is reached or the ray terminates.
			// Then we will process the list in order from closest to farthest until the activate() is consumed or the list is exhausted.
			const Ray ray =
			{
				{s_player.pos.x, s_player.pos.y + s_eyeHeight, s_player.pos.z},
				forwardDir,
				s_player.m_sectorId,
				3.5f,
				false,
			};
			MultiRayHitInfo hitInfo;
			TFE_Physics::traceRayMulti(&ray, &hitInfo);
			TFE_InfSystem::activate(&s_player.pos, &hitInfo, s_player.m_sectorId, s_player.m_keys);
		}

		// Weapons
		if (TFE_Input::keyPressed(KEY_1))
		{
			TFE_WeaponSystem::switchToWeapon(WEAPON_PUNCH);
		}
		else if (TFE_Input::keyPressed(KEY_2))
		{
			TFE_WeaponSystem::switchToWeapon(WEAPON_PISTOL);
		}
		else if (TFE_Input::keyPressed(KEY_3))
		{
			TFE_WeaponSystem::switchToWeapon(WEAPON_RIFLE);
		}
		else if (TFE_Input::keyPressed(KEY_4))
		{
			TFE_WeaponSystem::switchToWeapon(WEAPON_THERMAL_DETONATOR);
		}
				
		// get the desired movement.
		// Get the floor sector first at the same time as getting the speed info.
		bool inAir = false;
		const f32 speed = getSpeed(&inAir);

		Vec3f move = { 0 };
		const Vec2f forwardDir = { -sinf(s_player.m_yaw), cosf(s_player.m_yaw) };
		const Vec2f forward = { forwardDir.x * speed, forwardDir.z * speed };
		const Vec2f right = { -forward.z, forward.x };
				
		if (TFE_Input::keyDown(KEY_W) && s_inputDelay <= 0)
		{
			move.x += forward.x;
			move.z += forward.z;
		}
		else if (TFE_Input::keyDown(KEY_S) && s_inputDelay <= 0)
		{
			move.x -= forward.x;
			move.z -= forward.z;
		}
		if (TFE_Input::keyDown(KEY_A) && s_inputDelay <= 0)
		{
			move.x += right.x;
			move.z += right.z;
		}
		else if (TFE_Input::keyDown(KEY_D) && s_inputDelay <= 0)
		{
			move.x -= right.x;
			move.z -= right.z;
		}
				
		// Adjust the velocity based on move.
		s_player.vel = changeVelocity(&s_player.vel, &move, inAir, dt);

		// TODO: Add conveyors, current, etc.

		// Now the move is the final velocity, factoring in frame time.
		move.x = s_player.vel.x * dt;
		move.z = s_player.vel.z * dt;

		// then apply to the physics system and get the actual movement (if any).
		Vec3f startPos = { s_player.pos.x, s_player.pos.y, s_player.pos.z };
		Vec3f actualMove;
		s32 newSectorId;
		prevSectorId = s_player.m_sectorId;
		bool moveMatches = true;
		if (TFE_Physics::move(&startPos, &move, s_player.m_sectorId, &actualMove, &newSectorId, s_height))
		{
			s_player.pos.x += actualMove.x;
			s_player.pos.y += actualMove.y;
			s_player.pos.z += actualMove.z;

			f32 dx = fabsf(actualMove.x - move.x);
			f32 dz = fabsf(actualMove.z - move.z);
			if (dx > 0.00001f || dz > 0.00001f)
			{
				moveMatches = false;
			}

			s_player.m_sectorId = newSectorId;
			if (prevSectorId != newSectorId)
			{
				TFE_InfSystem::firePlayerEvent(INF_EVENT_ENTER_SECTOR, newSectorId, &s_player);
				TFE_InfSystem::firePlayerEvent(INF_EVENT_LEAVE_SECTOR, prevSectorId, &s_player);
			}
		}
		f32 floorHeight, ceilHeight, visualFloorHeight;
		TFE_Physics::getValidHeightRange(&s_player.pos, s_player.m_sectorId, &floorHeight, &visualFloorHeight, &ceilHeight);
		s_forceCrouch = (floorHeight - ceilHeight < c_standingHeight);
		s_player.pos.y = std::min(floorHeight, s_player.pos.y);
				
		if (TFE_Input::keyPressed(KEY_SPACE) && s_player.pos.y == floorHeight && s_player.vel.y == 0.0f)
		{
			s_jump = true;
			s_player.vel.y += c_jumpImpulse;
			TFE_Audio::playOneShot(SOUND_2D, 1.0f, MONO_SEPERATION, s_playerSounds.jump, false);
		}

		// Gravity.
		if (s_player.pos.y < floorHeight)
		{
			s_player.vel.y += c_gravityAccel * dt;
		}
		else if (s_player.vel.y > 0.0f)
		{
			// Send INF event
			if (!TFE_InfSystem::firePlayerEvent(INF_EVENT_LAND, s_player.m_sectorId, &s_player))
			{
				s_jump = false;

				// Land.
				s_landDist = s_fallHeight * 0.075f;
				s_landAnim = 0.0f;
				s_land = true;

				if (s_fallHeight > 3.0f)
				{
					// Is this water?
					const bool isWater = s_level->sectors[s_player.m_sectorId].secAlt > 0.0f;
					TFE_Audio::playOneShot(SOUND_2D, 1.0f, MONO_SEPERATION, isWater ? s_playerSounds.landWater : s_playerSounds.land, false);
				}

				// Clear Velocity and fall height.
				s_fallHeight = 0.0f;
				s_player.vel.y = 0.0f;
			}
			else
			{
				s_player.vel.y += c_gravityAccel * dt;
				TFE_Physics::getValidHeightRange(&s_player.pos, s_player.m_sectorId, &floorHeight, &visualFloorHeight, &ceilHeight);
			}
		}

		f32 dY = handleLandAnim(dt);

		// Handle vertical velocity.
		if (s_player.vel.y > 0.0f)
		{
			f32 dy = s_player.vel.y*dt;
			if (s_player.pos.y + dy >= floorHeight)
			{
				dy = floorHeight - s_player.pos.y;
			}
			if (s_fallHeight < c_yellThreshold && s_fallHeight + dy >= c_yellThreshold)
			{
				TFE_Audio::playOneShot(SOUND_2D, 1.0f, MONO_SEPERATION, s_playerSounds.fall, false);
			}
			s_fallHeight += dy;
		}
		s_player.pos.y = std::min(s_player.pos.y + s_player.vel.y*dt, floorHeight);

		if (s_player.pos.y - s_height < ceilHeight)
		{
			s_player.pos.y = ceilHeight + s_height;
			s_player.vel.y = 0.0f;
		}
		// Make sure the camera cannot go below the visual height.
		if (s_player.pos.y + s_eyeHeight > visualFloorHeight - 0.625f)
		{
			s_eyeHeight = visualFloorHeight - 0.625f - s_player.pos.y;
		}

		// How fast is the player moving?
		move.x = s_player.pos.x - startPos.x;
		move.z = s_player.pos.z - startPos.z;
		f32 actualSpeed = dt ? sqrtf(move.x*move.x + move.z*move.z) / dt : 0.0f;
		// Adjust velocity based on collisions.
		if (!moveMatches)
		{
			Vec2f hVel = { move.x, move.z };
			hVel = TFE_Math::normalize(&hVel);
			Vec3f newVel = { hVel.x * actualSpeed, s_player.vel.y, hVel.z * actualSpeed };
			s_player.vel = newVel;
		}
		// Motion for head and weapon movement.
		f32 blend = std::min(1.0f, dt * 10.0f);
		if (s_player.m_onScrollFloor) { actualSpeed *= 2.0f; }
		s_motion = std::max(0.0f, std::min((actualSpeed / c_walkSpeed)*blend + s_motion*(1.0f - blend), 1.0f));
		if (actualSpeed < FLT_EPSILON && s_motion < 0.0001f) { s_motion = 0.0f; }
		
		// Items.
		if (TFE_Input::keyPressed(KEY_F2))
		{
			s_player.m_nightVisionOn = !s_player.m_nightVisionOn;
			TFE_RenderCommon::enableNightVision(s_player.m_nightVisionOn);
		}
		else if (TFE_Input::keyPressed(KEY_F5))
		{
			s_player.m_headlampOn = !s_player.m_headlampOn;
		}
		
		// DEBUG
		if (TFE_Input::keyPressed(KEY_RIGHTBRACKET))
		{
			s_iterOverride = std::min(s_iterOverride + 1, 256);
		}
		else if (TFE_Input::keyPressed(KEY_LEFTBRACKET))
		{
			s_iterOverride = std::max(s_iterOverride - 1, 0);
		}
		TFE_View::setIterationOverride(s_iterOverride);

		s_heightVisual = (s_heightVisual + s_player.pos.y) * 0.5f;
		s_cameraPos = { s_player.pos.x, std::min(s_heightVisual + s_eyeHeight + dY, floorHeight - 0.5f), s_player.pos.z };
		// Check the current sector and lower the camera to fit.
		if (s_cameraPos.y < s_level->sectors[s_player.m_sectorId].ceilAlt + 0.2f)
		{
			s_cameraPos.y = std::min(s_level->sectors[s_player.m_sectorId].floorAlt - 0.1f, s_level->sectors[s_player.m_sectorId].ceilAlt + 0.2f);
		}
		s_motionTime += 1.2f * (f32)TFE_System::getDeltaTime();
		if (s_motionTime > 1.0f) { s_motionTime -= 1.0f; }
		s_cameraPos.y += cosf(s_motionTime * 2.0f * PI) * s_motion * 0.5f;
						
		TFE_View::update(&s_cameraPos, s_player.m_yaw, s_player.m_pitch, s_player.m_sectorId, mode);
		TFE_GameHud::update(&s_player);
		TFE_LogicSystem::update();
		TFE_WeaponSystem::update(s_motion, &s_player);

		// Update objects based on their physics settings (to handle explosions, gravity, bouncing, etc.).
		updateObjects();
		updateSoundObjects(&s_cameraPos);

		if (TFE_Input::mouseDown(MBUTTON_LEFT) && s_inputDelay <= 0)
		{
			TFE_WeaponSystem::shoot(&s_player, &forwardDir);
		}
		else
		{
			TFE_WeaponSystem::release();
		}

		const Vec3f flattenedForward = { forwardDir.x, 0.0f, forwardDir.z };
		TFE_Audio::update(&s_cameraPos, &flattenedForward);
		if (s_inputDelay > 0) { s_inputDelay--; }

		return trans;
	}
		
	void draw()
	{
		if (TFE_GameUi::shouldDrawGame())
		{
			TFE_View::draw(&s_cameraPos, s_player.m_sectorId);
			TFE_WeaponSystem::draw(&s_player, &s_cameraPos, s_player.m_headlampOn ? 31 : s_level->sectors[s_player.m_sectorId].ambient);
			TFE_GameHud::draw(&s_player);
		}
		TFE_GameUi::draw(&s_player);
	}

	void drawModel(const Model* model, const Vec3f* orientation)
	{
		if (!model) { return; }
		
		Vec3f modelPos = { 0.0f, 0.0f, 0.0f };
		Vec3f cameraPos = { model->center.x, -model->center.y, model->center.z - model->radius*1.5f };
		f32 cameraRot[] = { 1.0f, 0.0f };

		TFE_ModelRender::draw(model, orientation, &modelPos, nullptr, &cameraPos, cameraRot, 0);
	}

	//////////////////////////////////////////////
	// Internal
	//////////////////////////////////////////////
	void updateObjects()
	{
		GameObject* objects = LevelGameObjects::getGameObjectList()->data();
		SectorObjectList* sectorObjects = LevelGameObjects::getSectorObjectList();
		const u32 secCount = (u32)s_level->sectors.size();

		s_accum += (f32)TFE_System::getDeltaTime();
		while (s_accum >= c_step)
		{
			Sector* sector = s_level->sectors.data();
			for (u32 s = 0; s < secCount; s++, sector++)
			{
				const std::vector<u32>& list = (*sectorObjects)[s].list;
				const u32 objCount = (u32)list.size();
				const u32* indices = list.data();
				for (u32 i = 0; i < objCount; i++)
				{
					GameObject* obj = &objects[indices[i]];
					// Is the object affected by gravity?
					if (!(obj->physicsFlags&PHYSICS_GRAVITY)) { obj->verticalVel = 0.0f; continue; }

					// Is the object close enough to stick to the floor or second alt?
					const f32 dFloor = fabsf(obj->pos.y - sector->floorAlt);
					const f32 dSec   = fabsf(obj->pos.y - sector->floorAlt - std::min(sector->secAlt, 0.0f));
					if (dSec < 0.1f && sector->secAlt < 0.0f) { obj->pos.y = sector->floorAlt + sector->secAlt; obj->verticalVel = 0.0f; continue; }
					else if (dFloor < 0.1f) { obj->pos.y = sector->floorAlt; obj->verticalVel = 0.0f; continue; }

					// The object should fall towards the floor or second height.
					const bool aboveSecHeight = sector->secAlt < 0.0f && obj->pos.y < sector->floorAlt + sector->secAlt + 0.1f;
					const f32 floorHeight = aboveSecHeight ? sector->floorAlt + sector->secAlt : sector->floorAlt;

					obj->pos.y += obj->verticalVel * c_step;
					if (obj->pos.y >= floorHeight)
					{
						obj->verticalVel = 0.0f;
						obj->pos.y = floorHeight;
					}
					else
					{
						obj->verticalVel += c_gravityAccelStep;
					}
				}
			}

			s_accum -= c_step;
		}
	}

	// Go through level objects and add or remove sound sources based on proximity to the listener.
	void updateSoundObjects(const Vec3f* listenerPos)
	{
		const u32 count = (u32)LevelGameObjects::getGameObjectList()->size();
		const f32 borderSq = 16.0f * 16.0f;
		const f32 soundMaxDistSq = TFE_Audio::c_clipDistance * TFE_Audio::c_clipDistance;

		GameObject* object = LevelGameObjects::getGameObjectList()->data();
		for (u32 i = 0; i < count; i++, object++)
		{
			if (object->oclass != CLASS_SOUND || !object->buffer) { continue; }

			const Vec3f offset = { object->pos.x - listenerPos->x, object->pos.y - listenerPos->y, object->pos.z - listenerPos->z };
			const f32 distSq = TFE_Math::dot(&offset, &offset);

			if (distSq <= soundMaxDistSq && !object->source)
			{
				// Add a new looping 3D source.
				object->source = TFE_Audio::createSoundSource(SOUND_3D, 1.0f, MONO_SEPERATION, object->buffer, &object->pos);
				if (object->source)
				{
					TFE_Audio::playSource(object->source, true);
				}
			}
			else if (distSq > soundMaxDistSq + borderSq && object->source)
			{
				// Remove the looping 3D source. Note a border area is added so we aren't constantly adding and removing sounds with small movements.
				TFE_Audio::freeSource(object->source);
				object->source = nullptr;
			}
		}
	}
}

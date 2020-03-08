#include "gameLoop.h"
#include "modelRendering.h"
#include "renderCommon.h"
#include "player.h"
#include "physics.h"
#include "view.h"
#include "gameHud.h"
#include <DXL2_Game/geometry.h>
#include <DXL2_Game/player.h>
#include <DXL2_Game/level.h>
#include <DXL2_Game/gameObject.h>
#include <DXL2_System/system.h>
#include <DXL2_System/math.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Input/input.h>
#include <DXL2_Asset/levelAsset.h>
#include <DXL2_Asset/levelObjectsAsset.h>
#include <DXL2_Asset/infAsset.h>
#include <DXL2_Asset/modelAsset.h>
#include <DXL2_InfSystem/infSystem.h>
#include <DXL2_LogicSystem/logicSystem.h>
#include <DXL2_WeaponSystem/weaponSystem.h>
#include <assert.h>
#include <vector>
#include <algorithm>

namespace DXL2_GameLoop
{
	const f32 c_walkSpeed = 32.0f;
	const f32 c_runSpeed = 64.0f;
	const f32 c_crouchWalk = 17.0f;
	const f32 c_crouchRun = 34.0f;

	const f32 c_walkSpeedJump = 22.50f;
	const f32 c_runSpeedJump = 45.00f;
	const f32 c_crouchWalkJump = 11.95f;
	const f32 c_crouchRunJump = 23.91f;

	const f32 c_jumpImpulse = -36.828f;
	const f32 c_gravityAccel = 111.6f;
	
	static LevelData* s_level = nullptr;
	static LevelObjectData* s_levelObjects = nullptr;
	static Player s_player;
	static const f32 c_standingEyeHeight  = -46.0f * 0.125f;  // Kyle's standing eye height is 46 texels.
	static const f32 c_crouchingEyeHeight = -16.0f * 0.125f;  // Kyle's crouching eye height is 16 texels.
	static const f32 c_standingHeight = 6.8f;
	static const f32 c_crouchingHeight = 3.0f;
	static const f32 c_crouchOnSpeed  = 12.0f;	// DFU's per second.
	static const f32 c_crouchOffSpeed = 12.0f;
	static bool s_crouching = false;
	static f32 s_heightVisual;
	static f32 s_eyeHeight;
	static f32 s_height;
	static f32 s_motion;
	static f32 s_motionTime;
	static f32 s_fallHeight;
	static f32 s_landDist = 0.0f;
	static f32 s_landAnim = 0.0f;
	static bool s_land = false;
	static bool s_jump = false;

	static f32 s_accum = 0.0f;
	static f32 c_step = 1.0f / 60.0f;

	void updateObjects();

	void startRenderer(DXL2_Renderer* renderer, s32 w, s32 h)
	{
		DXL2_View::init(nullptr, renderer, w, h, false);
	}

	bool startLevelFromExisting(const Vec3f* startPos, f32 yaw, s32 startSectorId, const Palette256* pal, LevelObjectData* levelObj, DXL2_Renderer* renderer, s32 w, s32 h)
	{
		if (!startPos || !renderer || startSectorId < 0) { return false; }

		LevelData* level = DXL2_LevelAsset::getLevelData();
		InfData* infData = DXL2_InfAsset::getInfData();
		if (!level || !infData) { return false; }
		s_level = level;

		// Initialize the Logic system.
		DXL2_LogicSystem::init(&s_player);

		DXL2_Physics::init(s_level);
		DXL2_Level::startLevel(level, levelObj);
		s_motion = 0.0f;
		s_motionTime = 0.0f;
		s_fallHeight = 0.0f;
		s_landDist = 0.0f;
		s_landAnim = 0.0f;
		s_land = false;
		s_jump = false;
				
		// Current no objects...
		s_levelObjects = levelObj;

		DXL2_InfSystem::setupLevel(DXL2_InfAsset::getInfData(), level);

		// Setup the player.
		s_player.m_sectorId = startSectorId;
		s_player.pos = *startPos;
		s_player.vel = { 0 };
		s_player.m_yaw = yaw;
		s_player.m_pitch = 0;
		s_player.m_headlampOn = false;
		s_player.m_keys = 0;
		s_heightVisual = s_player.pos.y;

		DXL2_View::init(level, renderer, w, h, false);
		renderer->changeResolution(w, h);

		s_eyeHeight = c_standingEyeHeight;
		s_height = c_standingHeight;
		s_crouching = false;

		DXL2_GameHud::init(renderer);
		DXL2_WeaponSystem::init(renderer);

		// For now switch over to the pistol.
		DXL2_WeaponSystem::switchToWeapon(WEAPON_PISTOL);
		s_accum = 0.0f;

		return true;
	}

	bool startLevel(LevelData* level, const StartLocation& start, DXL2_Renderer* renderer, s32 w, s32 h, bool enableViewStats)
	{
		if (!level) { return false; }
		
		s_level = level;
		s_motion = 0.0f;
		s_motionTime = 0.0f;
		s_fallHeight = 0.0f;
		s_landDist = 0.0f;
		s_landAnim = 0.0f;
		s_land = false;
		s_jump = false;

		// Initialize the Logic system.
		DXL2_LogicSystem::init(&s_player);

		// Load INF
		char infFile[DXL2_MAX_PATH];
		size_t nameLen = strlen(level->palette);
		strcpy(infFile, level->palette);
		infFile[nameLen - 3] = 'i';
		infFile[nameLen - 2] = 'n';
		infFile[nameLen - 1] = 'f';
		infFile[nameLen - 0] = 0;
		DXL2_InfAsset::load(infFile);
		
		// Load objects.
		char oFile[DXL2_MAX_PATH];
		strcpy(oFile, level->palette);
		oFile[nameLen - 3] = 'O';
		oFile[nameLen - 2] = 0;

		s_levelObjects = nullptr;
		if (DXL2_LevelObjects::load(oFile))
		{
			s_levelObjects = DXL2_LevelObjects::getLevelObjectData();
		}
		if (!s_levelObjects) { return false; }

		DXL2_Physics::init(s_level);
		DXL2_Level::startLevel(level, s_levelObjects);

		DXL2_InfSystem::setupLevel(DXL2_InfAsset::getInfData(), level);

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

					sectorId = DXL2_Physics::findSector(&startPos);

					yaw = 2.0f * PI - 2.0f * PI * s_levelObjects->objects[i].orientation.y / 360.0f;
					break;
				}
			}
		}
		else
		{
			sectorId = DXL2_Physics::findSector(start.layer, &start.pos);
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
		s_player.m_keys = 0;
		s_heightVisual = s_player.pos.y;
			   			   
		DXL2_View::init(level, renderer, w, h, enableViewStats);
		renderer->changeResolution(w, h);

		s_eyeHeight = c_standingEyeHeight;
		s_height = c_standingHeight;
		s_crouching = false;

		DXL2_GameHud::init(renderer);
		DXL2_WeaponSystem::init(renderer);

		// For now switch over to the pistol.
		DXL2_WeaponSystem::switchToWeapon(WEAPON_PISTOL);
		s_accum = 0.0f;
		
		return true;
	}

	void endLevel()
	{
		s_level = nullptr;
		DXL2_Level::endLevel();
	}

	const ViewStats* getViewStats()
	{
		return DXL2_View::getViewStats();
	}

	static s32 s_iterOverride = 0;
	static const f32 c_pitchLimit = 0.78539816339744830962f;	// +/- 45 degrees.
	static Vec3f s_cameraPos;
	static bool s_forceCrouch = false;
	static bool s_slowToggle = false;
	
	f32 getSpeed(bool* inAir)
	{
		f32 floorHeight, ceilHeight, visualFloorHeight;
		DXL2_Physics::getValidHeightRange(&s_player.pos, s_player.m_sectorId, &floorHeight, &visualFloorHeight, &ceilHeight);

		// Running?
		bool running = DXL2_Input::keyDown(KEY_LSHIFT) || DXL2_Input::keyDown(KEY_RSHIFT);
		if (DXL2_Input::keyPressed(KEY_CAPSLOCK))
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
		const f32 lenSq = DXL2_Math::dot(&diff, &diff);
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

	void update()
	{
		s_player.m_onScrollFloor = false;
		s32 playerFloorCount = 0, playerSecAltCount = 0;
		s32 playerFloor[256], playerSecAlt[256];
		// On the floor or second height.
		// Go through sectors that the player is touching and figure out which ones they are standing on.
		u32 overlapSectorCount = 0;
		const Sector** overlapSectors = DXL2_Physics::getOverlappingSectors(&s_player.pos, s_player.m_sectorId, c_playerRadius, &overlapSectorCount);
		for (u32 i = 0; i < overlapSectorCount; i++)
		{
			if (fabsf(s_player.pos.y - DXL2_Level::getFloorHeight(overlapSectors[i]->id)) <= 0.001f && playerFloorCount < 256)
			{
				playerFloor[playerFloorCount++] = overlapSectors[i]->id;
			}
			if (fabsf(s_player.pos.y - DXL2_Level::getSecondHeight(overlapSectors[i]->id) - DXL2_Level::getFloorHeight(overlapSectors[i]->id)) <= 0.001f && playerSecAltCount < 256)
			{
				playerSecAlt[playerSecAltCount++] = overlapSectors[i]->id;
			}
		}
		DXL2_Level::setPlayerSector(&s_player, playerFloorCount, playerSecAltCount, playerFloor, playerSecAlt);
		// Update INF 
		DXL2_InfSystem::tick();
		// Before handling player movement - if the environment has changed correct the player position.
		s32 prevSectorId = s_player.m_sectorId;
		if (DXL2_Physics::correctPosition(&s_player.pos, &s_player.m_sectorId, c_playerRadius))
		{
			if (prevSectorId != s_player.m_sectorId && s_player.m_sectorId >= 0)
			{
				DXL2_InfSystem::firePlayerEvent(INF_EVENT_ENTER_SECTOR, s_player.m_sectorId, &s_player);
				DXL2_InfSystem::firePlayerEvent(INF_EVENT_LEAVE_SECTOR, prevSectorId, &s_player);
			}
		}
		if (prevSectorId >= 0 && s_player.m_sectorId < 0)
		{
			s_player.m_sectorId = prevSectorId;
		}

		const f32 dt = (f32)DXL2_System::getDeltaTime();
		const f32 turnSpeed = 2.1f;

		// hack in some controls.
		if (DXL2_Input::keyDown(KEY_LEFT))
		{
			s_player.m_yaw += turnSpeed * dt;
			if (s_player.m_yaw >= 2.0f*PI) { s_player.m_yaw -= 2.0f*PI; }
		}
		else if (DXL2_Input::keyDown(KEY_RIGHT))
		{
			s_player.m_yaw -= turnSpeed * dt;
			if (s_player.m_yaw < 0.0f) { s_player.m_yaw += 2.0f*PI; }
		}

		if (DXL2_Input::relativeModeEnabled())
		{
			s32 mdx, mdy;
			DXL2_Input::getMouseMove(&mdx, &mdy);
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

		if (DXL2_Input::keyDown(KEY_PAGEUP))
		{
			s_player.m_pitch += turnSpeed * dt;
			if (s_player.m_pitch >= c_pitchLimit) { s_player.m_pitch = c_pitchLimit; }
		}
		else if (DXL2_Input::keyDown(KEY_PAGEDOWN))
		{
			s_player.m_pitch -= turnSpeed * dt;
			if (s_player.m_pitch < -c_pitchLimit) { s_player.m_pitch = -c_pitchLimit; }
		}
		else if (DXL2_Input::keyDown(KEY_END))
		{
			s_player.m_pitch = 0.0f;
		}

		s_crouching = DXL2_Input::keyDown(KEY_LCTRL) || s_forceCrouch;
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
		if (DXL2_Input::keyPressed(KEY_E))
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
			DXL2_Physics::traceRayMulti(&ray, &hitInfo);
			DXL2_InfSystem::activate(&s_player.pos, &hitInfo, s_player.m_sectorId, s_player.m_keys);
		}

		// Weapons
		if (DXL2_Input::keyPressed(KEY_1))
		{
			DXL2_WeaponSystem::switchToWeapon(WEAPON_PUNCH);
		}
		else if (DXL2_Input::keyPressed(KEY_2))
		{
			DXL2_WeaponSystem::switchToWeapon(WEAPON_PISTOL);
		}
		else if (DXL2_Input::keyPressed(KEY_3))
		{
			DXL2_WeaponSystem::switchToWeapon(WEAPON_RIFLE);
		}
				
		// get the desired movement.
		// Get the floor sector first at the same time as getting the speed info.
		bool inAir = false;
		const f32 speed = getSpeed(&inAir);

		Vec3f move = { 0 };
		const Vec2f forwardDir = { -sinf(s_player.m_yaw), cosf(s_player.m_yaw) };
		const Vec2f forward = { forwardDir.x * speed, forwardDir.z * speed };
		const Vec2f right = { -forward.z, forward.x };
				
		if (DXL2_Input::keyDown(KEY_W))
		{
			move.x += forward.x;
			move.z += forward.z;
		}
		else if (DXL2_Input::keyDown(KEY_S))
		{
			move.x -= forward.x;
			move.z -= forward.z;
		}
		if (DXL2_Input::keyDown(KEY_A))
		{
			move.x += right.x;
			move.z += right.z;
		}
		else if (DXL2_Input::keyDown(KEY_D))
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
		if (DXL2_Physics::move(&startPos, &move, s_player.m_sectorId, &actualMove, &newSectorId, s_height))
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
				DXL2_InfSystem::firePlayerEvent(INF_EVENT_ENTER_SECTOR, newSectorId, &s_player);
				DXL2_InfSystem::firePlayerEvent(INF_EVENT_LEAVE_SECTOR, prevSectorId, &s_player);
			}
		}
		f32 floorHeight, ceilHeight, visualFloorHeight;
		DXL2_Physics::getValidHeightRange(&s_player.pos, s_player.m_sectorId, &floorHeight, &visualFloorHeight, &ceilHeight);
		s_forceCrouch = (floorHeight - ceilHeight < c_standingHeight);
		s_player.pos.y = std::min(floorHeight, s_player.pos.y);
				
		if (DXL2_Input::keyPressed(KEY_SPACE) && s_player.pos.y == floorHeight && s_player.vel.y == 0.0f)
		{
			s_jump = true;
			s_player.vel.y += c_jumpImpulse;
		}

		// Gravity.
		if (s_player.pos.y < floorHeight)
		{
			s_player.vel.y += c_gravityAccel * dt;
		}
		else if (s_player.vel.y > 0.0f)
		{
			// Send INF event
			if (!DXL2_InfSystem::firePlayerEvent(INF_EVENT_LAND, s_player.m_sectorId, &s_player))
			{
				s_jump = false;

				// Land.
				s_landDist = s_fallHeight * 0.075f;
				s_landAnim = 0.0f;
				s_land = true;

				// Clear Velocity and fall height.
				s_fallHeight = 0.0f;
				s_player.vel.y = 0.0f;
			}
			else
			{
				s_player.vel.y += c_gravityAccel * dt;
				DXL2_Physics::getValidHeightRange(&s_player.pos, s_player.m_sectorId, &floorHeight, &visualFloorHeight, &ceilHeight);
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
			hVel = DXL2_Math::normalize(&hVel);
			Vec3f newVel = { hVel.x * actualSpeed, s_player.vel.y, hVel.z * actualSpeed };
			s_player.vel = newVel;
		}
		// Motion for head and weapon movement.
		f32 blend = std::min(1.0f, dt * 10.0f);
		if (s_player.m_onScrollFloor) { actualSpeed *= 2.0f; }
		s_motion = std::max(0.0f, std::min((actualSpeed / c_walkSpeed)*blend + s_motion*(1.0f - blend), 1.0f));
		if (actualSpeed < FLT_EPSILON && s_motion < 0.0001f) { s_motion = 0.0f; }
		
		// Items.
		if (DXL2_Input::keyPressed(KEY_F5))
		{
			s_player.m_headlampOn = !s_player.m_headlampOn;
		}

		// DEBUG
		if (DXL2_Input::keyPressed(KEY_RIGHTBRACKET))
		{
			s_iterOverride = std::min(s_iterOverride + 1, 256);
		}
		else if (DXL2_Input::keyPressed(KEY_LEFTBRACKET))
		{
			s_iterOverride = std::max(s_iterOverride - 1, 0);
		}
		DXL2_View::setIterationOverride(s_iterOverride);

		s_heightVisual = (s_heightVisual + s_player.pos.y) * 0.5f;
		s_cameraPos = { s_player.pos.x, std::min(s_heightVisual + s_eyeHeight + dY, floorHeight - 0.5f), s_player.pos.z };
		// Check the current sector and lower the camera to fit.
		if (s_cameraPos.y < s_level->sectors[s_player.m_sectorId].ceilAlt + 0.2f)
		{
			s_cameraPos.y = std::min(s_level->sectors[s_player.m_sectorId].floorAlt - 0.1f, s_level->sectors[s_player.m_sectorId].ceilAlt + 0.2f);
		}
		s_motionTime += 1.2f * (f32)DXL2_System::getDeltaTime();
		if (s_motionTime > 1.0f) { s_motionTime -= 1.0f; }
		s_cameraPos.y += cosf(s_motionTime * 2.0f * PI) * s_motion * 0.5f;

		LightMode mode = LIGHT_OFF;
		if (s_player.m_shooting) { mode = LIGHT_BRIGHT; }
		else if (s_player.m_headlampOn) { mode = LIGHT_NORMAL; }
				
		DXL2_View::update(&s_cameraPos, s_player.m_yaw, s_player.m_pitch, s_player.m_sectorId, mode);
		DXL2_GameHud::update(&s_player);
		DXL2_LogicSystem::update();
		DXL2_WeaponSystem::update(s_motion, &s_player);

		// Update objects based on their physics settings (to handle explosions, gravity, bouncing, etc.).
		updateObjects();

		if (DXL2_Input::mouseDown(MBUTTON_LEFT))
		{
			DXL2_WeaponSystem::shoot(&s_player, &forwardDir);
		}
	}
		
	void draw()
	{
		DXL2_View::draw(&s_cameraPos, s_player.m_sectorId);
		DXL2_WeaponSystem::draw(&s_player, &s_cameraPos, s_player.m_headlampOn ? 31 : s_level->sectors[s_player.m_sectorId].ambient);
		DXL2_GameHud::draw(&s_player);
	}

	void drawModel(const Model* model, const Vec3f* orientation)
	{
		if (!model) { return; }
		
		Vec3f modelPos = { 0.0f, 0.0f, 0.0f };
		Vec3f cameraPos = { model->center.x, -model->center.y, model->center.z - model->radius*1.5f };
		f32 cameraRot[] = { 1.0f, 0.0f };

		DXL2_ModelRender::draw(model, orientation, &modelPos, &cameraPos, cameraRot, 0);
	}

	//////////////////////////////////////////////
	// Internal
	//////////////////////////////////////////////
	void updateObjects()
	{
		GameObject* objects = LevelGameObjects::getGameObjectList()->data();
		SectorObjectList* sectorObjects = LevelGameObjects::getSectorObjectList();
		const u32 secCount = (u32)s_level->sectors.size();

		s_accum += (f32)DXL2_System::getDeltaTime();
		while (s_accum >= c_step)
		{
			Sector* sector = s_level->sectors.data();
			for (u32 s = 0; s < secCount; s++, sector++)
			{
				const std::vector<u32>& list = (*sectorObjects)[s].list;
				const u32 objCount = list.size();
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

					// The object should fall towards the floor.
					bool aboveSecHeight = sector->secAlt < 0.0f && obj->pos.y < sector->floorAlt + sector->secAlt + 0.1f;
					f32 floorHeight = aboveSecHeight ? sector->floorAlt + sector->secAlt : sector->floorAlt;

					obj->pos.y += obj->verticalVel * c_step;
					if (obj->pos.y >= floorHeight)
					{
						obj->verticalVel = 0.0f;
						obj->pos.y = floorHeight;
					}
					else
					{
						obj->verticalVel += c_gravityAccel * c_step;
					}
				}
			}

			s_accum -= c_step;
		}
	}
}

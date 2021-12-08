#include <climits>
#include <cstring>

#include "level.h"
#include "rwall.h"
#include "rtexture.h"
#include <TFE_Game/igame.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Asset/vocAsset.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_System/parser.h>
#include <TFE_System/system.h>

#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/InfSystem/infTypesInternal.h>
#include <TFE_Jedi/InfSystem/message.h>

// TODO: Fix game dependency?
#include <TFE_DarkForces/logic.h>

namespace TFE_Jedi
{
	enum
	{
		DF_LEVEL_VERSION_MAJOR = 2,
		DF_LEVEL_VERSION_MINOR = 1,
	};
	JBool s_complete[2][NUM_COMPLETE];
	s32 s_completeNum[2][NUM_COMPLETE];

	static s32 s_dataIndex;
	static s32 s_textureCount;

	static s32 s_podCount = 0;
	static s32 s_spriteCount = 0;
	static s32 s_fmeCount = 0;
	static s32 s_soundCount = 0;
	static s32 s_objectCount = 0;

	static TextureData** s_textures;
	static Allocator* s_soundEmitters;
	static Allocator* s_safeLoc;
				
	static JediModel** s_pods;
	static JediWax** s_sprites;
	static JediFrame** s_frames;
	static s32* s_soundIds;

	static Task* s_soundEmitterTask;

	static char s_readBuffer[256];
	static std::vector<char> s_buffer;
	
	s32 s_minLayer;
	s32 s_maxLayer;
	s32 s_secretCount;
	u32 s_sectorCount = 0;
	RSector* s_bossSector = nullptr;
	RSector* s_mohcSector = nullptr;
	RSector* s_controlSector = nullptr;
	RSector* s_completeSector = nullptr;
	RSector* s_sectors = nullptr;

	fixed16_16 s_parallax0;
	fixed16_16 s_parallax1;

	JBool level_loadGeometry(const char* levelName);
	JBool level_loadObjects(const char* levelName, u8 difficulty);
	JBool level_loadGoals(const char* levelName);

	void level_clearData()
	{
		s_soundEmitters    = nullptr;
		s_soundEmitterTask = nullptr;
		s_safeLoc          = nullptr;
		s_completeSector   = nullptr;
		s_bossSector       = nullptr;
		s_mohcSector       = nullptr;

		s_sectors  = nullptr;
		s_pods     = nullptr;
		s_sprites  = nullptr;
		s_frames   = nullptr;
		s_soundIds = nullptr;
		s_textures = nullptr;

		s_secretCount  = 0;
		s_sectorCount  = 0;
		s_textureCount = 0;

		s_podCount    = 0;
		s_spriteCount = 0;
		s_fmeCount    = 0;
		s_soundCount  = 0;
		s_objectCount = 0;

		s_controlSector = (RSector*)level_alloc(sizeof(RSector));
		sector_clear(s_controlSector);
	}

	JBool level_load(const char* levelName, u8 difficulty)
	{
		if (!levelName) { return JFALSE; }

		// Clear just in case.
		for (s32 i = 0; i < NUM_COMPLETE; i++)
		{
			s_completeNum[COMPL_TRIG][i] = -1;
			s_completeNum[COMPL_ITEM][i] = -1;

			s_complete[COMPL_TRIG][i] = JFALSE;
			s_complete[COMPL_ITEM][i] = JFALSE;
		}

		if (!level_loadGeometry(levelName)) { return JFALSE; }
		level_loadObjects(levelName, difficulty);
		inf_load(levelName);
		level_loadGoals(levelName);

		return JTRUE;
	}

	JBool level_loadGeometry(const char* levelName)
	{
		s_secretCount = 0;
		s_dataIndex = 0;
		s_minLayer = INT_MAX;
		s_maxLayer = INT_MIN;
		message_free();

		char levelPath[TFE_MAX_PATH];
		strcpy(levelPath, levelName);
		strcat(levelPath, ".LEV");

		FilePath filePath;
		if (!TFE_Paths::getFilePath(levelPath, &filePath))
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot find level geometry '%s'.", levelName);
			return false;
		}
		FileStream file;
		if (!file.open(&filePath, FileStream::MODE_READ))
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot open level geometry '%s'.", levelName);
			return false;
		}
		size_t len = file.getSize();
		s_buffer.resize(len);
		file.readBuffer(s_buffer.data(), u32(len));
		file.close();

		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init(s_buffer.data(), s_buffer.size());
		parser.enableBlockComments();
		parser.addCommentString("#");
		parser.addCommentString("//");
		parser.convertToUpperCase(true);

		// Only use the parser "read line" functionality and otherwise read in the same was as the DOS code.
		const char* line;
		line = parser.readLine(bufferPos);
		s32 versionMajor, versionMinor;
		if (sscanf(line, "LEV %d.%d", &versionMajor, &versionMinor) != 2)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read version.");
			return false;
		}
		if (versionMajor != DF_LEVEL_VERSION_MAJOR || versionMinor != DF_LEVEL_VERSION_MINOR)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Invalid level version %d.%d.", versionMajor, versionMinor);
			return false;
		}
		
		line = parser.readLine(bufferPos);
		char name[256];
		if (sscanf(line, "LEVELNAME %s", name) != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read level name.");
			return false;
		}

		// This gets read here just to be overwritten later... so just ignore for now.
		line = parser.readLine(bufferPos);
		if (sscanf(line, "PALETTE %s", s_readBuffer) != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read palette name.");
			return false;
		}
		
		// Another value that is ignored.
		line = parser.readLine(bufferPos);
		JBool readNextLine = JTRUE;
		if (sscanf(line, "MUSIC %s", s_readBuffer) != 1)
		{
			TFE_System::logWrite(LOG_WARNING, "level_loadGeometry", "Cannot read music name.");
			readNextLine = JFALSE;
		}

		// Sky Parallax.
		f32 parallax0, parallax1;
		if (readNextLine) { line = parser.readLine(bufferPos); }
		if (sscanf(line, "PARALLAX %f %f", &parallax0, &parallax1) != 2)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read parallax values.");
			return false;
		}
		s_parallax0 = floatToFixed16(parallax0);
		s_parallax1 = floatToFixed16(parallax1);

		// Number of textures used by the level.
		line = parser.readLine(bufferPos);
		if (sscanf(line, "TEXTURES %d", &s_textureCount) != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read texture count.");
			return false;
		}
		s_textures = (TextureData**)res_alloc(s_textureCount * sizeof(TextureData**));

		// Load Textures.
		TextureData** texture = s_textures;
		for (s32 i = 0; i < s_textureCount; i++, texture++)
		{
			line = parser.readLine(bufferPos);
			char textureName[256];
			if (sscanf(line, " TEXTURE: %s", textureName) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read texture name.");
				textureName[0] = 0;
			}

			// If <NoTexture> is found, do not try to load - this will cause the default texture to be used.
			TextureData* tex = nullptr;
			if (strcasecmp(textureName, "<NoTexture>"))
			{
				if (TFE_Paths::getFilePath(textureName, &filePath))
				{
					tex = bitmap_load(&filePath, 1);
				}
			}

			if (!tex)
			{
				TFE_System::logWrite(LOG_WARNING, "level_loadGeometry", "Could not open '%s', using 'default.bm' instead.", textureName);

				TFE_Paths::getFilePath("default.bm", &filePath);
				tex = bitmap_load(&filePath, 1);
				if (!tex)
				{
					TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "'default.bm' is not a valid BM file!");
					return false;
				}
			}
			*texture = tex;

			// Setup an animated texture.
			if (tex->uvWidth == BM_ANIMATED_TEXTURE)
			{
				bitmap_setupAnimatedTexture(texture);
			}
		}

		// Load Sectors.
		line = parser.readLine(bufferPos);
		if (sscanf(line, "NUMSECTORS %d", &s_sectorCount) != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector count.");
			return false;
		}

		s_sectors = (RSector*)level_alloc(sizeof(RSector) * s_sectorCount);
		memset(s_sectors, 0, sizeof(RSector) * s_sectorCount);
		for (u32 i = 0; i < s_sectorCount; i++)
		{
			RSector* sector = &s_sectors[i];
			sector_clear(sector);
			sector->index = i;

			// Sector ID and Name
			line = parser.readLine(bufferPos);
			if (sscanf(line, "SECTOR %d", &sector->id) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector id.");
				return false;
			}

			line = parser.readLine(bufferPos);
			// Sectors missing a name are valid but do not get "addresses" - and thus cannot be
			// used by the INF system (except in the case of doors and exploding walls, see the flags section below).
			if (sscanf(line, " NAME %s", name) == 1)
			{
				// Add the sector "address" for later use by the INF system.
				message_addAddress(name, 0, 0, sector);

				// Track special elevators.
				if (!strcasecmp(name, "complete"))
				{
					s_completeSector = sector;
				}
				else if (!strcasecmp(name, "boss"))
				{
					s_bossSector = sector;
				}
				else if (!strcasecmp(name, "mohc"))
				{
					s_mohcSector = sector;
				}
			}

			// Lighting
			line = parser.readLine(bufferPos);
			s32 ambient;
			if (sscanf(line, " AMBIENT %d", &ambient) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector ambient.");
				return false;
			}
			sector->ambient = intToFixed16(ambient);

			// Floor Texture & Offset
			line = parser.readLine(bufferPos);
			s32 index, tmp;
			f32 offsetX, offsetZ;
			if (sscanf(line, " FLOOR TEXTURE %d %f %f %d", &index, &offsetX, &offsetZ, &tmp) != 4)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read floor texture.");
				return false;
			}

			sector->floorTex = nullptr;
			if (index != -1)
			{
				sector->floorTex = &s_textures[index];
			}
			sector->floorOffset.x = floatToFixed16(offsetX);
			sector->floorOffset.z = floatToFixed16(offsetZ);

			// Floor Altitude
			line = parser.readLine(bufferPos);
			f32 alt;
			if (sscanf(line, " FLOOR ALTITUDE %f", &alt) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read floor altitude.");
				return false;
			}
			sector->floorHeight = floatToFixed16(alt);

			// Ceiling Texture & Offset
			line = parser.readLine(bufferPos);
			if (sscanf(line, " CEILING TEXTURE %d %f %f %d", &index, &offsetX, &offsetZ, &tmp) != 4)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read ceiling texture.");
				return false;
			}
			sector->ceilTex = nullptr;
			if (index != -1)
			{
				sector->ceilTex = &s_textures[index];
			}
			sector->ceilOffset.x = floatToFixed16(offsetX);
			sector->ceilOffset.z = floatToFixed16(offsetZ);

			// Ceiling Altitude
			line = parser.readLine(bufferPos);
			if (sscanf(line, " CEILING ALTITUDE %f", &alt) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read ceiling altitude.");
				return false;
			}
			sector->ceilingHeight = floatToFixed16(alt);

			// Second Altitude
			line = parser.readLine(bufferPos);
			if (sscanf(line, " SECOND ALTITUDE %f", &alt) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read second altitude.");
				return false;
			}
			sector->secHeight = floatToFixed16(alt);

			// Sector flags
			line = parser.readLine(bufferPos);
			if (sscanf(line, " FLAGS %d %d %d", &sector->flags1, &sector->flags2, &sector->flags3) != 3)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector flags.");
				return false;
			}
			// Create a door if needed.
			if (sector->flags1 & SEC_FLAGS1_DOOR)
			{
				InfElevator* elev = inf_allocateSpecialElevator(sector, IELEV_SP_DOOR);
				if (elev) { elev->flags |= INF_EFLAG_DOOR; }
			}
			// Create an exploding wall if needed.
			if (sector->flags1 & SEC_FLAGS1_EXP_WALL)
			{
				inf_allocateSpecialElevator(sector, IELEV_SP_EXPLOSIVE_WALL);
			}
			// Add secrets.
			if (sector->flags1 & SEC_FLAGS1_SECRET)
			{
				s_secretCount++;
			}

			// Layer
			line = parser.readLine(bufferPos);
			if (sscanf(line, " LAYER %d", &sector->layer) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector layer.");
				return false;
			}
			s_minLayer = min(s_minLayer, sector->layer);
			s_maxLayer = max(s_maxLayer, sector->layer);

			// Vertices
			line = parser.readLine(bufferPos);
			s32 vertexCount;
			if (sscanf(line, " VERTICES %d", &vertexCount) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector vertices.");
				return false;
			}
			const size_t vtxSize = vertexCount * sizeof(vec2_fixed);
			sector->verticesWS = (vec2_fixed*)level_alloc(vtxSize);
			sector->verticesVS = (vec2_fixed*)level_alloc(vtxSize);
			sector->vertexCount = vertexCount;

			for (s32 v = 0; v < vertexCount; v++)
			{
				line = parser.readLine(bufferPos);

				f32 x, z;
				sscanf(line, " X: %f Z: %f", &x, &z);
				sector->verticesWS[v].x = floatToFixed16(x);
				sector->verticesWS[v].z = floatToFixed16(z);
			}

			// Walls
			line = parser.readLine(bufferPos);
			s32 wallCount;
			if (sscanf(line, " WALLS %d", &wallCount) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector walls.");
				return false;
			}
			sector->walls = (RWall*)level_alloc(wallCount * sizeof(RWall));
			sector->wallCount = wallCount;

			for (s32 w = 0; w < wallCount; w++)
			{
				s32 light, flags3, flags2, flags1, walk, mirror, adjoin;
				s32 signTex, unused;
				s32 botTex, topTex, midTex, right, left;
				f32 signOffsetZ, signOffsetX;
				f32 botOffsetZ, botOffsetX;
				f32 topOffsetZ, topOffsetX;
				f32 midOffsetZ, midOffsetX;

				line = parser.readLine(bufferPos);
				if (sscanf(line, " WALL LEFT: %d RIGHT: %d MID: %d %f %f %d TOP: %d %f %f %d BOT: %d %f %f %d SIGN: %d %f %f ADJOIN: %d MIRROR: %d WALK: %d FLAGS: %d %d %d LIGHT: %d",
					&left, &right, &midTex, &midOffsetX, &midOffsetZ, &unused, &topTex, &topOffsetX, &topOffsetZ, &unused, &botTex, &botOffsetX, &botOffsetZ, &unused,
					&signTex, &signOffsetX, &signOffsetZ, &adjoin, &mirror, &walk, &flags1, &flags2, &flags3, &light) != 24)
				{
					TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read wall.");
					return false;
				}

				RWall* wall = &sector->walls[w];
				wall->id = w;
				wall->sector = sector;
				wall->mirrorWall = nullptr;
				wall->seen = JFALSE;
				wall->flags1 = flags1;
				wall->flags2 = flags2;
				wall->flags3 = flags3;

				vec2_fixed* leftVtxWS = &sector->verticesWS[left];
				vec2_fixed* rightVtxWS = &sector->verticesWS[right];
				wall->w0 = leftVtxWS;
				wall->w1 = rightVtxWS;
				wall->v0 = &sector->verticesVS[left];
				wall->v1 = &sector->verticesVS[right];
				// Store the original position 0 in the wall since it is used by the sector rotation INF.
				wall->worldPos0.x = leftVtxWS->x;
				wall->worldPos0.z = leftVtxWS->z;

				wall->nextSector = nullptr;
				wall->mirror = -1;
				if (adjoin != -1)
				{
					wall->nextSector = &s_sectors[adjoin];
					if (mirror == -1)
					{
						TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Adjoining wall missing mirror.");
					}
					wall->mirror = mirror;
				}

				wall->infLink = nullptr;
				wall->collisionFrame = 0;
				wall->drawFrame = 0;
				wall->drawFlags = 0;
				wall->wallLight = intToFixed16(light);

				wall->midTex = nullptr;
				if (midTex != -1)
				{
					wall->midTex = &s_textures[midTex];
					wall->midOffset.x = floatToFixed16(midOffsetX) * 8;
					wall->midOffset.z = floatToFixed16(midOffsetZ) * 8;
				}

				wall->topTex = nullptr;
				if (topTex != -1)
				{
					wall->topTex = &s_textures[topTex];
					wall->topOffset.x = floatToFixed16(topOffsetX) * 8;
					wall->topOffset.z = floatToFixed16(topOffsetZ) * 8;
				}

				wall->botTex = nullptr;
				if (botTex != -1)
				{
					wall->botTex = &s_textures[botTex];
					wall->botOffset.x = floatToFixed16(botOffsetX) * 8;
					wall->botOffset.z = floatToFixed16(botOffsetZ) * 8;
				}

				wall->signTex = nullptr;
				if (signTex != -1)
				{
					wall->signTex = &s_textures[signTex];
					wall->signOffset.x = floatToFixed16(signOffsetX) * 8;
					wall->signOffset.z = floatToFixed16(signOffsetZ) * 8;
				}

				fixed16_16 dx = rightVtxWS->x - leftVtxWS->x;
				fixed16_16 dz = rightVtxWS->z - leftVtxWS->z;
				wall->angle  = vec2ToAngle(dx, dz);
				wall->length = vec2Length(dx, dz);
				wall_computeDirectionVector(wall);
				wall->texelLength = wall->length * 8;
			}
		}

		// Process sectors after load.
		RSector* sector = s_sectors;
		for (u32 i = 0; i < s_sectorCount; i++, sector++)
		{
			RWall* wall = sector->walls;
			for (s32 w = 0; w < sector->wallCount; w++, wall++)
			{
				RSector* nextSector = wall->nextSector;
				if (nextSector)
				{
					RWall* mirror = &nextSector->walls[wall->mirror];
					wall->mirrorWall = mirror;
					// Both sides of a mirror should have the same lower flags3 (such as walkability).
					wall->flags3 |= (mirror->flags3 & 0x0f);
					mirror->flags3 |= (wall->flags3 & 0x0f);
				}
			}
			sector_setupWallDrawFlags(sector);
			sector_adjustHeights(sector, 0, 0, 0);
			sector_computeBounds(sector);
			// TFE: Added to support non-fixed-point rendering.
			sector->dirtyFlags = SDF_ALL;
		}

		return true;
	}

	void level_freeAllAssets()
	{
		TFE_Sprite_Jedi::freeAll();
		TFE_Model_Jedi::freeAll();
	}

	JBool level_isGoalComplete(s32 goalIndex)
	{
		for (s32 i = 0; i < NUM_COMPLETE; i++)
		{
			if (s_completeNum[COMPL_TRIG][i] == goalIndex)
			{
				return s_complete[COMPL_TRIG][i];
			}

			if (s_completeNum[COMPL_ITEM][i] == goalIndex)
			{
				return s_complete[COMPL_ITEM][i];
			}
		}
		return JFALSE;
	}
		
	JBool level_loadGoals(const char* levelName)
	{
		char levelPath[TFE_MAX_PATH];
		strcpy(levelPath, levelName);
		strcat(levelPath, ".GOL");
				
		FilePath filePath;
		if (!TFE_Paths::getFilePath(levelPath, &filePath))
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGoals", "Cannot find level goals '%s'.", levelName);
			return JFALSE;
		}
		FileStream file;
		if (!file.open(&filePath, FileStream::MODE_READ))
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGoals", "Cannot open level goals '%s'.", levelName);
			return JFALSE;
		}

		size_t len = file.getSize();
		s_buffer.resize(len);
		file.readBuffer(s_buffer.data(), u32(len));
		file.close();

		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init(s_buffer.data(), s_buffer.size());
		parser.enableBlockComments();
		parser.addCommentString("//");
		parser.addCommentString("#");

		const char* line;
		line = parser.readLine(bufferPos);
		s32 versionMajor, versionMinor;
		if (sscanf(line, "GOL %d.%d", &versionMajor, &versionMinor) != 2)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot parse version for Goal file '%s'.", levelName);
			return false;
		}

		line = parser.readLine(bufferPos);
		while (line)
		{
			s32 goalNum, typeNum;
			char type[32];
			if (sscanf(line, " GOAL: %d %s %d", &goalNum, type, &typeNum) == 3)
			{
				if (typeNum < 0 || typeNum >= NUM_COMPLETE)
				{
					TFE_System::logWrite(LOG_ERROR, "Goals", "Bad item or trigger number in .GOL file");
				}

				if (typeNum >= 0 && typeNum < NUM_COMPLETE)
				{
					if (strcasecmp(type, "ITEM:") == 0)
					{
						s_completeNum[COMPL_ITEM][typeNum] = goalNum;
					}
					else if (strcasecmp(type, "TRIG:") == 0)
					{
						s_completeNum[COMPL_TRIG][typeNum] = goalNum;
					}
				}
			}

			line = parser.readLine(bufferPos);
		}
		file.close();

		return JTRUE;
	}
					
	JBool level_loadObjects(const char* levelName, u8 difficulty)
	{
		char levelPath[TFE_MAX_PATH];
		strcpy(levelPath, levelName);
		strcat(levelPath, ".O");

		s32 curDiff = s32(difficulty) + 1;

		FilePath filePath;
		if (!TFE_Paths::getFilePath(levelPath, &filePath))
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot find level objects '%s'.", levelName);
			return false;
		}
		FileStream file;
		if (!file.open(&filePath, FileStream::MODE_READ))
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot open level objects '%s'.", levelName);
			return false;
		}

		size_t len = file.getSize();
		s_buffer.resize(len);
		file.readBuffer(s_buffer.data(), u32(len));
		file.close();

		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init(s_buffer.data(), s_buffer.size());
		parser.enableBlockComments();
		parser.addCommentString("//");
		parser.addCommentString("#");
		parser.convertToUpperCase(true);

		// Only use the parser "read line" functionality and otherwise read in the same was as the DOS code.
		const char* line;
		line = parser.readLine(bufferPos);
		s32 versionMajor, versionMinor;
		if (sscanf(line, "O %d.%d", &versionMajor, &versionMinor) != 2)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot parse version for Object file '%s'.", levelName);
			return false;
		}

		line = parser.readLine(bufferPos);
		line = parser.readLine(bufferPos);
		if (sscanf(line, "PODS %d", &s_podCount) == 1)
		{
			s_pods = (JediModel**)res_alloc(sizeof(JediModel*)*s_podCount);
			for (s32 p = 0; p < s_podCount; p++)
			{
				line = parser.readLine(bufferPos);

				char podName[32];
				if (sscanf(line, " POD: %s", podName) == 1)
				{
					s_pods[p] = TFE_Model_Jedi::get(podName);
					if (!s_pods[p])
					{
						s_pods[p] = TFE_Model_Jedi::get("default.3do");
					}
				}
			}
		}

		line = parser.readLine(bufferPos);
		if (sscanf(line, "SPRS %d", &s_spriteCount) == 1)
		{
			s_sprites = (JediWax**)res_alloc(sizeof(JediWax*)*s_spriteCount);
			for (s32 s = 0; s < s_spriteCount; s++)
			{
				line = parser.readLine(bufferPos);

				char name[32];
				if (sscanf(line, " SPR: %s", name) == 1)
				{
					s_sprites[s] = TFE_Sprite_Jedi::getWax(name);
				}
				if (!s_sprites[s])
				{
					s_sprites[s] = TFE_Sprite_Jedi::getWax("default.wax");
				}
			}
		}

		line = parser.readLine(bufferPos);
		if (sscanf(line, "FMES %d", &s_fmeCount) == 1)
		{
			s_frames = (JediFrame**)res_alloc(sizeof(JediFrame*)*s_fmeCount);
			for (s32 f = 0; f < s_fmeCount; f++)
			{
				line = parser.readLine(bufferPos);

				char name[32];
				if (sscanf(line, " FME: %s", name) == 1)
				{
					s_frames[f] = TFE_Sprite_Jedi::getFrame(name);
				}
				if (!s_frames[f])
				{
					s_frames[f] = TFE_Sprite_Jedi::getFrame("default.fme");
				}
			}
		}

		line = parser.readLine(bufferPos);
		if (sscanf(line, "SOUNDS %d", &s_soundCount) == 1)
		{
			for (s32 s = 0; s < s_soundCount; s++)
			{
				line = parser.readLine(bufferPos);

				char name[32];
				if (sscanf(line, "SOUND: %s", name) == 1)
				{
					// The DOS code uses indices for sounds but TFE just returns the buffers for now.
					s32 index = 0;
					if (TFE_VocAsset::get(name))
					{
						// index = 0 = no sound.
						index = TFE_VocAsset::getIndex(name) + 1;
					}
					s_soundIds[s] = index;
				}
			}
		}

		line = parser.readLine(bufferPos);
		if (sscanf(line, "OBJECTS %d", &s_objectCount) == 1)
		{
			JBool valid = JTRUE;
			s32 count = s_objectCount;

			line = parser.readLine(bufferPos);
			for (s32 objIndex = 0; objIndex < count && valid;)
			{
				if (!line)
				{
					break;
				}

				s32 data = 0, objDiff = 0;
				f32 x, y, z, pch, yaw, rol;
				char objClass[32];

				if (sscanf(line, " CLASS: %s DATA: %d X: %f Y: %f Z: %f PCH: %f YAW: %f ROL: %f DIFF: %d", objClass, &s_dataIndex, &x, &y, &z, &pch, &yaw, &rol, &objDiff) > 5)
				{
					objIndex++;
					// objDiff >= 0: This difficulty and all greater.
					// objDiff <  0: Less than this difficulty.
					if ((objDiff >= 0 && curDiff < objDiff) || (objDiff < 0 && curDiff > TFE_Jedi::abs(objDiff)))
					{
						line = parser.readLine(bufferPos);
						continue;
					}

					vec3_fixed posWS;
					posWS.x = floatToFixed16(x);
					posWS.y = floatToFixed16(y);
					posWS.z = floatToFixed16(z);

					// The DOS code allocated the object, tried to find the sector it is in and than frees the object
					// if it doesn't fit.
					// Instead TFE just reads the values and only allocates the object if it has a valid sector.
					RSector* sector = sector_which3D(posWS.x, posWS.y, posWS.z);
					if (!sector)
					{
						line = parser.readLine(bufferPos);
						continue;
					}

					SecObject* obj = allocateObject();
					obj->posWS = posWS;
					obj->pitch = floatDegreesToFixed(pch);
					obj->yaw   = floatDegreesToFixed(yaw);
					obj->roll  = floatDegreesToFixed(rol);

					KEYWORD classType = getKeywordIndex(objClass);
					switch (classType)
					{
						case KW_3D:
						{
							sector_addObject(sector, obj);
							obj3d_setData(obj, s_pods[s_dataIndex]);
							obj->pitch = -obj->pitch;
							obj3d_computeTransform(obj);
						} break;
						case KW_SPRITE:
						{
							sector_addObject(sector, obj);
							sprite_setData(obj, s_sprites[s_dataIndex]);
						} break;
						case KW_FRAME:
						{
							sector_addObject(sector, obj);
							frame_setData(obj, s_frames[s_dataIndex]);
						} break;
						case KW_SPIRIT:
						{
							sector_addObject(sector, obj);
							spirit_setData(obj);
						} break;
						case KW_SOUND:
						{
							// TODO(Core Game Loop Release)
							// addSoundObject(s_soundIds[s_dataIndex], obj->posWS.x, obj->posWS.y, obj->posWS.z);
							freeObject(obj);
							obj = nullptr;
						} break;
						case KW_SAFE:
						{
							if (!s_safeLoc)
							{
								s_safeLoc = allocator_create(sizeof(Safe));
							}
							Safe* safe = (Safe*)allocator_newItem(s_safeLoc);
							safe->sector = sector;
							safe->x = obj->posWS.x;
							safe->z = obj->posWS.z;
							safe->yaw = obj->yaw;
							sector->flags1 |= SEC_FLAGS1_SAFESECTOR;

							freeObject(obj);
							obj = nullptr;
						} break;
					}

					JBool seqRead = JFALSE;
					if (obj)
					{
						seqRead = TFE_DarkForces::object_parseSeq(obj, &parser, &bufferPos);
						if (obj->entityFlags & ETFLAG_PLAYER)
						{
							if (!s_safeLoc)
							{
								s_safeLoc = allocator_create(sizeof(Safe));
							}
							Safe* safe = (Safe*)allocator_newItem(s_safeLoc);
							safe->sector = obj->sector;
							safe->x = obj->posWS.x;
							safe->z = obj->posWS.z;
							safe->yaw = obj->yaw;
							sector->flags1 |= SEC_FLAGS1_SAFESECTOR;
						}
					}
					if (!obj || seqRead)
					{
						line = parser.readLine(bufferPos);
					}
				}
				else
				{
					line = parser.readLine(bufferPos);
				}
			}
		}

		return JTRUE;
	}

	Safe* level_getSafeFromSector(RSector* sector)
	{
		Safe* safe = (Safe*)allocator_getHead(s_safeLoc);
		while (safe)
		{
			if (safe->sector == sector)
			{
				return safe;
			}
			else
			{
				safe = (Safe*)allocator_getNext(s_safeLoc);
			}
		}
		return nullptr;
	}
		
	void getSkyParallax(fixed16_16* parallax0, fixed16_16* parallax1)
	{
		*parallax0 = s_parallax0;
		*parallax1 = s_parallax1;
	}

	void setSkyParallax(fixed16_16 parallax0, fixed16_16 parallax1)
	{
		s_parallax0 = parallax0;
		s_parallax1 = parallax1;
	}

	void setObjPos_AddToSector(SecObject* obj, s32 x, s32 y, s32 z, RSector* sector)
	{
		obj->posWS.x = x;
		obj->posWS.y = y;
		obj->posWS.z = z;
		sector_addObject(sector, obj);
	}
}

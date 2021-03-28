#include "level.h"
#include "rwall.h"
#include "rtexture.h"
#include <TFE_Archive/archive.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_System/parser.h>
#include <TFE_System/system.h>

#include <TFE_InfSystem/infSystem.h>
#include <TFE_InfSystem/infTypesInternal.h>
#include <TFE_InfSystem/message.h>

using namespace TFE_InfSystem;

namespace TFE_Level
{
	enum
	{
		DF_LEVEL_VERSION_MAJOR = 2,
		DF_LEVEL_VERSION_MINOR = 1
	};

	static s32 s_minLayer;
	static s32 s_maxLayer;
	static s32 s_dataIndex;
	static s32 s_secretCount;
	static s32 s_textureCount;

	static TextureData** s_textures;
	static RSector* s_completeSector;
	static RSector* s_bossSector;
	static RSector* s_mohcSector;

	static fixed16_16 s_parallax0;
	static fixed16_16 s_parallax1;

	static u32 s_sectorCount = 0;
	static RSector* s_sectors = nullptr;
	static std::vector<char> s_buffer;

	static char s_readBuffer[256];

	static const char* c_defaultGob = "DARK.GOB";

	void initMission()
	{
		bitmap_createAnimatedTextureAllocator();
	}
	
	// level_loadGeometry() in the DOS code.
	bool loadGeometry(const char* levelName)
	{
		s_secretCount = 0;
		s_dataIndex = 0;
		s_minLayer = INT_MAX;
		s_maxLayer = INT_MIN;

		char levelPath[TFE_MAX_PATH];
		strcpy(levelPath, levelName);
		strcat(levelPath, ".LEV");

		char gobPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_SOURCE_DATA, c_defaultGob, gobPath);
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, levelName, s_buffer))
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot open level '%s', path '%s'.", levelName, gobPath);
			return false;
		}

		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init(s_buffer.data(), s_buffer.size());
		parser.enableBlockComments();
		parser.addCommentString("//");

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
		if (sscanf(line, "MUSIC %s", s_readBuffer) != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read music name.");
			return false;
		}

		// Sky Parallax.
		f32 parallax0, parallax1;
		line = parser.readLine(bufferPos);
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
		s_textures = (TextureData**)malloc(s_textureCount * sizeof(TextureData**));

		// Load Textures.
		TextureData** texture = s_textures;
		for (s32 i = 0; i < s_textureCount; i++, texture++)
		{
			line = parser.readLine(bufferPos);
			char textureName[256];
			if (sscanf(line, "TEXTURE: %s", textureName) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read texture name.");
				return false;
			}

			// If <NoTexture> is found, do not try to load - this will cause the default texture to be used.
			TextureData* tex = nullptr;
			if (strcasecmp(textureName, "<NoTexture>"))
			{
				tex = bitmap_load(textureName, 1);
			}
			else if (!tex)
			{
				TFE_System::logWrite(LOG_WARNING, "level_loadGeometry", "Could not open '%s', using 'default.bm' instead.", textureName);
			}

			if (!tex)
			{
				tex = bitmap_load("default.bm", 1);
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

		s_sectors = (RSector*)malloc(sizeof(RSector) * s_sectorCount);
		for (u32 i = 0; i < s_sectorCount; i++)
		{
			RSector* sector = &s_sectors[i];
			sector_clear(sector);

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
			if (sscanf(line, "NAME %s", name) == 1)
			{
				// Add the sector "address" for later use by the INF system.
				Message::addAddress(name, 0, 0, sector);

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
			if (sscanf(line, "AMBIENT %d", &ambient) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector ambient.");
				return false;
			}
			sector->ambient = intToFixed16(ambient);

			// Floor Texture & Offset
			line = parser.readLine(bufferPos);
			s32 index, tmp;
			f32 offsetX, offsetZ;
			if (sscanf(line, "FLOOR TEXTURE %d %f %f %d", &index, &offsetX, &offsetZ, &tmp) != 4)
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
			if (sscanf(line, "FLOOR ALTITUDE %f", &alt) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read floor altitude.");
				return false;
			}
			sector->floorHeight = floatToFixed16(alt);

			// Ceiling Texture & Offset
			line = parser.readLine(bufferPos);
			if (sscanf(line, "CEILING TEXTURE %d %f %f %d", &index, &offsetX, &offsetZ, &tmp) != 4)
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
			if (sscanf(line, "CEILING ALTITUDE %f", &alt) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read ceiling altitude.");
				return false;
			}
			sector->ceilingHeight = floatToFixed16(alt);

			// Second Altitude
			line = parser.readLine(bufferPos);
			if (sscanf(line, "SECOND ALTITUDE %f", &alt) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read second altitude.");
				return false;
			}
			sector->secHeight = floatToFixed16(alt);

			// Sector flags
			line = parser.readLine(bufferPos);
			if (sscanf(line, "FLAGS %d %d %d", &sector->flags1, &sector->flags2, &sector->flags3) != 3)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector flags.");
				return false;
			}
			// Create a door if needed.
			if (sector->flags1 & SEC_FLAGS1_DOOR)
			{
				InfElevator* elev = inf_allocateSpecialElevator(sector, IELEV_SP_DOOR);
				elev->flags |= INF_EFLAG_DOOR;
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
			if (sscanf(line, "LAYER %d", &sector->layer) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector layer.");
				return false;
			}
			s_minLayer = min(s_minLayer, sector->layer);
			s_maxLayer = max(s_maxLayer, sector->layer);

			// Vertices
			line = parser.readLine(bufferPos);
			s32 vertexCount;
			if (sscanf(line, "VERTICES %d", &vertexCount) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector vertices.");
				return false;
			}
			const size_t vtxSize = vertexCount * sizeof(vec2_fixed);
			sector->verticesWS = (vec2_fixed*)malloc(vtxSize);
			sector->verticesVS = (vec2_fixed*)malloc(vtxSize);
			sector->vertexCount = vertexCount;

			for (s32 v = 0; v < vertexCount; v++)
			{
				line = parser.readLine(bufferPos);

				f32 x, z;
				sscanf(line, "X: %f Z: %f", &x, &z);
				sector->verticesWS[v].x = floatToFixed16(x);
				sector->verticesWS[v].z = floatToFixed16(z);
			}

			// Walls
			line = parser.readLine(bufferPos);
			s32 wallCount;
			if (sscanf(line, "WALLS %d", &wallCount) != 1)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Cannot read sector walls.");
				return false;
			}
			sector->walls = (RWall*)malloc(wallCount * sizeof(RWall));
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

				if (sscanf(line, "WALL LEFT: %d RIGHT: %d MID: %d %f %f %d TOP: %d %f %f %d BOT: %d %f %f %d SIGN: %d %f %f ADJOIN: %d MIRROR: %d WALK: %d FLAGS: %d %d %d LIGHT: %d",
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
}

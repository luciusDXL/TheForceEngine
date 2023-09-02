#include "levelBin.h"
#include "level.h"
#include "levelData.h"
#include "rwall.h"
#include "rtexture.h"
#include <TFE_Game/igame.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_System/system.h>

#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/InfSystem/infTypesInternal.h>
#include <TFE_Jedi/InfSystem/message.h>

namespace TFE_Jedi
{
	struct ChunkHeaderDisk
	{
		char sig[3];
		u8   size;
	};
	struct ChunkHeader
	{
		u32 sig;
		u32 size;
	};

	// Chunk Signatures and other constants.
	#define	CHUNK_SIG(a, b, c) ((a) | (b)<<8 | (c)<<16 )
	enum ChunkConstants
	{
		// Signatures
		LvbRootSig        = CHUNK_SIG('L', 'V', 'B'),
		LvbVersionSig     = CHUNK_SIG('V', 'E', 'R'),
		LvbLevelNameSig   = CHUNK_SIG('L', 'N', 'A'),
		LvbPalFileSig     = CHUNK_SIG('P', 'F', 'L'),
		LvbMusicFileSig   = CHUNK_SIG('M', 'F', 'L'),
		LvbLevelInfoSig   = CHUNK_SIG('L', 'N', 'F'),
		LvbTexListSig     = CHUNK_SIG('T', 'E', 'X'),
		LvbSectorListSig  = CHUNK_SIG('S', 'C', 'S'),
		LvbTexCountSig    = CHUNK_SIG('T', 'N', 'O'),
		LvbSectorCountSig = CHUNK_SIG('S', 'N', 'O'),
		LvbSectorSig      = CHUNK_SIG('S', 'E', 'C'),
		LvbSectorNameSig  = CHUNK_SIG('S', 'N', 'A'),
		LvbSectorInfoSig  = CHUNK_SIG('S', 'N', 'F'),
		LvbVertexCountSig = CHUNK_SIG('V', 'N', 'O'),
		LvbVertexInfoSig  = CHUNK_SIG('V', 'R', 'T'),
		LvbWallListSig    = CHUNK_SIG('W', 'L', 'S'),
		LvbWallCountSig   = CHUNK_SIG('W', 'N', 'O'),
		// Other Constants.
		LvbVersion = 20,
		LvbChunkSizeMask   = 0xf0,
		LvbChunkSize16bits = 0xf2,
		LvbChunkSize32bits = 0xf4,
	};
	// Offsets to the wall part data.
	enum WallPartOffset
	{
		WallMidTex   = 0,
		WallMidX     = 2,
		WallMidZ     = 6,
		WallTopTex   = 11,
		WallTopX     = 13,
		WallTopZ     = 17,
		WallBotTex   = 22,
		WallBotX     = 24,
		WallBotZ     = 28,
		WallSignTex  = 33,
		WallSignX    = 35,
		WallSignZ    = 39,
		WallLeftId   = 43,
		WallRightId  = 45,
		WallAdjoinId = 47,
		WallMirrorId = 49,
		WallFlags1   = 51,
		WallFlags2   = 53,
		WallFlags3   = 55,
		WallLight    = 57,
	};
	// Offsets to the sector part data.
	enum SectorPartOffset
	{
		SectorFloorTex     = 0,
		SectorFloorX       = 2,
		SectorFloorZ       = 6,
		SectorFloorColor   = 10,
		SectorCeilTex      = 11,
		SectorCeilX        = 13,
		SectorCeilZ        = 17,
		SectorCeilColor    = 21,
		SectorFloorHeight  = 22,
		SectorCeilHeight   = 26,
		SectorSecondHeight = 30,
		SectorAmbient      = 34,
		SectorFlags1       = 36,
		SectorFlags2       = 38,
		SectorFlags3       = 40,
		SectorLayer        = 42,
	};

	// Helper macros to make the code a little more clear.
	#define ChunkReadU16() *((u16*)&data[offset]); offset += 2
	#define ChunkReadU32() *((u32*)&data[offset]); offset += 4
	#define ChunkReadS8()  *((s8*)&data[offset]);  offset++
	#define ChunkReadS16() *((s16*)&data[offset]); offset += 2
	#define ChunkReadS32() *((s32*)&data[offset]); offset += 4
	#define BufferReadFixed16(offset) *((fixed16_16*)&buffer[offset])
	#define BufferReadS16(offset) *((s16*)&buffer[offset])
	#define BufferReadU16(offset) *((u16*)&buffer[offset])

	/////////////////////////////////////////////////
	// Chunk File Format Code
	/////////////////////////////////////////////////
	s32 loadChunkHeader(const u8* data, u32& offset, ChunkHeader* headerOut)
	{
		ChunkHeaderDisk* header = (ChunkHeaderDisk*)&data[offset];
		u32 bytesRead = sizeof(ChunkHeaderDisk);
		offset += sizeof(ChunkHeaderDisk);

		headerOut->sig = 0;
		headerOut->size = 0;

		u32 size = 0;
		if ((header->size & LvbChunkSizeMask) != LvbChunkSizeMask)
		{
			size = header->size;
		}
		else
		{
			if (header->size == LvbChunkSize16bits) { size = ChunkReadU16(); bytesRead += 2; }
			else if (header->size == LvbChunkSize32bits) { size = ChunkReadU32(); bytesRead += 4; }
			else { return -1; }
		}

		memcpy(&headerOut->sig, header->sig, 3);
		headerOut->size = size;
		return bytesRead;
	}

	s32 loadChunkNumber(const ChunkHeader* header, const u8* data, u32& offset)
	{
		s32 retval = 0;
		switch (header->size)
		{
		case 1:
			retval = ChunkReadS8();
			break;
		case 2:
			retval = ChunkReadS16();
			break;
		case 4:
			retval = ChunkReadS32();
			break;
		}
		return retval;
	}

	s32 chunkSkipToSignature(ChunkHeader* header, u32 signature, u32& offset, const u8* data)
	{
		s32 totalBytesRead = 0;

		s32 bytesRead = loadChunkHeader(data, offset, header);
		if (bytesRead < 0) { return 0; }
		totalBytesRead += bytesRead;

		while (header->sig != signature)
		{
			totalBytesRead += header->size;
			offset += header->size;

			bytesRead = loadChunkHeader(data, offset, header);
			if (bytesRead < 0) { return 0; }
			totalBytesRead += bytesRead;
		}

		return totalBytesRead;
	}

	/////////////////////////////////////////////////
	// Internal Implementation
	/////////////////////////////////////////////////
	s32 level_loadTextureListBin(ChunkHeader* header, u32& offset, const u8* data)
	{
		const u32 dataEnd = offset + header->size;
		const s32 baseOffset = offset;

		loadChunkHeader(data, offset, header);
		if (header->sig != LvbTexCountSig)
		{
			TFE_System::logWrite(LOG_WARNING, "level_loadTextureListBin", "Invalid texture list formatting.");
			return 1;
		}
		s_levelState.textureCount = loadChunkNumber(header, data, offset);
		s_levelState.textures = (TextureData**)level_alloc(2 * s_levelState.textureCount * sizeof(TextureData**));
		memset(s_levelState.textures, 0, 2 * s_levelState.textureCount * sizeof(TextureData**));

		// Load Textures.
		TextureData** texture = s_levelState.textures;
		TextureData** texBase = s_levelState.textures + s_levelState.textureCount;
		for (s32 i = 0; offset < dataEnd; i++, texture++, texBase++)
		{
			loadChunkHeader(data, offset, header);
			const char* textureName = (const char*)&data[offset];
			offset += header->size;

			if (strcasecmp(textureName, "<NoTexture>") == 0)
			{
				*texture = nullptr;
			}
			else
			{
				TextureData* tex = bitmap_load(textureName, 1);
				if (!tex)
				{
					TFE_System::logWrite(LOG_WARNING, "level_loadTextureListBin", "Could not open '%s', using 'default.bm' instead.", textureName);
					tex = bitmap_load("default.bm", 1);
					if (!tex)
					{
						TFE_System::logWrite(LOG_ERROR, "level_loadTextureListBin", "'default.bm' is not a valid BM file!");
						offset = baseOffset;
						assert(0);
						return false;
					}
				}
				*texture = tex;
				// This version never gets modified, so serialization is simpler.
				*texBase = tex;

				// Setup an animated texture.
				if (tex->uvWidth == BM_ANIMATED_TEXTURE)
				{
					bitmap_setupAnimatedTexture(texture, i);
				}
			}
		}
		offset = baseOffset;
		return 0;
	}
		
	s32 level_loadWallsBin(RSector* sector, ChunkHeader* header, u32& offset, const u8* data)
	{
		loadChunkHeader(data, offset, header);
		if (header->sig != LvbWallCountSig)
		{
			TFE_System::logWrite(LOG_WARNING, "level_loadWallsBin", "Invalid wall list formatting.");
			return 1;
		}
		sector->wallCount = loadChunkNumber(header, data, offset);
		sector->walls = (RWall*)level_alloc(sector->wallCount * sizeof(RWall));
				
		RWall* wall = sector->walls;
		for (s32 w = 0; w < sector->wallCount; w++, wall++)
		{
			loadChunkHeader(data, offset, header);
			const u8* buffer = &data[offset];
			offset += header->size;

			memset(wall, 0, sizeof(RWall));
			wall->id = w;
			wall->sector = sector;

			s32 index = BufferReadS16(WallMidTex);
			wall->midTex = index >= 0 ? &s_levelState.textures[index] : nullptr;
			wall->midOffset.x = BufferReadFixed16(WallMidX);
			wall->midOffset.z = BufferReadFixed16(WallMidZ);

			index = BufferReadS16(WallTopTex);
			wall->topTex = index >= 0 ? &s_levelState.textures[index] : nullptr;
			wall->topOffset.x = BufferReadFixed16(WallTopX);
			wall->topOffset.z = BufferReadFixed16(WallTopZ);

			index = BufferReadS16(WallBotTex);
			wall->botTex = index >= 0 ? &s_levelState.textures[index] : nullptr;
			wall->botOffset.x = BufferReadFixed16(WallBotX);
			wall->botOffset.z = BufferReadFixed16(WallBotZ);

			index = BufferReadS16(WallSignTex);
			wall->signTex = index >= 0 ? &s_levelState.textures[index] : nullptr;
			wall->signOffset.x = BufferReadFixed16(WallSignX);
			wall->signOffset.z = BufferReadFixed16(WallSignZ);

			s32 left  = BufferReadS16(WallLeftId);
			s32 right = BufferReadS16(WallRightId);
			vec2_fixed* leftVtxWS = &sector->verticesWS[left];
			vec2_fixed* rightVtxWS = &sector->verticesWS[right];
			wall->w0 = leftVtxWS;
			wall->w1 = rightVtxWS;
			wall->v0 = &sector->verticesVS[left];
			wall->v1 = &sector->verticesVS[right];
			// Store the original position 0 in the wall since it is used by the sector rotation INF.
			wall->worldPos0.x = leftVtxWS->x;
			wall->worldPos0.z = leftVtxWS->z;

			fixed16_16 dx = rightVtxWS->x - leftVtxWS->x;
			fixed16_16 dz = rightVtxWS->z - leftVtxWS->z;
			wall->angle = vec2ToAngle(dx, dz);
			wall->length = vec2Length(dx, dz);
			wall_computeDirectionVector(wall);
			wall->texelLength = wall->length * 8;

			s32 adjoinId = BufferReadS16(WallAdjoinId);
			s32 mirrorId = BufferReadS16(WallMirrorId);

			wall->mirror = -1;
			if (adjoinId != -1)
			{
				wall->nextSector = &s_levelState.sectors[adjoinId];
				if (mirrorId == -1)
				{
					TFE_System::logWrite(LOG_ERROR, "level_loadGeometry", "Adjoining wall missing mirror.");
				}
				wall->mirror = mirrorId;
			}

			wall->flags1 = BufferReadS16(WallFlags1);
			wall->flags2 = BufferReadS16(WallFlags2);
			wall->flags3 = BufferReadS16(WallFlags3);
			wall->wallLight = intToFixed16(BufferReadS16(WallLight));
		}

		return 0;
	}

	s32 level_loadSectorsBin(ChunkHeader* header, u32& offset, const u8* data)
	{
		const u32 dataEnd = offset + header->size;
		const s32 baseOffset = offset;

		loadChunkHeader(data, offset, header);
		if (header->sig != LvbSectorCountSig)
		{
			offset = baseOffset;
			TFE_System::logWrite(LOG_WARNING, "level_loadSectorsBin", "Invalid sector list formatting.");
			return 1;
		}

		s_levelState.sectorCount = loadChunkNumber(header, data, offset);
		s_levelState.sectors = (RSector*)level_alloc(sizeof(RSector) * s_levelState.sectorCount);
		memset(s_levelState.sectors, 0, sizeof(RSector) * s_levelState.sectorCount);
		for (u32 i = 0; i < s_levelState.sectorCount && offset < dataEnd; i++)
		{
			RSector* sector = &s_levelState.sectors[i];
			sector_clear(sector);
			sector->index = i;

			loadChunkHeader(data, offset, header);
			if (header->sig != LvbSectorSig)
			{
				offset = baseOffset;
				TFE_System::logWrite(LOG_WARNING, "level_loadSectorsBin", "Invalid sector list formatting.");
				return 1;
			}

			loadChunkHeader(data, offset, header);
			if (header->sig == LvbSectorNameSig)
			{
				const char* sectorName = (const char*)&data[offset];
				offset += header->size;

				// Add the sector "address" for later use by the INF system.
				message_addAddress(sectorName, 0, 0, sector);

				// Track special elevators.
				if (!strcasecmp(sectorName, "complete"))
				{
					s_levelState.completeSector = sector;
				}
				else if (!strcasecmp(sectorName, "boss"))
				{
					s_levelState.bossSector = sector;
				}
				else if (!strcasecmp(sectorName, "mohc"))
				{
					s_levelState.mohcSector = sector;
				}

				loadChunkHeader(data, offset, header);
			}

			/////////////////////////////////////////////
			// Sector Data
			/////////////////////////////////////////////
			if (header->sig != LvbSectorInfoSig)
			{
				offset = baseOffset;
				TFE_System::logWrite(LOG_WARNING, "level_loadSectorsBin", "Invalid sector list formatting.");
				return 1;
			}
			const u8* buffer = &data[offset];
			offset += header->size;

			s32 index = BufferReadS16(SectorFloorTex);
			sector->floorTex = index >= 0 ? &s_levelState.textures[index] : nullptr;
			sector->floorOffset.x = BufferReadFixed16(SectorFloorX);
			sector->floorOffset.z = BufferReadFixed16(SectorFloorZ);
			// floor color is no longer used: SectorFloorColor

			index = BufferReadS16(SectorCeilTex);
			sector->ceilTex = index >= 0 ? &s_levelState.textures[index] : nullptr;
			sector->ceilOffset.x = BufferReadFixed16(SectorCeilX);
			sector->ceilOffset.z = BufferReadFixed16(SectorCeilZ);
			// ceiling color is no longer used: SectorFloorColor

			sector->floorHeight   = BufferReadFixed16(SectorFloorHeight);
			sector->ceilingHeight = BufferReadFixed16(SectorCeilHeight);
			sector->secHeight     = BufferReadFixed16(SectorSecondHeight);

			sector->ambient = intToFixed16(BufferReadS16(SectorAmbient));
			sector->flags1  = BufferReadU16(SectorFlags1);
			sector->flags2  = BufferReadU16(SectorFlags2);
			sector->flags3  = BufferReadU16(SectorFlags3);
			sector->layer   = BufferReadU16(SectorLayer);

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
				s_levelState.secretCount++;
			}
			s_levelState.minLayer = min(s_levelState.minLayer, sector->layer);
			s_levelState.maxLayer = max(s_levelState.maxLayer, sector->layer);

			/////////////////////////////////////////////
			// Vertices
			/////////////////////////////////////////////
			loadChunkHeader(data, offset, header);
			if (header->sig != LvbVertexCountSig)
			{
				offset = baseOffset;
				TFE_System::logWrite(LOG_WARNING, "level_loadSectorsBin", "Invalid sector list formatting.");
				return 1;
			}
			sector->vertexCount = loadChunkNumber(header, data, offset);
			const size_t vtxSize = sector->vertexCount * sizeof(vec2_fixed);
			sector->verticesWS = (vec2_fixed*)level_alloc(vtxSize);
			sector->verticesVS = (vec2_fixed*)level_alloc(vtxSize);
			for (s32 i = 0; i < sector->vertexCount; i++)
			{
				chunkSkipToSignature(header, LvbVertexInfoSig, offset, data);
				memcpy(&sector->verticesWS[i].x, &data[offset], sizeof(fixed16_16) * 2);
				offset += header->size;
			}

			/////////////////////////////////////////////
			// Walls
			/////////////////////////////////////////////
			loadChunkHeader(data, offset, header);
			if (header->sig != LvbWallListSig)
			{
				offset = baseOffset;
				TFE_System::logWrite(LOG_WARNING, "level_loadSectorsBin", "Invalid sector list formatting.");
				return 1;
			}
			level_loadWallsBin(sector, header, offset, data);
		}

		level_postProcessGeometry();
		offset = baseOffset;
		return 0;
	}

	/////////////////////////////////////////////////
	// Public API
	/////////////////////////////////////////////////
	bool level_loadGeometryBin(const char* levelName, std::vector<char>& buffer)
	{
		char levelPath[TFE_MAX_PATH];
		strcpy(levelPath, levelName);
		strcat(levelPath, ".LVB");

		// Do not warn if an LVB cannot be loaded,
		// the final game doesn't use LVB files.
		FilePath filePath;
		if (!TFE_Paths::getFilePath(levelPath, &filePath))
		{
			return false;
		}
		FileStream file;
		if (!file.open(&filePath, Stream::MODE_READ))
		{
			return false;
		}
		u32 len = (u32)file.getSize();
		buffer.resize(len);
		file.readBuffer(buffer.data(), len);
		file.close();

		u32 offset = 0u;
		const u8* data = (u8*)buffer.data();

		// File Header.
		ChunkHeader header;
		loadChunkHeader(data, offset, &header);
		if (header.sig != LvbRootSig)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometryBin", "Invalid LVB format for '%s'.", levelName);
			return false;
		}
		const u32 endOfFile = header.size + offset - 1;

		// Version.
		loadChunkHeader(data, offset, &header);
		if (header.sig != LvbVersionSig)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometryBin", "Invalid LVB format for '%s'.", levelName);
			return false;
		}
		s32 version = loadChunkNumber(&header, data, offset);
		if (version != LvbVersion)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadGeometryBin", "Invalid LVB version '%d' for '%s', it should be '%d'", version, levelName, LvbVersion);
			return false;
		}

		// Read the remaining chunks in the file.
		while (offset < endOfFile)
		{
			loadChunkHeader(data, offset, &header);
			const u32 chunkSize = header.size;

			switch (header.sig)
			{
				case LvbRootSig:
				{
				} break;
				case LvbLevelNameSig:
				{
					// Discard data.
				} break;
				case LvbPalFileSig:
				{
					memcpy(s_levelState.levelPaletteName, &data[offset], header.size);
					s_levelState.levelPaletteName[header.size] = 0;

					// HACK!
					strcpy(s_levelState.levelPaletteName, "SECBASE.PAL");

					level_loadPalette();
				} break;
				case LvbMusicFileSig:
				{
					// Discard data.
				} break;
				case LvbLevelInfoSig:
				{
					s_levelState.parallax0 = *((fixed16_16*)&data[offset]);
					s_levelState.parallax1 = *((fixed16_16*)&data[offset + 4]);
				} break;
				case LvbTexListSig:
				{
					level_loadTextureListBin(&header, offset, data);
				} break;
				case LvbSectorListSig:
				{
					level_loadSectorsBin(&header, offset, data);
				} break;
			}
			offset += chunkSize;
		}

		return true;
	}
}

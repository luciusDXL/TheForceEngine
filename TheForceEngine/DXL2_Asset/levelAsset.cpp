#include "levelAsset.h"
#include <DXL2_System/system.h>
#include <DXL2_Asset/assetSystem.h>
#include <DXL2_Archive/archive.h>
#include <DXL2_System/parser.h>
#include <DXL2_FileSystem/filestream.h>
#include <assert.h>
#include <algorithm>

namespace DXL2_LevelAsset
{
	static std::vector<char> s_buffer;
	static LevelData s_data = {};
	static const char* c_defaultGob = "DARK.GOB";

	bool parseLevel();
	void applyFixups(const char* name);

	bool load(const char* name)
	{
		char gobPath[DXL2_MAX_PATH];
		DXL2_Paths::appendPath(PATH_SOURCE_DATA, c_defaultGob, gobPath);
		// Unload the current level.
		unload();

		DXL2_System::logWrite(LOG_MSG, "Level", "Loading level \"%s\"", name);

		if (!DXL2_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, name, s_buffer))
		{
			return false;
		}

		// Parse the file...
		if (!parseLevel())
		{
			DXL2_System::logWrite(LOG_ERROR, "Level", "Failed to parse LEV file \"%s\" from GOB \"%s\"", name, gobPath);
			unload();
			return false;
		}
		// Apply any last minute fix-ups.
		applyFixups(name);

		// palette should be the same as the base.
		strcpy(s_data.palette, name);
		const size_t len = strlen(name);
		s_data.palette[len - 3] = 'P';
		s_data.palette[len - 2] = 'A';
		s_data.palette[len - 1] = 'L';
				
		return true;
	}

	void unload()
	{
		s_data.sectors.clear();
		s_data.textures.clear();
		s_data.vertices.clear();
		s_data.walls.clear();
		s_data.name[0] = 0;

		s_data.layerMin = 127;
		s_data.layerMax = -127;
	}

	void save(const char* name, const char* path)
	{
		FileStream outFile;
		if (!outFile.open(path, FileStream::MODE_WRITE))
		{
			return;
		}

		outFile.writeString("LEV 2.1\r\n");
		outFile.writeString("#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\r\n");
		outFile.writeString("# This file has been written from the Force Engine file '%s'.\r\n", name);
		outFile.writeString("#\r\n");
		outFile.writeString("#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\r\n");

		outFile.writeString("LEVELNAME %s\r\n", name);
		outFile.writeString("PALETTE %s.PAL\r\n", name);
		outFile.writeString("MUSIC AVENGE.GMD\r\n");
		outFile.writeString("PARALLAX %04.4f %04.4f\r\n", s_data.parallax[0], s_data.parallax[1]);

		const u32 texCount = (u32)s_data.textures.size();
		outFile.writeString("#`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~\r\n");
		outFile.writeString("#               T e x t u r e   T a b l e\r\n");
		outFile.writeString("#\r\n");
		outFile.writeString("#`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~`~\r\n");
		outFile.writeString("TEXTURES %u\r\n", texCount);
		const Texture* const* textures = s_data.textures.data();
		for (u32 i = 0; i < texCount; i++)
		{
			outFile.writeString("  TEXTURE: %s			#%u\r\n", textures[i]->name, i);
		}

		const u32 sectorCount = (u32)s_data.sectors.size();
		outFile.writeString("\r\n\r\nNUMSECTORS %u\r\n", sectorCount);

		Sector* sector = s_data.sectors.data();
		for (u32 s = 0; s < sectorCount; s++, sector++)
		{
			outFile.writeString("\r\n# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\r\n");
			outFile.writeString("#               S e c t o r   D e f i n i t i o n\r\n");
			outFile.writeString("# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\r\n");

			outFile.writeString("SECTOR %u\r\n", sector->id);
			outFile.writeString("  NAME %s\r\n", sector->name);
			outFile.writeString("  AMBIENT %u\r\n", sector->ambient);
			outFile.writeString("  FLOOR TEXTURE %u %2.2f %2.2f %u\r\n", sector->floorTexture.texId, sector->floorTexture.baseOffsetX, sector->floorTexture.baseOffsetY, sector->floorTexture.flag);
			outFile.writeString("  FLOOR ALTITUDE %2.2f\r\n", sector->floorAlt);
			outFile.writeString("  CEILING TEXTURE %u %2.2f %2.2f %u\r\n", sector->ceilTexture.texId, sector->ceilTexture.baseOffsetX, sector->ceilTexture.baseOffsetY, sector->ceilTexture.flag);
			outFile.writeString("  CEILING ALTITUDE %2.2f\r\n", sector->ceilAlt);
			outFile.writeString("  SECOND ALTITUDE %2.2f\r\n", sector->secAlt);
			outFile.writeString("  FLAGS %u %u %u\r\n", sector->flags[0], sector->flags[1], sector->flags[2]);
			outFile.writeString("  LAYER %d\r\n", sector->layer);

			outFile.writeString("\r\n\r\n  VERTICES %u\r\n", sector->vtxCount);
			const Vec2f* vtx = s_data.vertices.data() + sector->vtxOffset;
			for (u32 v = 0; v < sector->vtxCount; v++, vtx++)
			{
				outFile.writeString("    X: %2.2f	Z: %2.2f	#  %u\r\n", vtx->x, vtx->z, v);
			}

			outFile.writeString("\r\n\r\n  WALLS %u\r\n", sector->wallCount);
			const SectorWall* wall = s_data.walls.data() + sector->wallOffset;
			for (u32 w = 0; w < sector->wallCount; w++, wall++)
			{
				outFile.writeString("    WALL LEFT: %u RIGHT: %u MID: %d %2.2f %2.2f %u TOP: %d %2.2f %2.2f %u BOT: %d %2.2f %2.2f %u SIGN: %d %2.2f %2.2f ADJOIN: %d MIRROR: %d WALK: %d FLAGS: %u %u %u LIGHT: %u\r\n",
					wall->i0, wall->i1, wall->mid.texId, wall->mid.baseOffsetX, wall->mid.baseOffsetY, wall->mid.flag,
					wall->top.texId, wall->top.baseOffsetX, wall->top.baseOffsetY, wall->top.flag,
					wall->bot.texId, wall->bot.baseOffsetX, wall->bot.baseOffsetY, wall->bot.flag,
					wall->sign.texId, wall->sign.baseOffsetX, wall->sign.baseOffsetY, wall->adjoin, wall->mirror, wall->walk, wall->flags[0], wall->flags[1], wall->flags[2], u16(wall->light));
			}
		}

		outFile.close();
	}

	const char* getName()
	{
		return s_data.name;
	}

	LevelData* getLevelData()
	{
		return &s_data;
	}

	u32 getSectorsByName(const char* name, u32 maxCount, u32* sectorMatches)
	{
		// Usually there is only one sector per name but occassionally there are more than one.
		u32 matchCount = 0;
		const u32 count = (u32)s_data.sectors.size();
		const Sector* sector = s_data.sectors.data();
		for (u32 i = 0; i < count; i++, sector++)
		{
			if (strcasecmp(name, sector->name) == 0)
			{
				sectorMatches[matchCount++] = sector->id;
				if (matchCount == maxCount)
				{
					break;
				}
			}
		}
		return matchCount;
	}

	void applyFixups(const char* name)
	{
		// Fixups - fix errors in the original data rather than hacking up the systems such as INF.
		if (strcasecmp(name, "ROBOTICS.LEV") == 0)
		{
			if (strcasecmp(s_data.sectors[376].name, "pitroom2") == 0 && s_data.sectors[376].flags[0] == 2049 && s_data.sectors[376].wallCount == 11)
			{
				// This wall was flagged to scroll but this was clearly meant for a neighboring wall and this is fixed in the original EXE as a hack.
				// The flags were set on wall 9 and were clearly meant for wall 8.
				SectorWall* wall = &s_data.walls[s_data.sectors[376].wallOffset + 9];
				if (wall->flags[0] == 448 && wall->adjoin < 0)
				{
					wall->flags[0] = 0;
					// Move the flag to the intended wall.
					wall = &s_data.walls[s_data.sectors[376].wallOffset + 8];
					wall->flags[0] = 448;
				}
			}
		}
	}

	bool parseLevel()
	{
		if (s_buffer.empty()) { return false; }

		const size_t len = s_buffer.size();

		DXL2_Parser parser;
		size_t bufferPos = 0;
		parser.init(s_buffer.data(), len);
		parser.addCommentString("//");
		parser.addCommentString("#");
		parser.enableBlockComments();

		s32 curSectorIndex = -1;
		u32 vtxIndex = 0, wallIndex = 0;
		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }

			TokenList tokens;
			parser.tokenizeLine(line, tokens);
			if (tokens.size() < 1) { continue; }

			char* endPtr = nullptr;
			if (strcasecmp("LEVELNAME", tokens[0].c_str()) == 0)
			{
				strcpy(s_data.name, tokens[1].c_str());
			}
			else if (strcasecmp("PALETTE", tokens[0].c_str()) == 0)
			{
				strcpy(s_data.palette, tokens[1].c_str());
			}
			else if (strcasecmp("MUSIC", tokens[0].c_str()) == 0)
			{
				strcpy(s_data.music, tokens[1].c_str());
			}
			else if (strcasecmp("PARALLAX", tokens[0].c_str()) == 0)
			{
				s_data.parallax[0] = (f32)strtod(tokens[1].c_str(), &endPtr);
				s_data.parallax[1] = (f32)strtod(tokens[2].c_str(), &endPtr);
			}
			else if (strcasecmp("TEXTURES", tokens[0].c_str()) == 0)
			{
				const u32 count = strtoul(tokens[1].c_str(), &endPtr, 10);
				s_data.textures.reserve(count);
			}
			else if (strcasecmp("TEXTURE:", tokens[0].c_str()) == 0)
			{
				if (tokens.size() < 2)
				{
					s_data.textures.push_back(nullptr);
				}
				else
				{
					Texture* texture = DXL2_Texture::get(tokens[1].c_str());
					s_data.textures.push_back(texture);
				}
			}
			else if (strcasecmp("NUMSECTORS", tokens[0].c_str()) == 0)
			{
				const u32 count = strtoul(tokens[1].c_str(), &endPtr, 10);
				s_data.sectors.resize(count);
			}
			else if (strcasecmp("SECTOR", tokens[0].c_str()) == 0)
			{
				curSectorIndex++;
				// Some levels have IDs that don't match indices, so just use the index for id [id = strtoul(tokens[1].c_str(), &endPtr, 10)]
				if (curSectorIndex >= s_data.sectors.size())
				{
					s_data.sectors.resize(curSectorIndex + 1);
				}
				s_data.sectors[curSectorIndex].id = curSectorIndex;
			}
			else if (strcasecmp("NAME", tokens[0].c_str()) == 0)
			{
				if (tokens.size() > 1)
				{
					strcpy(s_data.sectors[curSectorIndex].name, tokens[1].c_str());
				}
				else
				{
					s_data.sectors[curSectorIndex].name[0] = 0;
				}
			}
			else if (strcasecmp("AMBIENT", tokens[0].c_str()) == 0)
			{
				s_data.sectors[curSectorIndex].ambient = (u8)strtoul(tokens[1].c_str(), &endPtr, 10);
			}
			else if (tokens.size() >= 6 && strcasecmp("FLOOR", tokens[0].c_str()) == 0 && strcasecmp("TEXTURE", tokens[1].c_str()) == 0)
			{
				s_data.sectors[curSectorIndex].floorTexture.texId = (u16)strtoul(tokens[2].c_str(), &endPtr, 10);
				s_data.sectors[curSectorIndex].floorTexture.offsetX = (f32)strtod(tokens[3].c_str(), &endPtr);
				s_data.sectors[curSectorIndex].floorTexture.offsetY = (f32)strtod(tokens[4].c_str(), &endPtr);
				s_data.sectors[curSectorIndex].floorTexture.baseOffsetX = s_data.sectors[curSectorIndex].floorTexture.offsetX;
				s_data.sectors[curSectorIndex].floorTexture.baseOffsetY = s_data.sectors[curSectorIndex].floorTexture.offsetY;
				s_data.sectors[curSectorIndex].floorTexture.flag = (u16)strtoul(tokens[2].c_str(), &endPtr, 10);
				s_data.sectors[curSectorIndex].floorTexture.frame = 0;
			}
			else if (tokens.size() >= 6 && strcasecmp("CEILING", tokens[0].c_str()) == 0 && strcasecmp("TEXTURE", tokens[1].c_str()) == 0)
			{
				s_data.sectors[curSectorIndex].ceilTexture.texId = (u16)strtoul(tokens[2].c_str(), &endPtr, 10);
				s_data.sectors[curSectorIndex].ceilTexture.offsetX = (f32)strtod(tokens[3].c_str(), &endPtr);
				s_data.sectors[curSectorIndex].ceilTexture.offsetY = (f32)strtod(tokens[4].c_str(), &endPtr);
				s_data.sectors[curSectorIndex].ceilTexture.baseOffsetX = s_data.sectors[curSectorIndex].ceilTexture.offsetX;
				s_data.sectors[curSectorIndex].ceilTexture.baseOffsetY = s_data.sectors[curSectorIndex].ceilTexture.offsetY;
				s_data.sectors[curSectorIndex].ceilTexture.flag = (u16)strtoul(tokens[2].c_str(), &endPtr, 10);
				s_data.sectors[curSectorIndex].ceilTexture.frame = 0;
			}
			else if (strcasecmp("FLOOR", tokens[0].c_str()) == 0 && strcasecmp("ALTITUDE", tokens[1].c_str()) == 0)
			{
				s_data.sectors[curSectorIndex].floorAlt = (f32)strtod(tokens[2].c_str(), &endPtr);
			}
			else if (strcasecmp("CEILING", tokens[0].c_str()) == 0 && strcasecmp("ALTITUDE", tokens[1].c_str()) == 0)
			{
				s_data.sectors[curSectorIndex].ceilAlt = (f32)strtod(tokens[2].c_str(), &endPtr);
			}
			else if (strcasecmp("SECOND", tokens[0].c_str()) == 0 && strcasecmp("ALTITUDE", tokens[1].c_str()) == 0)
			{
				s_data.sectors[curSectorIndex].secAlt = (f32)strtod(tokens[2].c_str(), &endPtr);
			}
			else if (strcasecmp("FLAGS", tokens[0].c_str()) == 0)
			{
				if (tokens.size() >= 2)
					s_data.sectors[curSectorIndex].flags[0] = strtoul(tokens[1].c_str(), &endPtr, 10);
				if (tokens.size() >= 3)
					s_data.sectors[curSectorIndex].flags[1] = strtoul(tokens[2].c_str(), &endPtr, 10);
				if (tokens.size() >= 4)
					s_data.sectors[curSectorIndex].flags[2] = strtoul(tokens[3].c_str(), &endPtr, 10);
			}
			else if (strcasecmp("LAYER", tokens[0].c_str()) == 0)
			{
				s_data.sectors[curSectorIndex].layer = (s8)strtol(tokens[1].c_str(), &endPtr, 10);
				s_data.layerMin = std::min(s_data.layerMin, s_data.sectors[curSectorIndex].layer);
				s_data.layerMax = std::max(s_data.layerMax, s_data.sectors[curSectorIndex].layer);
			}
			else if (strcasecmp("VERTICES", tokens[0].c_str()) == 0)
			{
				s_data.sectors[curSectorIndex].vtxCount = (u8)strtoul(tokens[1].c_str(), &endPtr, 10);
				s_data.sectors[curSectorIndex].vtxOffset = (curSectorIndex == 0) ? 0 : s_data.sectors[curSectorIndex - 1].vtxOffset + s_data.sectors[curSectorIndex - 1].vtxCount;
				vtxIndex = s_data.sectors[curSectorIndex].vtxOffset;

				s_data.vertices.resize(vtxIndex + s_data.sectors[curSectorIndex].vtxCount);
			}
			else if (strcasecmp("X:", tokens[0].c_str()) == 0)
			{
				if (vtxIndex >= s_data.vertices.size())
				{
					s_data.vertices.push_back({});
				}

				s_data.vertices[vtxIndex].x = (f32)strtod(tokens[1].c_str(), &endPtr);
				assert(tokens[2] == "Z:");
				s_data.vertices[vtxIndex].z = (f32)strtod(tokens[3].c_str(), &endPtr);
				vtxIndex++;
			}
			else if (strcasecmp("WALLS", tokens[0].c_str()) == 0)
			{
				s_data.sectors[curSectorIndex].wallCount = (u8)strtoul(tokens[1].c_str(), &endPtr, 10);
				s_data.sectors[curSectorIndex].wallOffset = (curSectorIndex == 0) ? 0 : s_data.sectors[curSectorIndex - 1].wallOffset + s_data.sectors[curSectorIndex - 1].wallCount;
				wallIndex = s_data.sectors[curSectorIndex].wallOffset;

				s_data.walls.resize(wallIndex + s_data.sectors[curSectorIndex].wallCount);
			}
			else if (strcasecmp("WALL", tokens[0].c_str()) == 0 && strcasecmp("LEFT:", tokens[1].c_str()) == 0)
			{
				if (wallIndex >= s_data.walls.size())
				{
					s_data.walls.push_back({});
				}

				s_data.walls[wallIndex].i0 = (u16)strtoul(tokens[2].c_str(), &endPtr, 10);
				assert(tokens[3] == "RIGHT:");
				s_data.walls[wallIndex].i1 = (u16)strtoul(tokens[4].c_str(), &endPtr, 10);
				assert(tokens[5] == "MID:");
				s_data.walls[wallIndex].mid.texId = (s16)strtol(tokens[6].c_str(), &endPtr, 10);
				s_data.walls[wallIndex].mid.offsetX = (f32)strtod(tokens[7].c_str(), &endPtr);
				s_data.walls[wallIndex].mid.offsetY = (f32)strtod(tokens[8].c_str(), &endPtr);
				s_data.walls[wallIndex].mid.baseOffsetX = s_data.walls[wallIndex].mid.offsetX;
				s_data.walls[wallIndex].mid.baseOffsetY = s_data.walls[wallIndex].mid.offsetY;
				s_data.walls[wallIndex].mid.flag = (s16)strtol(tokens[9].c_str(), &endPtr, 10);
				s_data.walls[wallIndex].mid.frame = 0;
				assert(tokens[10] == "TOP:");
				s_data.walls[wallIndex].top.texId = (s16)strtol(tokens[11].c_str(), &endPtr, 10);
				s_data.walls[wallIndex].top.offsetX = (f32)strtod(tokens[12].c_str(), &endPtr);
				s_data.walls[wallIndex].top.offsetY = (f32)strtod(tokens[13].c_str(), &endPtr);
				s_data.walls[wallIndex].top.baseOffsetX = s_data.walls[wallIndex].top.offsetX;
				s_data.walls[wallIndex].top.baseOffsetY = s_data.walls[wallIndex].top.offsetY;
				s_data.walls[wallIndex].top.flag = (s16)strtol(tokens[14].c_str(), &endPtr, 10);
				s_data.walls[wallIndex].top.frame = 0;
				assert(tokens[15] == "BOT:");
				s_data.walls[wallIndex].bot.texId = (s16)strtol(tokens[16].c_str(), &endPtr, 10);
				s_data.walls[wallIndex].bot.offsetX = (f32)strtod(tokens[17].c_str(), &endPtr);
				s_data.walls[wallIndex].bot.offsetY = (f32)strtod(tokens[18].c_str(), &endPtr);
				s_data.walls[wallIndex].bot.baseOffsetX = s_data.walls[wallIndex].bot.offsetX;
				s_data.walls[wallIndex].bot.baseOffsetY = s_data.walls[wallIndex].bot.offsetY;
				s_data.walls[wallIndex].bot.flag = (s16)strtol(tokens[19].c_str(), &endPtr, 10);
				s_data.walls[wallIndex].bot.frame = 0;
				assert(tokens[20] == "SIGN:");
				s_data.walls[wallIndex].sign.texId = (s16)strtol(tokens[21].c_str(), &endPtr, 10);
				s_data.walls[wallIndex].sign.offsetX = (f32)strtod(tokens[22].c_str(), &endPtr);
				s_data.walls[wallIndex].sign.offsetY = (f32)strtod(tokens[23].c_str(), &endPtr);
				s_data.walls[wallIndex].sign.baseOffsetX = s_data.walls[wallIndex].sign.offsetX;
				s_data.walls[wallIndex].sign.baseOffsetY = s_data.walls[wallIndex].sign.offsetY;
				s_data.walls[wallIndex].sign.flag = 0;
				s_data.walls[wallIndex].sign.frame = 0;
				assert(tokens[24] == "ADJOIN:");
				s_data.walls[wallIndex].adjoin = (s32)strtol(tokens[25].c_str(), &endPtr, 10);
				assert(tokens[26] == "MIRROR:");
				s_data.walls[wallIndex].mirror = (s32)strtol(tokens[27].c_str(), &endPtr, 10);
				assert(tokens[28] == "WALK:");
				s_data.walls[wallIndex].walk = (s32)strtol(tokens[29].c_str(), &endPtr, 10);
				assert(tokens[30] == "FLAGS:");
				s_data.walls[wallIndex].flags[0] = strtoul(tokens[31].c_str(), &endPtr, 10);
				s_data.walls[wallIndex].flags[1] = strtoul(tokens[32].c_str(), &endPtr, 10);
				s_data.walls[wallIndex].flags[2] = strtoul(tokens[33].c_str(), &endPtr, 10);
				assert(tokens[34] == "LIGHT:");
				s_data.walls[wallIndex].light = (s16)strtoul(tokens[35].c_str(), &endPtr, 10);

				wallIndex++;
			}
		}
		return true;
	}
};

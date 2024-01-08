#include "levelEditorData.h"
#include "selection.h"
#include "error.h"
#include "shell.h"
#include <TFE_Editor/history.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/editorLevel.h>
#include <TFE_Editor/editorProject.h>
#include <TFE_Editor/editorResources.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Level/levelData.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_DarkForces/mission.h>
#include <TFE_Input/input.h>
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <climits>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;
using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	extern char s_colormapName[TFE_MAX_PATH];
	extern u8* s_levelColorMap;
	extern void mission_loadColormap();
}

namespace LevelEditor
{
	static std::vector<u8> s_fileData;
	static u32* s_palette = nullptr;
	static s32 s_palIndex = 0;

	static s32 s_curSnapshotId = -1;
	static EditorLevel s_curSnapshot;
	static SnapshotBuffer* s_buffer = nullptr;
	static const u8* s_readBuffer;

	EditorLevel s_level = {};

	extern AssetList s_levelTextureList;
	extern void edit_clearSelections();
	bool loadFromTFL(const char* name);

	enum Constants
	{
		LevVersionMin = 15,
		LevVersionMax = 21,
		LevVersion_Layers_WallLight = 21,
	};
	enum LevelEditorFormat
	{
		LEF_MinVersion = 1,
		LEF_EntityV1   = 2,
		LEF_EntityV2   = 3,
		LEF_CurVersion = 3,
	};

	AssetHandle loadTexture(const char* bmTextureName)
	{
		Asset* texAsset = AssetBrowser::findAsset(bmTextureName, TYPE_TEXTURE);
		if (!texAsset) { return NULL_ASSET; }
		return AssetBrowser::loadAssetData(texAsset);
	}
		
	AssetHandle loadPalette(const char* paletteName)
	{
		Asset* palAsset = AssetBrowser::findAsset(paletteName, TYPE_PALETTE);
		if (!palAsset) { return NULL_ASSET; }
		return AssetBrowser::loadAssetData(palAsset);
	}

	AssetHandle loadColormap(const char* colormapName)
	{
		Asset* colormapAsset = AssetBrowser::findAsset(colormapName, TYPE_COLORMAP);
		if (!colormapAsset) { return NULL_ASSET; }
		return AssetBrowser::loadAssetData(colormapAsset);
	}
		
	bool loadLevelFromAsset(Asset* asset)
	{
		EditorLevel* level = &s_level;
		char slotName[256];
		FileUtil::stripExtension(asset->name.c_str(), slotName);

		// First check to see if there is a "tfl" version of the level.
		if (loadFromTFL(slotName))
		{
			return true;
		}
		level->slot = slotName;

		s_fileData.clear();
		if (asset->archive)
		{
			if (asset->archive->openFile(asset->name.c_str()))
			{
				const size_t len = asset->archive->getFileLength();
				s_fileData.resize(len);
				asset->archive->readFile(s_fileData.data(), len);
				asset->archive->closeFile();
			}
		}
		else
		{
			FileStream file;
			if (file.open(asset->filePath.c_str(), Stream::MODE_READ))
			{
				const size_t len = file.getSize();
				s_fileData.resize(len);
				file.readBuffer(s_fileData.data(), (u32)len);
				file.close();
			}
		}

		if (s_fileData.empty()) { return false; }

		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init((char*)s_fileData.data(), s_fileData.size());
		parser.addCommentString("#");
		parser.convertToUpperCase(true);

		const char* line;
		line = parser.readLine(bufferPos);
		s32 versionMajor, versionMinor;
		if (sscanf(line, " LEV %d.%d", &versionMajor, &versionMinor) != 2)
		{
			return false;
		}
		// Check the version number. The editor actually supports a range of versions.
		s32 version = versionMajor * 10 + versionMinor;
		if (version < LevVersionMin || version > LevVersionMax)
		{
			return false;
		}

		char readBuffer[256];
		line = parser.readLine(bufferPos);
		if (sscanf(line, " LEVELNAME %s", readBuffer) != 1)
		{
			return false;
		}
		level->name = readBuffer;
		
		line = parser.readLine(bufferPos);
		if (sscanf(line, " PALETTE %s", readBuffer) != 1)
		{
			return false;
		}
		// Fixup the palette, strip any path.
		char filename[256];
		FileUtil::getFileNameFromPath(readBuffer, filename, true);
		level->palette = filename;

		// Another value that is ignored.
		line = parser.readLine(bufferPos);
		if (sscanf(line, " MUSIC %s", readBuffer) == 1)
		{
			line = parser.readLine(bufferPos);
		}

		// Sky Parallax - this option until version 1.9, so handle its absence.
		if (sscanf(line, " PARALLAX %f %f", &level->parallax.x, &level->parallax.z) != 2)
		{
			level->parallax = { 1024.0f, 1024.0f };
		}
		else
		{
			line = parser.readLine(bufferPos);
		}

		// Number of textures used by the level.
		s32 textureCount = 0;
		if (sscanf(line, " TEXTURES %d", &textureCount) != 1)
		{
			return false;
		}
		level->textures.resize(textureCount);

		// Read texture names.
		char textureName[256];
		for (s32 i = 0; i < textureCount; i++)
		{
			line = parser.readLine(bufferPos);
			if (sscanf(line, " TEXTURE: %s ", textureName) != 1)
			{
				strcpy(textureName, "DEFAULT.BM");
			}

			char bmTextureName[32];
			FileUtil::replaceExtension(textureName, "BM", bmTextureName);
			level->textures[i] = { bmTextureName, loadTexture(bmTextureName) };
		}
		// Sometimes there are extra textures, just add them - they will be compacted later.
		bool readNext = true;
		while (sscanf(line, " TEXTURE: %s ", textureName) == 1)
		{
			char bmTextureName[32];
			FileUtil::replaceExtension(textureName, "BM", bmTextureName);
			level->textures.push_back({ bmTextureName, loadTexture(bmTextureName) });

			line = parser.readLine(bufferPos);
			readNext = false;
		}
		
		// Load Sectors.
		if (readNext)
		{
			line = parser.readLine(bufferPos);
		}
		s32 sectorCount = 0;
		if (sscanf(line, "NUMSECTORS %d", &sectorCount) != 1)
		{
			return false;
		}
		level->sectors.resize(sectorCount);

		EditorSector* sector = level->sectors.data();
		for (s32 i = 0; i < sectorCount; i++, sector++)
		{
			*sector = {};

			// Sector ID and Name
			line = parser.readLine(bufferPos);
			if (sscanf(line, " SECTOR %d", &sector->id) != 1)
			{
				return false;
			}

			// Allow names to have '#' in them.
			line = parser.readLine(bufferPos, false, true);
			// Sectors missing a name are valid but do not get "addresses" - and thus cannot be
			// used by the INF system (except in the case of doors and exploding walls, see the flags section below).
			char name[256];
			if (sscanf(line, " NAME %s", name) == 1)
			{
				sector->name = name;
			}

			// Lighting
			line = parser.readLine(bufferPos);
			if (sscanf(line, " AMBIENT %d", &sector->ambient) != 1)
			{
				return false;
			}

			s32 floorTexId, ceilTexId;
			// Floor Texture & Offset
			line = parser.readLine(bufferPos);
			s32 tmp;
			if (sscanf(line, " FLOOR TEXTURE %d %f %f %d", &floorTexId, &sector->floorTex.offset.x, &sector->floorTex.offset.z, &tmp) != 4)
			{
				return false;
			}
			line = parser.readLine(bufferPos);
			if (sscanf(line, " FLOOR ALTITUDE %f", &sector->floorHeight) != 1)
			{
				return false;
			}
			sector->floorTex.texIndex = floorTexId;
			
			// Ceiling Texture & Offset
			line = parser.readLine(bufferPos);
			if (sscanf(line, " CEILING TEXTURE %d %f %f %d", &ceilTexId, &sector->ceilTex.offset.x, &sector->ceilTex.offset.z, &tmp) != 4)
			{
				return false;
			}
			line = parser.readLine(bufferPos);
			if (sscanf(line, " CEILING ALTITUDE %f", &sector->ceilHeight) != 1)
			{
				return false;
			}
			sector->ceilTex.texIndex = ceilTexId;

			// Second Altitude
			line = parser.readLine(bufferPos);
			if (sscanf(line, " SECOND ALTITUDE %f", &sector->secHeight) == 1)
			{
				// Second altitude was added in version 1.7, so is optional before then.
				line = parser.readLine(bufferPos);
			}

			// Note: the editor works with +Y up, so negate heights.
			if (sector->floorHeight != 0.0f) { sector->floorHeight = -sector->floorHeight; }
			if (sector->ceilHeight  != 0.0f) { sector->ceilHeight  = -sector->ceilHeight; }
			if (sector->secHeight   != 0.0f) { sector->secHeight   = -sector->secHeight; }

			// Sector flags
			if (sscanf(line, " FLAGS %d %d %d", &sector->flags[0], &sector->flags[1], &sector->flags[2]) != 3)
			{
				return false;
			}

			// Optional layer
			line = parser.readLine(bufferPos);
			if (sscanf(line, " LAYER %d", &sector->layer) == 1)
			{
				// Not all versions have a layer.
				line = parser.readLine(bufferPos);
			}

			// Vertices
			s32 vertexCount = 0;
			if (sscanf(line, " VERTICES %d", &vertexCount) != 1)
			{
				return false;
			}

			sector->bounds[0] = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
			sector->bounds[1] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
			sector->bounds[0].y = min(sector->floorHeight, sector->ceilHeight);
			sector->bounds[1].y = max(sector->floorHeight, sector->ceilHeight);

			sector->vtx.resize(vertexCount);
			Vec2f* vtx = sector->vtx.data();
			for (s32 v = 0; v < vertexCount; v++, vtx++)
			{
				line = parser.readLine(bufferPos);
				sscanf(line, " X: %f Z: %f ", &vtx->x, &vtx->z);

				sector->bounds[0].x = min(sector->bounds[0].x, vtx->x);
				sector->bounds[0].z = min(sector->bounds[0].z, vtx->z);

				sector->bounds[1].x = max(sector->bounds[1].x, vtx->x);
				sector->bounds[1].z = max(sector->bounds[1].z, vtx->z);
			}

			line = parser.readLine(bufferPos);
			s32 wallCount = 0;
			if (sscanf(line, " WALLS %d", &wallCount) != 1)
			{
				return false;
			}
			sector->walls.resize(wallCount);
			EditorWall* wall = sector->walls.data();
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				memset(wall, 0, sizeof(EditorWall));

				s32 unused, walk;
				// wallLight is optional, so there are 24 parameters, but 23 are required.
				line = parser.readLine(bufferPos);
				s32 texId[WP_COUNT] = { 0 };
				if (sscanf(line, " WALL LEFT: %d RIGHT: %d MID: %d %f %f %d TOP: %d %f %f %d BOT: %d %f %f %d SIGN: %d %f %f ADJOIN: %d MIRROR: %d WALK: %d FLAGS: %d %d %d LIGHT: %d",
					&wall->idx[0], &wall->idx[1], &texId[WP_MID], &wall->tex[WP_MID].offset.x, &wall->tex[WP_MID].offset.z, &unused,
					&texId[WP_TOP], &wall->tex[WP_TOP].offset.x, &wall->tex[WP_TOP].offset.z, &unused,
					&texId[WP_BOT], &wall->tex[WP_BOT].offset.x, &wall->tex[WP_BOT].offset.z, &unused,
					&texId[WP_SIGN], &wall->tex[WP_SIGN].offset.x, &wall->tex[WP_SIGN].offset.z,
					&wall->adjoinId, &wall->mirrorId, &walk, &wall->flags[0], &wall->flags[1], &wall->flags[2], &wall->wallLight) < 23)
				{
					return false;
				}

				if (wall->wallLight >= 32768)
				{
					wall->wallLight -= 65536;
				}

				wall->tex[WP_MID].texIndex = texId[WP_MID] >= 0 ? texId[WP_MID] : -1;
				wall->tex[WP_TOP].texIndex = texId[WP_TOP] >= 0 ? texId[WP_TOP] : -1;
				wall->tex[WP_BOT].texIndex = texId[WP_BOT] >= 0 ? texId[WP_BOT] : -1;
				wall->tex[WP_SIGN].texIndex = texId[WP_SIGN] >= 0 ? texId[WP_SIGN] : -1;

				if (wall->tex[WP_SIGN].texIndex < 0)
				{
					wall->tex[WP_SIGN].offset = { 0 };
				}
			}
		}

		// Original format level, so default to vanilla.
		level->featureSet = FSET_VANILLA;
		
		// Compute the bounds.
		level->bounds[0] = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
		level->bounds[1] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		level->layerRange[0] =  INT_MAX;
		level->layerRange[1] = -INT_MAX;
		const size_t count = level->sectors.size();
		sector = level->sectors.data();
		for (size_t i = 0; i < count; i++, sector++)
		{
			level->bounds[0].x = min(level->bounds[0].x, sector->bounds[0].x);
			level->bounds[0].y = min(level->bounds[0].y, sector->bounds[0].y);
			level->bounds[0].z = min(level->bounds[0].z, sector->bounds[0].z);

			level->bounds[1].x = max(level->bounds[1].x, sector->bounds[1].x);
			level->bounds[1].y = max(level->bounds[1].y, sector->bounds[1].y);
			level->bounds[1].z = max(level->bounds[1].z, sector->bounds[1].z);

			level->layerRange[0] = min(level->layerRange[0], sector->layer);
			level->layerRange[1] = max(level->layerRange[1], sector->layer);

			sectorToPolygon(sector);
		}

		return true;
	}

	bool loadFromTFL(const char* name)
	{
		// If there is no project, then the TFL can't exist.
		Project* project = project_get();
		if (!project)
		{
			return false;
		}
		char filePath[TFE_MAX_PATH];
		sprintf(filePath, "%s/%s.tfl", project->path, name);

		// Then try to open it based on the path, if it fails load the LEV file.
		FileStream file;
		if (!file.open(filePath, FileStream::MODE_READ))
		{
			return false;
		}

		// Check the version.
		u32 version;
		file.read(&version);
		if (version < LEF_MinVersion || version > LEF_CurVersion)
		{
			file.close();
			return false;
		}

		// Load the level.
		file.read(&s_level.name);
		file.read(&s_level.slot);
		file.read(&s_level.palette);
		file.readBuffer(&s_level.featureSet, sizeof(TFE_Editor::FeatureSet));
		file.readBuffer(&s_level.parallax, sizeof(Vec2f));
		file.readBuffer(&s_level.bounds, sizeof(Vec3f) * 2);
		file.readBuffer(&s_level.layerRange, sizeof(s32) * 2);

		// Textures.
		u32 textureCount;
		file.read(&textureCount);
		s_level.textures.resize(textureCount);
		LevelTextureAsset* tex = s_level.textures.data();
		for (u32 i = 0; i < textureCount; i++)
		{
			file.read(&tex[i].name);
			tex[i].handle = loadTexture(tex[i].name.c_str());
		}

		// Sectors.
		u32 sectorCount;
		file.read(&sectorCount);
		s_level.sectors.resize(sectorCount);
		EditorSector* sector = s_level.sectors.data();
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			file.read(&sector->id);
			file.read(&sector->name);
			file.readBuffer(&sector->floorTex, sizeof(LevelTexture));
			file.readBuffer(&sector->ceilTex, sizeof(LevelTexture));
			file.read(&sector->floorHeight);
			file.read(&sector->ceilHeight);
			file.read(&sector->secHeight);
			file.read(&sector->ambient);
			file.read(sector->flags, 3);

			u32 vtxCount;
			file.read(&vtxCount);
			sector->vtx.resize(vtxCount);
			file.readBuffer(sector->vtx.data(), sizeof(Vec2f), vtxCount);

			u32 wallCount;
			file.read(&wallCount);
			sector->walls.resize(wallCount);
			file.readBuffer(sector->walls.data(), sizeof(EditorWall), wallCount);

			file.readBuffer(sector->bounds, sizeof(Vec3f), 2);
			file.read(&sector->layer);

			if (version >= LEF_EntityV1)
			{
				s32 objCount;
				file.read(&objCount);
				sector->obj.resize(objCount);

				EditorObject* obj = sector->obj.data();
				for (s32 o = 0; o < objCount; o++, obj++)
				{
					file.read(&obj->entityId);
					file.read(&obj->angle);
					obj->diff = 1; // default
					if (version >= LEF_EntityV2)
					{
						file.read(&obj->diff);
					}
					file.readBuffer(&obj->pos, sizeof(Vec3f));
				}
			}

			sector->searchKey = 0;
			sectorToPolygon(sector);
		}
		file.close();

		return true;
	}
		
	// Save in the binary editor format.
	bool saveLevel()
	{
		Project* project = project_get();
		if (!project)
		{
			LE_ERROR("Cannot save if no project is open.");
			return false;
		}

		char filePath[TFE_MAX_PATH];
		sprintf(filePath, "%s/%s.tfl", project->path, s_level.slot.c_str());
		LE_INFO("Saving level to '%s'", filePath);

		FileStream file;
		if (!file.open(filePath, FileStream::MODE_WRITE))
		{
			LE_ERROR("Cannot open '%s' for writing.", filePath);
			return false;
		}

		// Version.
		const u32 version = LEF_CurVersion;
		file.write(&version);

		// Level data.
		file.write(&s_level.name);
		file.write(&s_level.slot);
		file.write(&s_level.palette);
		file.writeBuffer(&s_level.featureSet, sizeof(TFE_Editor::FeatureSet));
		file.writeBuffer(&s_level.parallax, sizeof(Vec2f));
		file.writeBuffer(&s_level.bounds, sizeof(Vec3f) * 2);
		file.writeBuffer(&s_level.layerRange, sizeof(s32) * 2);

		// Textures.
		const u32 textureCount = (u32)s_level.textures.size();
		file.write(&textureCount);
		LevelTextureAsset* tex = s_level.textures.data();
		for (u32 i = 0; i < textureCount; i++)
		{
			file.write(&tex[i].name);
		}

		// Sectors.
		const u32 sectorCount = (u32)s_level.sectors.size();
		file.write(&sectorCount);
		EditorSector* sector = s_level.sectors.data();
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			file.write(&sector->id);
			file.write(&sector->name);
			file.writeBuffer(&sector->floorTex, sizeof(LevelTexture));
			file.writeBuffer(&sector->ceilTex, sizeof(LevelTexture));
			file.write(&sector->floorHeight);
			file.write(&sector->ceilHeight);
			file.write(&sector->secHeight);
			file.write(&sector->ambient);
			file.write(sector->flags, 3);

			u32 vtxCount = (u32)sector->vtx.size();
			file.write(&vtxCount);
			file.writeBuffer(sector->vtx.data(), sizeof(Vec2f), vtxCount);

			u32 wallCount = (u32)sector->walls.size();
			file.write(&wallCount);
			file.writeBuffer(sector->walls.data(), sizeof(EditorWall), wallCount);

			file.writeBuffer(sector->bounds, sizeof(Vec3f), 2);
			file.write(&sector->layer);
			// Polygon is derived on load.
			// Searchkey is reset on load.

			// LEF_EntityV1
			s32 objCount = (s32)sector->obj.size();
			file.write(&objCount);
			EditorObject* obj = sector->obj.data();
			for (s32 o = 0; o < objCount; o++, obj++)
			{
				file.write(&obj->entityId);
				file.write(&obj->angle);
				file.write(&obj->diff);
				file.writeBuffer(&obj->pos, sizeof(Vec3f));
			}
		}
		file.close();

		LE_INFO("Save Complete");
		return true;
	}

	// Export the level to the game format.
	#define WRITE_LINE(...) \
		sprintf(buffer, __VA_ARGS__); \
		file.writeBuffer(buffer, (u32)strlen(buffer));
	#define NEW_LINE() file.writeBuffer(newLine, (u32)strlen(newLine))

	bool exportDfLevel(const char* levFile)
	{
		FileStream file;
		if (!file.open(levFile, FileStream::MODE_WRITE))
		{
			return false;
		}

		char newLine[256];
		strcpy(newLine, "\r\n");

		char buffer[256];
		WRITE_LINE("%s", "LEV 2.1\r\n");
		WRITE_LINE("LEVELNAME %s\r\n", s_level.name.c_str());
		WRITE_LINE("PALETTE %s\r\n", s_level.palette.c_str());
		WRITE_LINE("%s", "MUSIC surfin.GMD\r\n");
		WRITE_LINE("%s", "PARALLAX 1024.0 1024.0\r\n");
		NEW_LINE();

		const s32 textureCount = (s32)s_level.textures.size();
		WRITE_LINE("TEXTURES %d\r\n", textureCount);
		const LevelTextureAsset* tex = s_level.textures.data();
		for (s32 i = 0; i < textureCount; i++, tex++)
		{
			tex->name.c_str();
			WRITE_LINE("  TEXTURE: %s\t\t#  %d\r\n", tex->name.c_str(), i);
		}
		NEW_LINE();

		const s32 sectorCount = (s32)s_level.sectors.size();
		WRITE_LINE("NUMSECTORS %d\r\n", sectorCount);
		NEW_LINE();
		NEW_LINE();

		const EditorSector* sector = s_level.sectors.data();
		for (s32 i = 0; i < sectorCount; i++, sector++)
		{
			WRITE_LINE("SECTOR %d\r\n", sector->id);

			if (sector->name.empty()) { WRITE_LINE("  NAME\r\n"); }
			else { WRITE_LINE("  NAME\t%s\r\n", sector->name.c_str()); }

			WRITE_LINE("  AMBIENT\t%d\r\n", (s32)sector->ambient);
			WRITE_LINE("  FLOOR TEXTURE\t%d\t%0.2f\t%0.2f\t%d\r\n", sector->floorTex.texIndex, sector->floorTex.offset.x, sector->floorTex.offset.z, 2);
			WRITE_LINE("  FLOOR ALTITUDE\t%0.2f\r\n", -sector->floorHeight);
			WRITE_LINE("  CEILING TEXTURE\t%d\t%0.2f\t%0.2f\t%d\r\n", sector->ceilTex.texIndex, sector->ceilTex.offset.x, sector->ceilTex.offset.z, 2);
			WRITE_LINE("  CEILING ALTITUDE\t%0.2f\r\n", -sector->ceilHeight);
			WRITE_LINE("  SECOND ALTITUDE\t%0.2f\r\n", -sector->secHeight);
			WRITE_LINE("  FLAGS %u %u %u\r\n", sector->flags[0], sector->flags[1], sector->flags[2]);
			WRITE_LINE("  LAYER %d\r\n", sector->layer);
			NEW_LINE();

			// Vertices.
			s32 vtxCount = (s32)sector->vtx.size();
			WRITE_LINE("  VERTICES %05d\r\n", vtxCount);
			const Vec2f* vtx = sector->vtx.data();
			for (s32 v = 0; v < vtxCount; v++, vtx++)
			{
				WRITE_LINE("    X: %0.2f\tZ: %0.2f\t#  %d\r\n", vtx->x, vtx->z, v);
			}
			NEW_LINE();

			// Walls
			s32 wallCount = (s32)sector->walls.size();
			WRITE_LINE("  WALLS %d\r\n", wallCount);
			const EditorWall* wall = sector->walls.data();
			for (s32 w = 0; w < wallCount; w++, wall++)
			{
				WRITE_LINE("    WALL LEFT:\t%d  RIGHT:\t%d  MID:\t%d\t%0.2f\t%0.2f\t%d  TOP:\t%d\t%0.2f\t%0.2f\t%d  BOT:\t%d\t%0.2f\t%0.2f\t%d  "
					"SIGN:\t%d\t%0.2f\t%0.2f  ADJOIN:\t%d  MIRROR:\t%d  WALK:\t%d  FLAGS: %d %d %d  LIGHT: %d\r\n", 
					wall->idx[0], wall->idx[1],
					wall->tex[WP_MID].texIndex, wall->tex[WP_MID].offset.x, wall->tex[WP_MID].offset.z, 0,
					wall->tex[WP_TOP].texIndex, wall->tex[WP_TOP].offset.x, wall->tex[WP_TOP].offset.z, 0,
					wall->tex[WP_BOT].texIndex, wall->tex[WP_BOT].offset.x, wall->tex[WP_BOT].offset.z, 0,
					wall->tex[WP_SIGN].texIndex, wall->tex[WP_SIGN].offset.x, wall->tex[WP_SIGN].offset.z,
					wall->adjoinId, wall->mirrorId, wall->adjoinId, wall->flags[0], wall->flags[1], wall->flags[2], wall->wallLight);
			}
			NEW_LINE();
		}

		file.close();
		return true;
	}

	s32 addObjAsset(const std::string& name, std::vector<std::string>& list)
	{
		// Does it already exist?
		const size_t count = list.size();
		const std::string* data = list.data();
		for (size_t i = 0; i < count; i++, data++)
		{
			if (strcasecmp(name.c_str(), data->c_str()) == 0)
			{
				return s32(i);
			}
		}

		s32 newId = (s32)list.size();
		list.push_back(name);
		return newId;
	}

	bool exportDfObj(const char* oFile, const StartPoint* start)
	{
		// For now require the start point.
		if (!start) { return false; }

		FileStream file;
		if (!file.open(oFile, FileStream::MODE_WRITE))
		{
			return false;
		}

		char newLine[256];
		strcpy(newLine, "\r\n");

		char buffer[256];
		WRITE_LINE("%s", "O 1.1\r\n");
		NEW_LINE();

		std::vector<std::string> pods;
		std::vector<std::string> sprites;
		std::vector<std::string> frames;
		std::vector<const EditorObject*> objList;
		std::vector<s32> objData;
		const s32 sectorCount = (s32)s_level.sectors.size();
		const EditorSector* sector = s_level.sectors.data();
		s32 startPointId = -1;
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			const s32 objCount = (s32)sector->obj.size();
			const EditorObject* obj = sector->obj.data();
			for (s32 o = 0; o < objCount; o++, obj++)
			{
				const Entity* entity = &s_entityList[obj->entityId];

				objList.push_back(obj);
				s32 dataIndex = 0;
				// TODO: Handle other types.
				if (entity->type == ETYPE_FRAME || entity->type == ETYPE_SPRITE)
				{
					dataIndex = addObjAsset(entity->assetName, entity->type == ETYPE_FRAME ? frames : sprites);
				}
				else if (entity->type == ETYPE_SPIRIT)
				{
					startPointId = o;
				}
				objData.push_back(dataIndex);
			}
		}

		WRITE_LINE("LEVELNAME %s\r\n", s_level.name.c_str());
		NEW_LINE();

		const s32 podCount = (s32)pods.size();
		WRITE_LINE("PODS %d\r\n", podCount);
		NEW_LINE();

		const s32 spriteCount = (s32)sprites.size();
		WRITE_LINE("SPRS %d\r\n", spriteCount);
		for (s32 i = 0; i < spriteCount; i++)
		{
			WRITE_LINE("    SPR: %s\r\n", sprites[i].c_str());
		}
		NEW_LINE();

		const s32 frameCount = (s32)frames.size();
		WRITE_LINE("FMES %d\r\n", frameCount);
		for (s32 i = 0; i < frameCount; i++)
		{
			WRITE_LINE("    FME: %s\r\n", frames[i].c_str());
		}
		NEW_LINE();

		WRITE_LINE("SOUNDS %d\r\n", 0);
		NEW_LINE();

		// For now just put in a start point.
		const f32 radToDeg = 360.0f / (2.0f * PI);
		const f32 yaw   = fmodf(start->yaw * radToDeg + 180.0f, 360.0f);
		const f32 pitch = start->pitch * radToDeg;
		const f32 y = std::max(start->sector->floorHeight, start->pos.y - 5.8f);

		s32 objCount = (s32)objList.size();
		s32 finalObjCount = objCount;
		if (startPointId < 0) { finalObjCount++; }

		WRITE_LINE("OBJECTS %d\r\n", finalObjCount);

		// Add the start point if needed.
		if (startPointId < 0)
		{
			WRITE_LINE("    CLASS: SPIRIT     DATA: 0   X: %0.2f Y: %0.2f Z: %0.2f PCH: %0.2f   YAW: %0.2f ROL: 0.00   DIFF: 1\r\n", start->pos.x, -y, start->pos.z, pitch, yaw);
			WRITE_LINE("        SEQ\r\n");
			WRITE_LINE("            LOGIC:     PLAYER\r\n");
			WRITE_LINE("            EYE:       TRUE\r\n");
			WRITE_LINE("        SEQEND\r\n");
		}

		for (s32 i = 0; i < objCount; i++)
		{
			const EditorObject* obj = objList[i];
			const Entity* entity = &s_entityList[obj->entityId];
			const f32 objYaw = fmodf(obj->angle * radToDeg, 360.0f);
			const s32 logicCount = (s32)entity->logic.size();

			switch (entity->type)
			{
				case ETYPE_SPIRIT:
				{
					WRITE_LINE("    CLASS: SPIRIT     DATA: 0   X: %0.2f Y: %0.2f Z: %0.2f PCH: %0.2f   YAW: %0.2f ROL: 0.00   DIFF: %d\r\n", start->pos.x, -y, start->pos.z, pitch, yaw, obj->diff);
					WRITE_LINE("        SEQ\r\n");
					WRITE_LINE("            LOGIC:     PLAYER\r\n");
					WRITE_LINE("            EYE:       TRUE\r\n");
					WRITE_LINE("        SEQEND\r\n");
				} break;
				case ETYPE_SAFE:
				{
					WRITE_LINE("    CLASS: SAFE     DATA: 0   X: %0.2f Y: %0.2f Z: %0.2f PCH: %0.2f   YAW: %0.2f ROL: 0.00   DIFF: %d\r\n", obj->pos.x, -obj->pos.y, obj->pos.z, 0.0f, objYaw, obj->diff);
				} break;
				case ETYPE_FRAME:
				{
					WRITE_LINE("    CLASS: FRAME     DATA: %d   X: %0.2f Y: %0.2f Z: %0.2f PCH: %0.2f   YAW: %0.2f ROL: 0.00   DIFF: %d\r\n", objData[i], obj->pos.x, -obj->pos.y, obj->pos.z, 0.0f, objYaw, obj->diff);
					if (logicCount > 0)
					{
						WRITE_LINE("        SEQ\r\n");
						for (s32 l = 0; l < logicCount; l++)
						{
							WRITE_LINE("            LOGIC:     %s\r\n", entity->logic[l].c_str());
						}
						// TODO: Variables.
						WRITE_LINE("        SEQEND\r\n");
					}
				} break;
				case ETYPE_SPRITE:
				{
					WRITE_LINE("    CLASS: SPRITE     DATA: %d   X: %0.2f Y: %0.2f Z: %0.2f PCH: %0.2f   YAW: %0.2f ROL: 0.00   DIFF: %d\r\n", objData[i], obj->pos.x, -obj->pos.y, obj->pos.z, 0.0f, objYaw, obj->diff);
					if (logicCount > 0)
					{
						WRITE_LINE("        SEQ\r\n");
						for (s32 l = 0; l < logicCount; l++)
						{
							WRITE_LINE("            LOGIC:     %s\r\n", entity->logic[l].c_str());
						}
						// TODO: Variables.
						WRITE_LINE("        SEQEND\r\n");
					}
				} break;
			}
		}
		NEW_LINE();

		file.close();
		return true;
	}

	bool exportDfInf(const char* infFile)
	{
		FileStream file;
		if (!file.open(infFile, FileStream::MODE_WRITE))
		{
			return false;
		}

		char newLine[256];
		strcpy(newLine, "\r\n");

		char buffer[256];
		WRITE_LINE("%s", "INF 1.0\r\n");
		NEW_LINE();

		WRITE_LINE("LEVELNAME %s\r\n", s_level.name.c_str());
		WRITE_LINE("items %d\r\n", 0);
		NEW_LINE();

		file.close();
		return true;
	}

	// TODO: Factor this code out.
	// Gob Stuff
#pragma pack(push)
#pragma pack(1)
	struct GobHeader
	{
		char GOB_MAGIC[4];
		u32 MASTERX;	// offset to GOB_Index_t
	};

	struct GobEntry
	{
		u32 IX;		// offset to the start of the file.
		u32 LEN;		// length of the file.
		char NAME[13];	// file name.
	};

	struct GobIndex
	{
		u32 MASTERN;	// num files
		GobEntry* entries;
	};
#pragma pack(pop)
	static std::vector<GobEntry> s_directory;
	static std::vector<u8> s_workBuffer;

	u32 getFileLength(const char* path)
	{
		FileStream file;
		u32 len = 0;
		if (file.open(path, FileStream::MODE_READ))
		{
			len = (u32)file.getSize();
			file.close();
		}
		return len;
	}

	bool writeGob(const char* path, FileList& fileList)
	{
		if (!path || fileList.empty()) { return false; }

		FILE* gob = fopen(path, "wb");
		if (!gob) { return false; }

		GobHeader header = { 0 };
		header.GOB_MAGIC[0] = 'G';
		header.GOB_MAGIC[1] = 'O';
		header.GOB_MAGIC[2] = 'B';
		header.GOB_MAGIC[3] = '\n';
		header.MASTERX = sizeof(GobHeader);

		// Fill out the directory.
		u32 fileCount = (u32)fileList.size();
		s_directory.resize(fileCount);
		GobEntry* entry = s_directory.data();
		for (u32 i = 0; i < fileCount; i++, entry++)
		{
			// Otherwise read from the passed in fileList.
			entry->LEN = getFileLength(fileList[i].c_str());

			char name[256];
			FileUtil::getFileNameFromPath(fileList[i].c_str(), name, true);
			strcpy(entry->NAME, name);

			entry->IX = header.MASTERX;
			header.MASTERX += entry->LEN;
		}

		// Write the header and file count.
		fwrite(&header, sizeof(GobHeader), 1, gob);

		// Write the files.
		entry = s_directory.data();
		for (u32 i = 0; i < fileCount; i++, entry++)
		{
			fseek(gob, entry->IX, SEEK_SET);

			// Write the files in fileList.
			FileStream file;
			file.open(fileList[i].c_str(), FileStream::MODE_READ);
			u32 len = (u32)file.getSize();
			s_workBuffer.resize(len);
			file.readBuffer(s_workBuffer.data(), len);
			file.close();

			assert(len == entry->LEN);
			fwrite(s_workBuffer.data(), len, 1, gob);
		}

		// Write the directory.
		fseek(gob, header.MASTERX, SEEK_SET);
		fwrite(&fileCount, sizeof(u32), 1, gob);
		fwrite(s_directory.data(), sizeof(GobEntry), fileCount, gob);

		fclose(gob);
		return true;
	}

	// Hack test path for now.
	static const char* s_testPath = "D:/dev/TheForceEngine/TheForceEngine/Games/Dark Forces";
	static const char* s_testAppPath = "D:/dev/TheForceEngine/Build/TheForceEngine.exe";

	bool exportLevel(const char* path, const char* name, const StartPoint* start)
	{
		char levFile[TFE_MAX_PATH];
		char infFile[TFE_MAX_PATH];
		char objFile[TFE_MAX_PATH];
		sprintf(levFile, "%s/%s.LEV", path, name);
		sprintf(infFile, "%s/%s.INF", path, name);
		sprintf(objFile, "%s/%s.O", path, name);

		if (!exportDfLevel(levFile)) { return false; }
		if (!exportDfInf(infFile)) { return false; }
		if (!exportDfObj(objFile, start)) { return false; }

		// Build a file list for the gob writer.
		FileList fileList;
		fileList.push_back(levFile);
		fileList.push_back(infFile);
		fileList.push_back(objFile);

		char gobPath[TFE_MAX_PATH];
		sprintf(gobPath, "%s/TFE_TEST.GOB", s_testPath);
		writeGob(gobPath, fileList);

		// Now run "TFE"
		// Get the app directory and GOB name.
		char appDir[1024], gobName[256];
		FileUtil::getFilePath(s_testAppPath, appDir);
		FileUtil::getFileNameFromPath(gobPath, gobName, true);

		// Build the Commandline.
		char cmdLine[1024];
		sprintf(cmdLine, "-gDark -skip_load_delay -u%s -c0 -l%s", gobName, s_level.slot.c_str());

		// Run the test app (TFE, etc.), this will block until finished.
		osShellExecute(s_testAppPath, appDir, cmdLine, true);
		// Then cleanup by deleting the test GOB.
		FileUtil::deleteFile(gobPath);
		
		return true;
	}

	EditorTexture* getTexture(s32 index)
	{
		if (index < 0) { return nullptr; }
		return (EditorTexture*)getAssetData(s_level.textures[index].handle);
	}

	Asset* getTextureAssetByName(const char* name)
	{
		const s32 count = (s32)s_levelTextureList.size();
		Asset* asset = s_levelTextureList.data();
		for (s32 i = 0; i < count; i++)
		{
			if (strcasecmp(asset[i].name.c_str(), name) == 0)
			{
				return &asset[i];
			}
		}
		return nullptr;
	}

	s32 getTextureIndex(const char* name)
	{
		const s32 count = (s32)s_level.textures.size();
		const LevelTextureAsset* texAsset = s_level.textures.data();
		for (s32 i = 0; i < count; i++, texAsset++)
		{
			if (strcasecmp(texAsset->name.c_str(), name) == 0)
			{
				return i;
			}
		}
		Asset* asset = getTextureAssetByName(name);
		if (asset)
		{
			s32 newId = count;
			LevelTextureAsset newTex;
			newTex.name = name;
			newTex.handle = asset->handle;
			s_level.textures.push_back(newTex);
			return newId;
		}
		return -1;
	}

	// Update the sector's polygon from the sector data.
	void sectorToPolygon(EditorSector* sector)
	{
		Polygon& poly = sector->poly;
		poly.edge.resize(sector->walls.size());
		poly.vtx.resize(sector->vtx.size());

		poly.bounds[0] = {  FLT_MAX,  FLT_MAX };
		poly.bounds[1] = { -FLT_MAX, -FLT_MAX };

		const size_t vtxCount = sector->vtx.size();
		const Vec2f* vtx = sector->vtx.data();
		for (size_t v = 0; v < vtxCount; v++, vtx++)
		{
			poly.vtx[v] = *vtx;
			poly.bounds[0].x = std::min(poly.bounds[0].x, vtx->x);
			poly.bounds[0].z = std::min(poly.bounds[0].z, vtx->z);
			poly.bounds[1].x = std::max(poly.bounds[1].x, vtx->x);
			poly.bounds[1].z = std::max(poly.bounds[1].z, vtx->z);
		}

		const size_t wallCount = sector->walls.size();
		const EditorWall* wall = sector->walls.data();
		for (size_t w = 0; w < wallCount; w++, wall++)
		{
			poly.edge[w] = { wall->idx[0], wall->idx[1] };
		}

		// Clear out cached triangle data.
		poly.triVtx.clear();
		poly.triIdx.clear();

		TFE_Polygon::computeTriangulation(&sector->poly);

		// Update the sector bounds.
		sector->bounds[0] = { poly.bounds[0].x, 0.0f, poly.bounds[0].z };
		sector->bounds[1] = { poly.bounds[1].x, 0.0f, poly.bounds[1].z };
		sector->bounds[0].y = min(sector->floorHeight, sector->ceilHeight);
		sector->bounds[1].y = max(sector->floorHeight, sector->ceilHeight);
	}

	// Update the sector itself from the sector's polygon.
	void polygonToSector(EditorSector* sector)
	{
		// TODO
	}

	s32 findSector2d(s32 layer, const Vec2f* pos)
	{
		if (s_level.sectors.empty()) { return -1; }

		const s32 sectorCount = (s32)s_level.sectors.size();
		EditorSector* sectors = s_level.sectors.data();

		for (s32 i = 0; i < sectorCount; i++)
		{
			if (sectors[i].layer != layer) { continue; }
			if (TFE_Polygon::pointInsidePolygon(&sectors[i].poly, *pos))
			{
				return i;
			}
		}
		return -1;
	}

	bool rayHitAABB(const Ray* ray, const Vec3f* bounds)
	{
		enum
		{
			LEFT = 0,
			RIGHT,
			MID,
		};

		// Pick representative planes.
		s32 quadrant[3];
		f32 candidatePlane[3];
		bool inside = true;
		for (s32 i = 0; i < 3; i++)
		{
			quadrant[i] = MID;
			if (ray->origin.m[i] < bounds[0].m[i])
			{
				quadrant[i] = LEFT;
				candidatePlane[i] = bounds[0].m[i];
				inside = false;
			}
			else if (ray->origin.m[i] > bounds[0].m[i])
			{
				quadrant[i] = RIGHT;
				candidatePlane[i] = bounds[1].m[i];
				inside = false;
			}
		}
		// The ray starts inside the bounds, so we're done.
		if (inside) { return true; }

		// Calcuate the distance to the candidate planes.
		f32 maxT[3];
		for (s32 i = 0; i < 3; i++)
		{
			maxT[i] = -1.0f;
			if (quadrant[i] != MID && ray->dir.m[i] != 0.0f)
			{
				maxT[i] = (candidatePlane[i] - ray->origin.m[i]) / ray->dir.m[i];
			}
		}

		// Get the largest dist
		s32 planeId = 0;
		for (s32 i = 1; i < 3; i++)
		{
			if (maxT[planeId] < maxT[i]) { planeId = i; }
		}

		// Make sure it is really inside.
		if (maxT[planeId] < 0.0f) { return false; }
		Vec3f coord = { 0 };
		for (s32 i = 0; i < 3; i++)
		{
			if (planeId != i)
			{
				coord.m[i] = ray->origin.m[i] + maxT[planeId] * ray->dir.m[i];
				if (coord.m[i] < bounds[0].m[i] || coord.m[i] > bounds[1].m[i])
				{
					return false;
				}
			}
			else
			{
				coord.m[i] = candidatePlane[i];
			}
		}
		return true;
	}

	f32 getWallLength(const EditorSector* sector, const EditorWall* wall)
	{
		const Vec2f* v0 = &sector->vtx[wall->idx[0]];
		const Vec2f* v1 = &sector->vtx[wall->idx[1]];
		const Vec2f delta = { v1->x - v0->x, v1->z - v0->z };
		return sqrtf(delta.x*delta.x + delta.z*delta.z);
	}

	bool getSignExtents(const EditorSector* sector, const EditorWall* wall, Vec2f ext[2])
	{
		if (wall->tex[WP_SIGN].texIndex < 0) { return false; }

		f32 uOffset = wall->tex[WP_MID].offset.x;
		f32 vOffset = sector->floorHeight;
		if (wall->adjoinId >= 0)
		{
			const EditorSector* next = &s_level.sectors[wall->adjoinId];
			if (next->floorHeight > sector->floorHeight)
			{
				uOffset = wall->tex[WP_BOT].offset.x;
			}
			else if (next->ceilHeight < sector->ceilHeight)
			{
				uOffset = wall->tex[WP_TOP].offset.x;
				vOffset = next->ceilHeight;
			}
		}

		bool hasSign = false;
		const EditorTexture* tex = getTexture(wall->tex[WP_SIGN].texIndex);
		if (tex)
		{
			ext[0].x = wall->tex[WP_SIGN].offset.x - uOffset;
			ext[1].x = ext[0].x + f32(tex->width) / 8.0f;
			ext[0].z = vOffset - wall->tex[WP_SIGN].offset.z;
			ext[1].z = ext[0].z + f32(tex->height) / 8.0f;
			hasSign = true;
		}
		return hasSign;
	}

	void centerSignOnSurface(const EditorSector* sector, EditorWall* wall)
	{
		if (wall->tex[WP_SIGN].texIndex < 0) { return; }
		const EditorTexture* signTex = getTexture(wall->tex[WP_SIGN].texIndex);
		if (!signTex) { return; }

		f32 uOffset = wall->tex[WP_MID].offset.x;
		f32 baseY = sector->floorHeight;
		f32 partHeight = std::max(0.0f, sector->ceilHeight - sector->floorHeight);
		if (wall->adjoinId >= 0)
		{
			const EditorSector* next = &s_level.sectors[wall->adjoinId];
			if (next->floorHeight > sector->floorHeight)
			{
				uOffset = wall->tex[WP_BOT].offset.x;
				partHeight = next->floorHeight - sector->floorHeight;
			}
			else if (next->ceilHeight < sector->ceilHeight)
			{
				uOffset = wall->tex[WP_TOP].offset.x;
				baseY = next->ceilHeight;
				partHeight = sector->ceilHeight - next->ceilHeight;
			}
		}

		f32 wallLen = getWallLength(sector, wall);
		wall->tex[WP_SIGN].offset.x = uOffset + std::max(0.0f, (wallLen - signTex->width/8.0f)*0.5f);
		wall->tex[WP_SIGN].offset.z = -std::max(0.0f, partHeight - signTex->height/8.0f) * 0.5f;
	}

	bool rayAABBIntersection(const Ray* ray, const Vec3f* bounds, f32* hitDist)
	{
		const f32 ix = fabsf(ray->dir.x) > FLT_EPSILON ? 1.0f/ray->dir.x : FLT_MAX;
		const f32 iy = fabsf(ray->dir.y) > FLT_EPSILON ? 1.0f/ray->dir.y : FLT_MAX;
		const f32 iz = fabsf(ray->dir.z) > FLT_EPSILON ? 1.0f/ray->dir.z : FLT_MAX;

		const f32 tx1 = (bounds[0].x - ray->origin.x) * ix;
		const f32 tx2 = (bounds[1].x - ray->origin.x) * ix;
		const f32 ty1 = (bounds[0].y - ray->origin.y) * iy;
		const f32 ty2 = (bounds[1].y - ray->origin.y) * iy;
		const f32 tz1 = (bounds[0].z - ray->origin.z) * iz;
		const f32 tz2 = (bounds[1].z - ray->origin.z) * iz;

		f32 tmin = -FLT_MAX;
		tmin = max(tmin, min(tx1, tx2));
		tmin = max(tmin, min(ty1, ty2));
		tmin = max(tmin, min(tz1, tz2));

		f32 tmax = FLT_MAX;
		tmax = min(tmax, max(tx1, tx2));
		tmax = min(tmax, max(ty1, ty2));
		tmax = min(tmax, max(tz1, tz2));

		*hitDist = tmin;
		return tmax >= tmin && tmax >= 0.0f;
	}
		
	// Return true if a hit is found.
	bool traceRay(const Ray* ray, RayHitInfo* hitInfo, bool flipFaces, bool canHitSigns, bool canHitObjects)
	{
		const EditorLevel* level = &s_level;
		if (level->sectors.empty()) { return false; }
		const s32 sectorCount = (s32)level->sectors.size();
		const EditorSector* sector = level->sectors.data();

		f32 maxDist  = ray->maxDist;
		Vec3f origin = ray->origin;
		Vec2f p0xz = { origin.x, origin.z };
		Vec2f p1xz = { origin.x + ray->dir.x * maxDist, origin.z + ray->dir.z * maxDist };
		Vec2f dirxz = { ray->dir.x, ray->dir.z };

		f32 overallClosestHit = FLT_MAX;
		hitInfo->hitSectorId = -1;
		hitInfo->hitWallId = -1;
		hitInfo->hitObjId = -1;
		hitInfo->hitPart = HP_MID;
		hitInfo->hitPos = { 0 };
		hitInfo->dist = FLT_MAX;

		// Loop through sectors in the world.
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if (ray->layer != LAYER_ANY && ray->layer != sector->layer) { continue; }

			// Check the bounds.
			//if (!rayHitAABB(ray, sector->bounds)) { continue; }

			// Now check against the walls.
			const u32 wallCount = (u32)sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vtx.data();
			f32 closestHit = FLT_MAX;
			s32 closestWallId = -1;
			bool rayInSector = false;
			for (u32 w = 0; w < wallCount; w++, wall++)
			{
				const Vec2f* v0 = &vtx[wall->idx[0]];
				const Vec2f* v1 = &vtx[wall->idx[1]];
				Vec2f nrm = { -(v1->z - v0->z), v1->x - v0->x };
				if (flipFaces && TFE_Math::dot(&dirxz, &nrm) > 0.0f) { continue; }
				else if (!flipFaces && TFE_Math::dot(&dirxz, &nrm) < 0.0f) { continue; }

				f32 s, t;
				if (TFE_Math::lineSegmentIntersect(&p0xz, &p1xz, v0, v1, &s, &t))
				{
					if (s < closestHit)
					{
						const f32 yAtHit = origin.y + ray->dir.y * s * maxDist;
						if (yAtHit > sector->floorHeight - FLT_EPSILON && yAtHit < sector->ceilHeight + FLT_EPSILON)
						{
							bool canHit = true;
							if (wall->adjoinId >= 0)
							{
								const EditorSector* next = &level->sectors[wall->adjoinId];
								canHit = (yAtHit <= next->floorHeight) || (yAtHit >= next->ceilHeight) || (wall->flags[0] & WF1_ADJ_MID_TEX);
							}
							if (canHit)
							{
								closestHit = s;
								closestWallId = w;
							}
						}
						rayInSector = true;
					}
				}
			}

			// Test the closest wall
			wall = sector->walls.data();
			if (closestWallId >= 0)
			{
				closestHit *= maxDist;
				const Vec3f hitPoint = { origin.x + ray->dir.x*closestHit, origin.y + ray->dir.y*closestHit, origin.z + ray->dir.z*closestHit };

				Vec2f signExt[2];
				f32 hitU = 0.0f;
				const bool hasSign = canHitSigns && getSignExtents(sector, &wall[closestWallId], signExt);
				bool hitSign = false;
				if (hasSign)
				{
					const Vec2f* v0 = &sector->vtx[wall[closestWallId].idx[0]];
					const Vec2f* v1 = &sector->vtx[wall[closestWallId].idx[1]];
					Vec2f wallDir = { v1->x - v0->x, v1->z - v0->z };
					wallDir = TFE_Math::normalize(&wallDir);

					if (fabsf(wallDir.x) >= fabsf(wallDir.z))
					{
						hitU = (hitPoint.x - v0->x) / wallDir.x;
					}
					else
					{
						hitU = (hitPoint.z - v0->z) / wallDir.z;
					}

					hitSign = hitU >= signExt[0].x && hitU < signExt[1].x && 
						hitPoint.y >= signExt[0].z && hitPoint.y < signExt[1].z;
				}

				if (hitSign && closestHit < overallClosestHit)
				{
					overallClosestHit = closestHit;
					hitInfo->hitSectorId = sector->id;
					hitInfo->hitWallId = closestWallId;
					hitInfo->hitObjId = -1;
					hitInfo->hitPart = HP_SIGN;
					hitInfo->hitPos = hitPoint;
					hitInfo->dist = closestHit;
				}
				else if (wall[closestWallId].adjoinId >= 0)
				{
					// given the hit point, is it below the next floor or above the next ceiling?
					const EditorSector* next = &level->sectors[wall[closestWallId].adjoinId];
					if ((hitPoint.y <= next->floorHeight || hitPoint.y >= next->ceilHeight) && closestHit < overallClosestHit)
					{
						overallClosestHit = closestHit;
						hitInfo->hitSectorId = sector->id;
						hitInfo->hitWallId = closestWallId;
						hitInfo->hitObjId = -1;
						hitInfo->hitPart = hitPoint.y <= next->floorHeight ? HP_BOT : HP_TOP;
						hitInfo->hitPos = hitPoint;
						hitInfo->dist = closestHit;
					}
					else if ((wall[closestWallId].flags[0] & WF1_ADJ_MID_TEX) && closestHit < overallClosestHit)
					{
						overallClosestHit = closestHit;
						hitInfo->hitSectorId = sector->id;
						hitInfo->hitWallId = closestWallId;
						hitInfo->hitObjId = -1;
						hitInfo->hitPart = HP_MID;
						hitInfo->hitPos = hitPoint;
						hitInfo->dist = closestHit;
					}
					// TODO: Handle Sign.
				}
				else if (closestHit < overallClosestHit)
				{
					overallClosestHit = closestHit;
					hitInfo->hitSectorId = sector->id;
					hitInfo->hitWallId = closestWallId;
					hitInfo->hitObjId = -1;
					hitInfo->hitPart = HP_MID;
					hitInfo->hitPos = hitPoint;
					hitInfo->dist = closestHit;
				}
			}

			// Test the floor and ceiling planes.
			const Vec3f planeTest = { origin.x + ray->dir.x*maxDist, origin.y + ray->dir.y*maxDist, origin.z + ray->dir.z*maxDist };
			Vec3f hitPoint;

			const bool canHitFloor = (!flipFaces && origin.y > sector->floorHeight && ray->dir.y < 0.0f) ||
	               (flipFaces && origin.y < sector->floorHeight && ray->dir.y > 0.0f);
			const bool canHitCeil = (!flipFaces && origin.y < sector->ceilHeight && ray->dir.y > 0.0f) ||
			      (flipFaces && origin.y > sector->ceilHeight && ray->dir.y < 0.0f);

			if (canHitFloor && TFE_Math::lineYPlaneIntersect(&origin, &planeTest, sector->floorHeight, &hitPoint))
			{
				const Vec3f offset = { hitPoint.x - origin.x, hitPoint.y - origin.y, hitPoint.z - origin.z };
				const f32 distSq = TFE_Math::dot(&offset, &offset);
				if (overallClosestHit == FLT_MAX || distSq < overallClosestHit*overallClosestHit)
				{
					// The ray hit the plane, but is it inside of the sector polygon?
					Vec2f testPt = { hitPoint.x, hitPoint.z };
					if (TFE_Polygon::pointInsidePolygon(&sector->poly, testPt))
					{
						overallClosestHit = sqrtf(distSq);
						hitInfo->hitSectorId = sector->id;
						hitInfo->hitWallId = -1;
						hitInfo->hitObjId = -1;
						hitInfo->hitPart = HP_FLOOR;
						hitInfo->hitPos = hitPoint;
						hitInfo->dist = overallClosestHit;
						rayInSector = true;
					}
				}
			}
			if (canHitCeil && TFE_Math::lineYPlaneIntersect(&origin, &planeTest, sector->ceilHeight, &hitPoint))
			{
				const Vec3f offset = { hitPoint.x - origin.x, hitPoint.y - origin.y, hitPoint.z - origin.z };
				const f32 distSq = TFE_Math::dot(&offset, &offset);
				if (overallClosestHit == FLT_MAX || distSq < overallClosestHit*overallClosestHit)
				{
					// The ray hit the plane, but is it inside of the sector polygon?
					Vec2f testPt = { hitPoint.x, hitPoint.z };
					if (TFE_Polygon::pointInsidePolygon(&sector->poly, testPt))
					{
						overallClosestHit = sqrtf(distSq);
						hitInfo->hitSectorId = sector->id;
						hitInfo->hitWallId = -1;
						hitInfo->hitObjId = -1;
						hitInfo->hitPart = HP_CEIL;
						hitInfo->hitPos = hitPoint;
						hitInfo->dist = overallClosestHit;
						rayInSector = true;
					}
				}
			}

			// Objects
			if (rayInSector && canHitObjects)
			{
				const s32 objCount = (s32)sector->obj.size();
				const EditorObject* obj = sector->obj.data();
				for (s32 o = 0; o < objCount; o++, obj++)
				{
					const Entity* entity = &s_entityList[obj->entityId];
					Vec3f pos = obj->pos;

					// If the entity is on the floor, make sure it doesn't stick through it for editor selection.
					if (entity->type != ETYPE_SPIRIT && entity->type != ETYPE_SAFE)
					{
						f32 offset = -(entity->offset.y + fabsf(entity->st[1].z - entity->st[0].z)) * 0.1f;
						if (fabsf(pos.y - sector->floorHeight) < 0.5f) { offset = max(0.0f, offset); }
						pos.y += offset;
					}
					const f32 width = entity->size.x * 0.5f;
					const f32 height = entity->size.z;
					const Vec3f bounds[2] = { {pos.x - width, pos.y, pos.z - width}, {pos.x + width, pos.y + height, pos.z + width} };
					f32 dist;
					if (rayAABBIntersection(ray, bounds, &dist))
					{
						if (dist < overallClosestHit)
						{
							overallClosestHit = dist;
							hitInfo->hitSectorId = sector->id;
							hitInfo->hitWallId = -1;
							hitInfo->dist = overallClosestHit;
							hitInfo->hitObjId = o;
							hitInfo->hitPos = { ray->origin.x + ray->dir.x*dist, ray->origin.y + ray->dir.y*dist, ray->origin.z + ray->dir.z*dist };
						}
					}
				}
			}
		}

		return hitInfo->hitSectorId >= 0;
	}
	
	bool pointInsideAABB3d(const Vec3f* aabb, const Vec3f* pt)
	{
		return (pt->x >= aabb[0].x && pt->x <= aabb[1].x &&
			pt->y >= aabb[0].y && pt->y <= aabb[1].y &&
			pt->z >= aabb[0].z && pt->z <= aabb[1].z);
	}

	bool pointInsideAABB2d(const Vec3f* aabb, const Vec3f* pt)
	{
		return (pt->x >= aabb[0].x && pt->x <= aabb[1].x &&
			pt->z >= aabb[0].z && pt->z <= aabb[1].z);
	}

	bool aabbOverlap3d(const Vec3f* aabb0, const Vec3f* aabb1)
	{
		// X
		if (aabb0[0].x > aabb1[1].x || aabb0[1].x < aabb1[0].x ||
			aabb1[0].x > aabb0[1].x || aabb1[1].x < aabb0[0].x)
		{
			return false;
		}

		// Y
		if (aabb0[0].y > aabb1[1].y || aabb0[1].y < aabb1[0].y ||
			aabb1[0].y > aabb0[1].y || aabb1[1].y < aabb0[0].y)
		{
			return false;
		}

		// Z
		if (aabb0[0].z > aabb1[1].z || aabb0[1].z < aabb1[0].z ||
			aabb1[0].z > aabb0[1].z || aabb1[1].z < aabb0[0].z)
		{
			return false;
		}

		return true;
	}

	bool aabbOverlap2d(const Vec3f* aabb0, const Vec3f* aabb1)
	{
		// Ignore the Y axis.
		// X
		if (aabb0[0].x > aabb1[1].x || aabb0[1].x < aabb1[0].x ||
			aabb1[0].x > aabb0[1].x || aabb1[1].x < aabb0[0].x)
		{
			return false;
		}

		// Z
		if (aabb0[0].z > aabb1[1].z || aabb0[1].z < aabb1[0].z ||
			aabb1[0].z > aabb0[1].z || aabb1[1].z < aabb0[0].z)
		{
			return false;
		}

		return true;
	}

	// TODO: Spatial data structure to handle cases where there are 10k, 100k, etc. sectors.
	bool getOverlappingSectorsPt(const Vec3f* pos, SectorList* result)
	{
		if (!pos || !result) { return false; }

		result->clear();
		const s32 count = (s32)s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		for (s32 i = 0; i < count; i++, sector++)
		{
			if (pos->x < sector->bounds[0].x || pos->x > sector->bounds[1].x ||
				pos->y < sector->bounds[0].y || pos->y > sector->bounds[1].y ||
				pos->z < sector->bounds[0].z || pos->z > sector->bounds[1].z)
			{
				continue;
			}
			result->push_back(sector);
		}

		return !result->empty();
	}
	   
	bool getOverlappingSectorsBounds(const Vec3f bounds[2], SectorList* result)
	{
		if (!bounds || !result) { return false; }

		result->clear();
		const s32 count = (s32)s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		for (s32 i = 0; i < count; i++, sector++)
		{
			if (aabbOverlap3d(sector->bounds, bounds))
			{
				result->push_back(sector);
			}
		}

		return !result->empty();
	}
	
	// Note: memcpys() are used to avoid pointer alignment issues.
	void writeU8(u8 value)
	{
		s_buffer->push_back(value);
	}

	void writeU32(u32 value)
	{
		const size_t pos = s_buffer->size();
		s_buffer->resize(pos + sizeof(u32));
		memcpy(s_buffer->data() + pos, &value, sizeof(u32));
	}

	void writeS32(s32 value)
	{
		const size_t pos = s_buffer->size();
		s_buffer->resize(pos + sizeof(s32));
		memcpy(s_buffer->data() + pos, &value, sizeof(s32));
	}

	void writeF32(f32 value)
	{
		const size_t pos = s_buffer->size();
		s_buffer->resize(pos + sizeof(f32));
		memcpy(s_buffer->data() + pos, &value, sizeof(f32));
	}

	void writeData(const void* srcData, u32 size)
	{
		const size_t pos = s_buffer->size();
		s_buffer->resize(pos + size);
		memcpy(s_buffer->data() + pos, srcData, size);
	}

	void writeString(const std::string& str)
	{
		writeU32((u32)str.length());
		writeData(str.data(), (u32)str.length());
	}

	u8 readU8()
	{
		u8 value = *s_readBuffer;
		s_readBuffer++;
		return value;
	}

	u32 readU32()
	{
		u32 value;
		memcpy(&value, s_readBuffer, sizeof(u32));
		s_readBuffer += sizeof(u32);
		return value;
	}

	s32 readS32()
	{
		s32 value;
		memcpy(&value, s_readBuffer, sizeof(s32));
		s_readBuffer += sizeof(s32);
		return value;
	}

	f32 readF32()
	{
		f32 value;
		memcpy(&value, s_readBuffer, sizeof(f32));
		s_readBuffer += sizeof(f32);
		return value;
	}

	void readData(void* dstData, u32 size)
	{
		memcpy(dstData, s_readBuffer, size);
		s_readBuffer += size;
	}

	void readString(std::string& str)
	{
		u32 len = readU32();
		char strBuffer[1024];
		readData(strBuffer, len);
		strBuffer[len] = 0;

		str = strBuffer;
	}
		
	void level_createSnapshot(SnapshotBuffer* buffer)
	{
		assert(buffer);
		// Serialize the level into a buffer.
		s_buffer = buffer;
		
		writeString(s_level.name);
		writeString(s_level.slot);
		writeString(s_level.palette);
		writeU8((u8)s_level.featureSet);
		writeData(&s_level.parallax, sizeof(Vec2f));

		const u32 texCount = (u32)s_level.textures.size();
		writeU32(texCount);
		for (u32 i = 0; i < texCount; i++)
		{
			writeString(s_level.textures[i].name);
		}

		const u32 sectorCount = (u32)s_level.sectors.size();
		const EditorSector* sector = s_level.sectors.data();
		writeU32(sectorCount);
		for (u32 s = 0; s < sectorCount; s++, sector++)
		{
			writeU32(sector->id);
			writeString(sector->name);
			writeData(&sector->floorTex, sizeof(LevelTexture));
			writeData(&sector->ceilTex, sizeof(LevelTexture));
			writeF32(sector->floorHeight);
			writeF32(sector->ceilHeight);
			writeF32(sector->secHeight);
			writeU32(sector->ambient);
			writeS32(sector->layer);
			writeData(sector->flags, sizeof(u32) * 3);
			writeU32((u32)sector->vtx.size());
			writeU32((u32)sector->walls.size());
			writeU32((u32)sector->obj.size());
			writeData(sector->vtx.data(), u32(sizeof(Vec2f) * sector->vtx.size()));
			writeData(sector->walls.data(), u32(sizeof(EditorWall) * sector->walls.size()));
			writeData(sector->obj.data(), u32(sizeof(EditorObject) * sector->obj.size()));
		}
	}
		
	void level_unpackSnapshot(s32 id, u32 size, void* data)
	{
		// Only unpack the snapshot if its not already cached.
		if (s_curSnapshotId != id)
		{
			s_curSnapshotId = id;

			s_readBuffer = (u8*)data;
			readString(s_curSnapshot.name);
			readString(s_curSnapshot.slot);
			readString(s_curSnapshot.palette);
			s_curSnapshot.featureSet = (FeatureSet)readU8();
			readData(&s_curSnapshot.parallax, sizeof(Vec2f));

			u32 texCount = readU32();
			s_curSnapshot.textures.resize(texCount);
			for (u32 i = 0; i < texCount; i++)
			{
				readString(s_curSnapshot.textures[i].name);
				s_curSnapshot.textures[i].handle = loadTexture(s_curSnapshot.textures[i].name.c_str());
			}

			u32 sectorCount = readU32();
			s_curSnapshot.sectors.resize(sectorCount);
			EditorSector* sector = s_curSnapshot.sectors.data();
			for (u32 s = 0; s < sectorCount; s++, sector++)
			{
				sector->id = readU32();
				readString(sector->name);
				readData(&sector->floorTex, sizeof(LevelTexture));
				readData(&sector->ceilTex, sizeof(LevelTexture));
				sector->floorHeight = readF32();
				sector->ceilHeight = readF32();
				sector->secHeight = readF32();
				sector->ambient = readU32();
				sector->layer = readS32();
				readData(sector->flags, sizeof(u32) * 3);

				u32 vtxCount = readU32();
				u32 wallCount = readU32();
				u32 objCount = readU32();
				sector->vtx.resize(vtxCount);
				sector->walls.resize(wallCount);
				sector->obj.resize(objCount);

				readData(sector->vtx.data(), u32(sizeof(Vec2f) * vtxCount));
				readData(sector->walls.data(), u32(sizeof(EditorWall) * wallCount));
				readData(sector->obj.data(), u32(sizeof(EditorObject) * objCount));

				// Compute derived data.
				sectorToPolygon(sector);
				sector->bounds[0] = { sector->poly.bounds[0].x, std::min(sector->floorHeight, sector->ceilHeight), sector->poly.bounds[0].z };
				sector->bounds[1] = { sector->poly.bounds[1].x, std::max(sector->floorHeight, sector->ceilHeight), sector->poly.bounds[1].z };
				sector->searchKey = 0;
			}

			// Compute final snapshot bounds.
			s_curSnapshot.bounds[0] = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
			s_curSnapshot.bounds[1] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
			s_curSnapshot.layerRange[0] =  INT_MAX;
			s_curSnapshot.layerRange[1] = -INT_MAX;
			sector = s_curSnapshot.sectors.data();
			for (size_t i = 0; i < sectorCount; i++, sector++)
			{
				s_curSnapshot.bounds[0].x = min(s_curSnapshot.bounds[0].x, sector->bounds[0].x);
				s_curSnapshot.bounds[0].y = min(s_curSnapshot.bounds[0].y, sector->bounds[0].y);
				s_curSnapshot.bounds[0].z = min(s_curSnapshot.bounds[0].z, sector->bounds[0].z);

				s_curSnapshot.bounds[1].x = max(s_curSnapshot.bounds[1].x, sector->bounds[1].x);
				s_curSnapshot.bounds[1].y = max(s_curSnapshot.bounds[1].y, sector->bounds[1].y);
				s_curSnapshot.bounds[1].z = max(s_curSnapshot.bounds[1].z, sector->bounds[1].z);

				s_curSnapshot.layerRange[0] = min(s_curSnapshot.layerRange[0], sector->layer);
				s_curSnapshot.layerRange[1] = max(s_curSnapshot.layerRange[1], sector->layer);
			}
		}
		// Then copy the snapshot to the level data itself. Its the new state.
		s_level = s_curSnapshot;

		// For now until the way snapshot memory is handled is refactored, to avoid duplicate code that will be removed later.
		// TODO: Handle edit state properly here too.
		edit_clearSelections();
	}
}

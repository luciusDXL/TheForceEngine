#include "levelEditorData.h"
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
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	static std::vector<u8> s_fileData;
	static std::vector<AssetHandle> s_textureList;
	static u32* s_palette = nullptr;
	static s32 s_palIndex = 0;

	enum Constants
	{
		LevVersionMin = 15,
		LevVersionMax = 21,
		LevVersion_Layers_WallLight = 21,
	};

	AssetHandle loadTexture(const char* bmTextureName)
	{
		Asset* texAsset = AssetBrowser::findAsset(bmTextureName, TYPE_TEXTURE);
		if (!texAsset) { return NULL_ASSET; }
		return AssetBrowser::loadAssetData(texAsset);
	}

	bool loadLevelFromAsset(Asset* asset, EditorLevel* level)
	{
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
		s_textureList.resize(textureCount);

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
			s_textureList[i] = loadTexture(bmTextureName);
		}
		// Sometimes there are extra textures, just add them - they will be compacted later.
		bool readNext = true;
		while (sscanf(line, " TEXTURE: %s ", textureName) == 1)
		{
			char bmTextureName[32];
			FileUtil::replaceExtension(textureName, "BM", bmTextureName);
			s_textureList.push_back(loadTexture(bmTextureName));

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
			sector->floorTex.handle = s_textureList[floorTexId];
			
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
			sector->ceilTex.handle = s_textureList[ceilTexId];

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

				wall->tex[WP_MID].handle = texId[WP_MID] >= 0 ? s_textureList[texId[WP_MID]] : NULL_ASSET;
				wall->tex[WP_TOP].handle = texId[WP_TOP] >= 0 ? s_textureList[texId[WP_TOP]] : NULL_ASSET;
				wall->tex[WP_BOT].handle = texId[WP_BOT] >= 0 ? s_textureList[texId[WP_BOT]] : NULL_ASSET;
				wall->tex[WP_SIGN].handle = texId[WP_SIGN] >= 0 ? s_textureList[texId[WP_SIGN]] : NULL_ASSET;

				if (wall->tex[WP_SIGN].handle == NULL_ASSET)
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
	}

	// Update the sector itself from the sector's polygon.
	void polygonToSector(EditorSector* sector)
	{
		// TODO
	}
}

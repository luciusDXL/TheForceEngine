/////////////////////////////////////////////////////////////////////////
// The Force Engine Level Editor Data
// The runtime has fixed sectors and INF data that doesn't change in size
// whereas the data the Level Editor needs to access and change is
// constantly malleable. So the LevelEditor needs to copy the tight
// runtime structures into more malleable structures and be able to reverse
// the process in order to test.
/////////////////////////////////////////////////////////////////////////
#include "levelEditorData.h"
#include <DXL2_Asset/imageAsset.h>
#include <DXL2_Asset/spriteAsset.h>
#include <DXL2_Asset/modelAsset.h>
#include <DXL2_Archive/archive.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_RenderBackend/renderBackend.h>
#include <DXL2_Game/geometry.h>
#include <DXL2_Game/renderCommon.h>
#include <DXL2_System/math.h>
#include <DXL2_System/memoryPool.h>
// Triangulation
#include <DXL2_Polygon/polygon.h>
#include <assert.h>
#include <algorithm>

namespace LevelEditorData
{
	static InfEditState s_infEditState = { 0 };

	static EditorLevel s_editorLevel;
	static EditorInfData s_editorInf;
	static std::vector<EditorTexture> s_objIcons;
	static std::vector<u32> s_textureBuffer;

	static const Palette256* s_pal = nullptr;

	void convertInfToEditor(const InfData* infData);
	void convertObjectsToEditor(const LevelObjectData* objData);
	void triangulateSector(const EditorSector* sector, SectorTriangles* outTri);
	void determineSectorTypes();
	EditorInfItem* findInfItem(const EditorSector* sector, s32 wall);

	EditorTexture* getEditorTexture(const char* srcName)
	{
		if (!srcName) { return nullptr; }

		TextureMap::iterator iTex = s_editorLevel.textureMap.find(srcName);
		if (iTex != s_editorLevel.textureMap.end())
		{
			return iTex->second;
		}
		return nullptr;
	}

	EditorTexture* getEditorTexture(const Texture* src)
	{
		if (!src) { return nullptr; }
		return getEditorTexture(src->name);
	}

	u32* convertPalImageToTrueColor(u32 w, u32 h, const u8* image)
	{
		s_textureBuffer.resize(w * h);
		u32* trueColor = s_textureBuffer.data();

		const u32 pixelCount = w * h;
		for (u32 y = 0; y < h; y++)
		{
			for (u32 x = 0; x < w; x++)
			{
				const u32 dstIndex = (h - y - 1)*w + x;
				const u8 srcTexel = image[x*h + y];

				trueColor[dstIndex] = s_pal->colors[srcTexel];
				if (srcTexel != 0u)
				{
					trueColor[dstIndex] |= 0xff000000;
				}
				else
				{
					trueColor[dstIndex] &= 0x00ffffff;
				}
			}
		}

		return trueColor;
	}

	EditorTexture* createTexture(const Texture* src)
	{
		if (!src) { return nullptr; }

		EditorTexture* tex = getEditorTexture(src->name);
		if (tex) { return tex; }
		if (s_objIcons.capacity() == 0) { s_objIcons.reserve(4096u); }

		s_objIcons.push_back({});
		tex = &s_objIcons.back();
		tex->scale = { 1.0f, 1.0f };

		const u32* trueColor = convertPalImageToTrueColor(src->frames[0].width, src->frames[0].height, src->frames[0].image);
		tex->texture = DXL2_RenderBackend::createTexture(src->frames[0].width, src->frames[0].height, trueColor);
		tex->width   = src->frames[0].width;
		tex->height  = src->frames[0].height;
		strcpy(tex->name, src->name);

		tex->scale.x = 1.0f;
		tex->scale.z = 1.0f;
		tex->rect[0] = 0.0f;
		tex->rect[1] = 0.0f;
		tex->rect[2] = 0.0f;
		tex->rect[3] = 0.0f;

		s_editorLevel.textureMap[tex->name] = tex;
		return tex;
	}

	EditorTexture* createObjectTexture(ObjectClass oclass, const char* dataFile)
	{
		EditorTexture* tex = getEditorTexture(dataFile);
		if (tex) { return tex; }
		if (s_objIcons.capacity() == 0) { s_objIcons.reserve(4096u); }
						
		char imagePath[DXL2_MAX_PATH];
		switch (oclass)
		{
			case CLASS_SPIRIT:
			{
				tex = getEditorTexture("SpiritObject.png");
				if (tex) { return tex; }

				DXL2_Paths::appendPath(DXL2_PathType::PATH_PROGRAM, "UI_Images/SpiritObject.png", imagePath, DXL2_MAX_PATH);
				Image* image = DXL2_Image::get(imagePath);
				if (image)
				{
					s_objIcons.push_back({});
					tex = &s_objIcons.back();
					tex->scale = { 1.0f, 1.0f };

					tex->texture = DXL2_RenderBackend::createTexture(image->width, image->height, image->data);
					tex->width = image->width;
					tex->height = image->height;
					strcpy(tex->name, "SpiritObject.png");

					s_editorLevel.textureMap[tex->name] = tex;
				}

			} break;
			case CLASS_SAFE:
			{
				tex = getEditorTexture("SafeObject.png");
				if (tex) { return tex; }

				DXL2_Paths::appendPath(DXL2_PathType::PATH_PROGRAM, "UI_Images/SafeObject.png", imagePath, DXL2_MAX_PATH);
				Image* image = DXL2_Image::get(imagePath);
				if (image)
				{
					s_objIcons.push_back({});
					tex = &s_objIcons.back();
					tex->scale = { 1.0f, 1.0f };

					tex->texture = DXL2_RenderBackend::createTexture(image->width, image->height, image->data);
					tex->width = image->width;
					tex->height = image->height;
					strcpy(tex->name, "SafeObject.png");

					s_editorLevel.textureMap[tex->name] = tex;
				}
			} break;
			case CLASS_SPRITE:
			{
				const Sprite* sprite = DXL2_Sprite::getSprite(dataFile);
				if (sprite)
				{
					s_objIcons.push_back({});
					tex = &s_objIcons.back();
					tex->scale = { 1.0f, 1.0f };

					const u32* trueColor = convertPalImageToTrueColor(sprite->frames[0].width, sprite->frames[0].height, sprite->frames[0].image);
					tex->texture = DXL2_RenderBackend::createTexture(sprite->frames[0].width, sprite->frames[0].height, trueColor);
					tex->width   = sprite->frames[0].width;
					tex->height  = sprite->frames[0].height;
					strcpy(tex->name, dataFile);

					tex->scale.x = f32(sprite->anim[0].worldWidth)  * c_spriteWorldScale;
					tex->scale.z = f32(sprite->anim[0].worldHeight) * c_spriteWorldScale;
					tex->rect[0] = (f32)sprite->frames[0].rect[0];
					tex->rect[1] = (f32)sprite->frames[0].rect[1];
					tex->rect[2] = (f32)sprite->frames[0].rect[2];
					tex->rect[3] = (f32)sprite->frames[0].rect[3];

					s_editorLevel.textureMap[tex->name] = tex;
				}
			} break;
			case CLASS_FRAME:
			{
				const Frame* frame = DXL2_Sprite::getFrame(dataFile);
				if (frame)
				{
					s_objIcons.push_back({});
					tex = &s_objIcons.back();
					tex->scale = { 1.0f, 1.0f };

					const u32* trueColor = convertPalImageToTrueColor(frame->width, frame->height, frame->image);
					tex->texture = DXL2_RenderBackend::createTexture(frame->width, frame->height, trueColor);
					tex->width   = frame->width;
					tex->height  = frame->height;
					strcpy(tex->name, dataFile);

					tex->rect[0] = (f32)frame->rect[0];
					tex->rect[1] = (f32)frame->rect[1];
					tex->rect[2] = (f32)frame->rect[2];
					tex->rect[3] = (f32)frame->rect[3];

					s_editorLevel.textureMap[tex->name] = tex;
				}
			} break;
			case CLASS_3D:
			{
			} break;
			case CLASS_SOUND:
			{
				tex = getEditorTexture("SoundObject.png");
				if (tex) { return tex; }
								
				DXL2_Paths::appendPath(DXL2_PathType::PATH_PROGRAM, "UI_Images/SoundObject.png", imagePath, DXL2_MAX_PATH);
				Image* image = DXL2_Image::get(imagePath);
				if (image)
				{
					s_objIcons.push_back({});
					tex = &s_objIcons.back();
					tex->scale = { 1.0f, 1.0f };

					tex->texture = DXL2_RenderBackend::createTexture(image->width, image->height, image->data);
					tex->width = image->width;
					tex->height = image->height;
					strcpy(tex->name, "SoundObject.png");

					s_editorLevel.textureMap[tex->name] = tex;
				}
			} break;
		};
		return tex;
	}

	void freeTextures()
	{
		const size_t count = s_editorLevel.textures.size();
		for (size_t t = 0; t < count; t++)
		{
			DXL2_RenderBackend::freeTexture(s_editorLevel.textures[t].texture);
		}
		const size_t iconCount = s_objIcons.size();
		for (size_t t = 0; t < iconCount; t++)
		{
			DXL2_RenderBackend::freeTexture(s_objIcons[t].texture);
		}
		s_objIcons.clear();
		s_editorLevel.textures.clear();
		s_editorLevel.textureMap.clear();
	}

	void loadTextures(const Palette256* pal)
	{
		freeTextures();
		s_pal = pal;

		// Load textures from custom GOBs

		// Load the textures from the resources (TEXTURES.GOB by default).
		char textureGobPath[DXL2_MAX_PATH];
		DXL2_Paths::appendPath(DXL2_PathType::PATH_SOURCE_DATA, "TEXTURES.GOB", textureGobPath);
		Archive* textureArchive = Archive::getArchive(ARCHIVE_GOB, "TEXTURES.GOB", textureGobPath);

		const u32 fileCount = textureArchive->getFileCount();
		s_editorLevel.textures.clear();
		s_editorLevel.textures.reserve(fileCount);
		// Skip ahead past weapon textures.
		for (u32 i = TEXTURES_GOB_START_TEX; i < fileCount; i++)
		{
			const char* name = textureArchive->getFileName(i);
			const size_t len = strlen(name);
			if (name[len - 2] != 'B' || name[len - 1] != 'M') { continue; }

			const Texture* texture = DXL2_Texture::get(name);
			const u32 w = texture->frames[0].width;
			const u32 h = texture->frames[0].height;

			s_textureBuffer.resize(w * h);
			u32* trueColor = s_textureBuffer.data();

			const u32 pixelCount = w * h;
			for (u32 y = 0; y < h; y++)
			{
				for (u32 x = 0; x < w; x++)
				{
					const u32 dstIndex = (h - y - 1)*w + x;
					const u8 srcTexel = texture->frames[0].image[x*h + y];

					trueColor[dstIndex] = pal->colors[srcTexel];
					if (srcTexel != 0u || texture->frames[0].opacity != OPACITY_TRANS)
					{
						trueColor[dstIndex] |= 0xff000000;
					}
					else
					{
						trueColor[dstIndex] &= 0x00ffffff;
					}
				}
			}

			TextureGpu* textureGpu = DXL2_RenderBackend::createTexture(w, h, trueColor);
			EditorTexture editorTexture =
			{
				textureGpu,
				w, h
			};
			strcpy(editorTexture.name, name);
			// The texture vector was pre-allocated, so pointers stay valid.
			s_editorLevel.textures.push_back(editorTexture);
			s_editorLevel.textureMap[name] = &s_editorLevel.textures.back();
		}
	}

	void copyTexture(EditorSectorTexture* dst, const SectorTexture* src, const LevelData* levelData)
	{
		const bool hasTex = src->texId >= 0 && levelData->textures[src->texId];

		dst->offsetX = hasTex ? src->offsetX : 0;
		dst->offsetY = hasTex ? src->offsetY : 0;
		dst->flag    = hasTex ? src->flag    : 0;
		dst->frame   = hasTex ? src->frame   : 0;
		dst->tex     = hasTex ? getEditorTexture(levelData->textures[src->texId]) : nullptr;
	}
	
	bool convertLevelDataToEditor(const LevelData* levelData, const Palette256* palette, const InfData* infData, const LevelObjectData* objData)
	{
		if (!levelData) { return false; }
		loadTextures(palette);

		const size_t sectorCount = levelData->sectors.size();
		s_editorLevel.sectors.resize(sectorCount);

		s_editorLevel.layerMin = levelData->layerMin;
		s_editorLevel.layerMax = levelData->layerMax;
		s_editorLevel.name     = levelData->name;
		s_editorLevel.parallax[0] = levelData->parallax[0];
		s_editorLevel.parallax[1] = levelData->parallax[1];

		const Sector* src = levelData->sectors.data();
		EditorSector* dst = s_editorLevel.sectors.data();
		for (size_t s = 0; s < sectorCount; s++, src++, dst++)
		{
			dst->id = u32(s);
			dst->objects.clear();

			if (src->name[0]) { strcpy(dst->name, src->name); }
			else { dst->name[0] = 0; }

			dst->ambient = src->ambient;
			copyTexture(&dst->floorTexture, &src->floorTexture, levelData);
			copyTexture(&dst->ceilTexture,  &src->ceilTexture,  levelData);

			dst->floorAlt = src->floorAlt;
			dst->ceilAlt  = src->ceilAlt;
			dst->secAlt   = src->secAlt;
			dst->layer    = src->layer;
			dst->infType  = INF_NONE;
			memcpy(dst->flags, src->flags, sizeof(u32) * 3);

			// Dynamically resizable, self-contained geometry data.
			dst->walls.resize(src->wallCount);
			dst->vertices.resize(src->vtxCount);

			const SectorWall* srcWall = levelData->walls.data() + src->wallOffset;
			const Vec2f*      srcVtx  = levelData->vertices.data() + src->vtxOffset;
			memcpy(dst->vertices.data(), srcVtx, sizeof(Vec2f) * src->vtxCount);
			EditorWall* dstWall = dst->walls.data();

			for (u32 w = 0; w < src->wallCount; w++, srcWall++, dstWall++)
			{
				copyTexture(&dstWall->mid,  &srcWall->mid,  levelData);
				copyTexture(&dstWall->top,  &srcWall->top,  levelData);
				copyTexture(&dstWall->bot,  &srcWall->bot,  levelData);
				copyTexture(&dstWall->sign, &srcWall->sign, levelData);
				dstWall->i0     = srcWall->i0;
				dstWall->i1     = srcWall->i1;
				dstWall->adjoin = srcWall->adjoin;
				dstWall->walk   = srcWall->walk;
				dstWall->mirror = srcWall->mirror;
				dstWall->light  = srcWall->light;
				dstWall->infType = INF_NONE;
				memcpy(dstWall->flags, srcWall->flags, sizeof(u32) * 4);
			}

			// Polygon data.
			triangulateSector(dst, &dst->triangles);
			dst->needsUpdate = false;
		}

		convertInfToEditor(infData);
		convertObjectsToEditor(objData);

		// Go through the sectors and walls and assign INF items.
		dst = s_editorLevel.sectors.data();
		for (size_t s = 0; s < sectorCount; s++, src++, dst++)
		{
			dst->infItem = findInfItem(dst, -1);

			const s32 wallCount = (s32)dst->walls.size();
			for (s32 w = 0; w < wallCount; w++)
			{
				dst->walls[w].infItem = findInfItem(dst, w);
			}
		}
		determineSectorTypes();
		
		return true;
	}

	void updateSectors()
	{
		const size_t sectorCount = s_editorLevel.sectors.size();
		EditorSector* sector = s_editorLevel.sectors.data();
		for (size_t s = 0; s < sectorCount; s++, sector++)
		{
			if (sector->needsUpdate)
			{
				sector->triangles.count = 0;
				sector->triangles.vtx.clear();

				triangulateSector(sector, &sector->triangles);
				sector->needsUpdate = false;
			}
		}
	}

	s32 loadRuntimeTexture(const char* name, LevelData* output)
	{
		// is it already in the list?
		const size_t texCount = output->textures.size();
		Texture** textures = output->textures.data();
		for (size_t t = 0; t < texCount; t++)
		{
			if (strcasecmp(name, textures[t]->name) == 0)
			{
				return s32(t);
			}
		}

		s32 texId = -1;
		Texture* tex = DXL2_Texture::get(name);
		if (tex)
		{
			texId = (s32)texCount;
			output->textures.push_back(tex);
		}
		return texId;
	}

	void copyTexture(SectorTexture* dst, const EditorSectorTexture* src, LevelData* output)
	{
		const bool hasTex = src->tex;

		dst->offsetX = hasTex ? src->offsetX : 0;
		dst->offsetY = hasTex ? src->offsetY : 0;
		dst->baseOffsetX = dst->offsetX;
		dst->baseOffsetY = dst->offsetY;
		dst->flag  = hasTex ? src->flag : 0;
		dst->frame = hasTex ? src->frame : 0;

		dst->texId = -1;
		if (hasTex)
		{
			dst->texId = loadRuntimeTexture(src->tex->name, output);
		}
	}

	bool generateLevelData()
	{
		if (s_editorLevel.sectors.empty())
		{
			return false;
		}

		// Unload the level asset once it has been converted.
		DXL2_LevelAsset::unload();
		LevelData* output = DXL2_LevelAsset::getLevelData();
		output->textures.clear();
		output->layerMin = s_editorLevel.layerMin;
		output->layerMax = s_editorLevel.layerMax;
		strcpy(output->name, s_editorLevel.name.c_str());
		output->parallax[0] = s_editorLevel.parallax[0];
		output->parallax[1] = s_editorLevel.parallax[1];

		const u32 sectorCount = (u32)s_editorLevel.sectors.size();
		output->sectors.resize(sectorCount);
		// Get the total wall and vertex counts.
		u32 vertexCount = 0;
		u32 wallCount = 0;
		const EditorSector* srcSector = s_editorLevel.sectors.data();
		Sector* dstSector = output->sectors.data();
		for (u32 s = 0; s < sectorCount; s++, srcSector++, dstSector++)
		{
			dstSector->vtxOffset  = vertexCount;
			dstSector->wallOffset = wallCount;
			dstSector->vtxCount   = (u32)srcSector->vertices.size();
			dstSector->wallCount  = (u32)srcSector->walls.size();

			vertexCount += dstSector->vtxCount;
			wallCount   += dstSector->wallCount;

			dstSector->ambient  = srcSector->ambient;
			dstSector->floorAlt = srcSector->floorAlt;
			dstSector->ceilAlt  = srcSector->ceilAlt;
			dstSector->secAlt   = srcSector->secAlt;
			dstSector->layer    = srcSector->layer;
			dstSector->id       = srcSector->id;
			strcpy(dstSector->name, srcSector->name);
			memcpy(dstSector->flags, srcSector->flags, sizeof(u32) * 3);

			copyTexture(&dstSector->floorTexture, &srcSector->floorTexture, output);
			copyTexture(&dstSector->ceilTexture,  &srcSector->ceilTexture,  output);
		}

		output->vertices.resize(vertexCount);
		output->walls.resize(wallCount);
		srcSector = s_editorLevel.sectors.data();
		dstSector = output->sectors.data();
		for (u32 s = 0; s < sectorCount; s++, srcSector++, dstSector++)
		{
			Vec2f* outVtx = output->vertices.data() + dstSector->vtxOffset;
			memcpy(outVtx, srcSector->vertices.data(), dstSector->vtxCount * sizeof(Vec2f));

			const EditorWall* srcWall = srcSector->walls.data();
			SectorWall* outWall = output->walls.data() + dstSector->wallOffset;
			for (u32 w = 0; w < dstSector->wallCount; w++, srcWall++, outWall++)
			{
				outWall->i0     = srcWall->i0;
				outWall->i1     = srcWall->i1;
				outWall->adjoin = srcWall->adjoin;
				outWall->walk   = srcWall->walk;
				outWall->mirror = srcWall->mirror;
				outWall->light  = srcWall->light;
				memcpy(outWall->flags, srcWall->flags, sizeof(u32) * 4);

				copyTexture(&outWall->mid, &srcWall->mid, output);
				copyTexture(&outWall->top, &srcWall->top, output);
				copyTexture(&outWall->bot, &srcWall->bot, output);
				copyTexture(&outWall->sign, &srcWall->sign, output);
			}
		}

		// Convert editor level data to runtime level data.
		return true;
	}

	u32 getObjectDataOffset(const char* dataFile, ObjectClass oclass, LevelObjectData* output)
	{
		u32 dataOffset = 0u;
		if (oclass == CLASS_SPRITE)
		{
			const u32 count = (u32)output->sprites.size();
			for (u32 i = 0; i < count; i++)
			{
				if (strcasecmp(output->sprites[i].c_str(), dataFile) == 0)
				{
					return i;
				}
			}
			dataOffset = (u32)output->sprites.size();
			output->sprites.push_back(dataFile);
		}
		else if (oclass == CLASS_FRAME)
		{
			const u32 count = (u32)output->frames.size();
			for (u32 i = 0; i < count; i++)
			{
				if (strcasecmp(output->frames[i].c_str(), dataFile) == 0)
				{
					return i;
				}
			}
			dataOffset = (u32)output->frames.size();
			output->frames.push_back(dataFile);
		}
		else if (oclass == CLASS_3D)
		{
			const u32 count = (u32)output->pods.size();
			for (u32 i = 0; i < count; i++)
			{
				if (strcasecmp(output->pods[i].c_str(), dataFile) == 0)
				{
					return i;
				}
			}
			dataOffset = (u32)output->pods.size();
			output->pods.push_back(dataFile);
		}
		else if (oclass == CLASS_SOUND)
		{
			const u32 count = (u32)output->sounds.size();
			for (u32 i = 0; i < count; i++)
			{
				if (strcasecmp(output->sounds[i].c_str(), dataFile) == 0)
				{
					return i;
				}
			}
			dataOffset = (u32)output->sounds.size();
			output->sounds.push_back(dataFile);
		}

		return dataOffset;
	}

	bool generateObjects()
	{
		DXL2_LevelObjects::unload();
		LevelObjectData* output = DXL2_LevelObjects::getLevelObjectData();
				
		u32 objCount = 0;
		const u32 sectorCount = (u32)s_editorLevel.sectors.size();
		const EditorSector* sector = s_editorLevel.sectors.data();
		for (u32 s = 0; s < sectorCount; s++, sector++)
		{
			objCount += (u32)sector->objects.size();
		}

		output->objects.resize(objCount);
		output->objectCount = objCount;

		sector = s_editorLevel.sectors.data();
		LevelObject* dstObj = output->objects.data();
		for (u32 s = 0; s < sectorCount; s++, sector++)
		{
			const u32 objCount = (u32)sector->objects.size();
			const EditorLevelObject* srcObj = sector->objects.data();
			for (u32 i = 0; i < objCount; i++, srcObj++, dstObj++)
			{
				dstObj->dataOffset = getObjectDataOffset(srcObj->dataFile.c_str(), srcObj->oclass, output);

				dstObj->oclass = srcObj->oclass;
				dstObj->pos = srcObj->pos;
				dstObj->orientation = srcObj->orientation;
				dstObj->difficulty = srcObj->difficulty;
				dstObj->logics = srcObj->logics;
				dstObj->generators = srcObj->generators;

				dstObj->comFlags = srcObj->comFlags;
				dstObj->radius = srcObj->radius;
				dstObj->height = srcObj->height;
			}
		}
		
		return true;
	}

	bool generateInfAsset()
	{
		if (s_editorInf.item.empty()) { return false; }
		// Clear the INF data and initialize the memory pool.
		DXL2_InfAsset::unload();
		InfData* output = DXL2_InfAsset::getInfData();
		MemoryPool* memoryPool = DXL2_InfAsset::getMemoryPool(true);

		output->itemCount = (u32)s_editorInf.item.size();
		output->completeId = -1;

		const u32 secFlagDoorCount = DXL2_InfAsset::countSectorFlagDoors();
		output->item = (InfItem*)memoryPool->allocate(sizeof(InfItem) * (output->itemCount + secFlagDoorCount));

		InfItem* dstItem = output->item;
		const EditorInfItem* srcItem = s_editorInf.item.data();
		for (u32 i = 0; i < output->itemCount; i++, srcItem++, dstItem++)
		{
			if (srcItem->sectorName.empty())
			{
				dstItem->id = 0xffffu;
			}
			else
			{
				dstItem->id = getSector(srcItem->sectorName.c_str())->id;
			}

			if (strcasecmp(srcItem->sectorName.c_str(), "complete") == 0)
			{
				output->completeId = i;
			}

			if (srcItem->wallId >= 0)
			{
				dstItem->id |= (srcItem->wallId << 16u);
			}
			else
			{
				dstItem->id |= (0xffffu << 16u);
			}

			dstItem->type = dstItem->type;
			dstItem->classCount = (u32)srcItem->classData.size();
			dstItem->classData = (InfClassData*)memoryPool->allocate(sizeof(InfClassData) * dstItem->classCount);

			InfClassData* dstClass = dstItem->classData;
			const EditorInfClassData* srcClass = srcItem->classData.data();
			for (u32 c = 0; c < dstItem->classCount; c++, srcClass++, dstClass++)
			{
				dstClass->iclass = srcClass->iclass;
				dstClass->isubclass = srcClass->isubclass;
				dstClass->stateIndex = 0;

				//Variables.
				dstClass->var.master = srcClass->var.master;
				dstClass->var.event = srcClass->var.event;
				dstClass->var.start = srcClass->var.start;
				dstClass->var.event_mask = srcClass->var.event_mask;
				dstClass->var.entity_mask = srcClass->var.entity_mask;
				dstClass->var.key = srcClass->var.key;
				dstClass->var.flags = srcClass->var.flags;
				dstClass->var.center = srcClass->var.center;
				dstClass->var.angle = srcClass->var.angle;
				dstClass->var.speed = srcClass->var.speed;
				for (u32 s = 0; s < 3; s++)
				{
					dstClass->var.sound[s] = srcClass->var.sound[s];
				}
				dstClass->var.target = srcClass->var.target != "" ? getSector(srcClass->var.target.c_str())->id : -1;

				//Stops.
				dstClass->stopCount = (u32)srcClass->stop.size();
				dstClass->stop = (InfStop*)memoryPool->allocate(sizeof(InfStop) * dstClass->stopCount);
				InfStop* dstStop = dstClass->stop;
				const EditorInfStop* srcStop = srcClass->stop.data();
				for (u32 s = 0; s < dstClass->stopCount; s++, srcStop++, dstStop++)
				{
					const u32 funcCount = (u32)srcStop->func.size();
					dstStop->code = srcStop->stopValue0Type | (srcStop->stopValue1Type << 4u) | (funcCount << 8u);
					dstStop->time = srcStop->time;

					if (srcStop->stopValue0Type == INF_STOP0_SECTORNAME)
					{
						dstStop->value0.iValue = getSector(srcStop->value0.sValue.c_str())->id;
					}
					else
					{
						dstStop->value0.fValue = srcStop->value0.fValue;
					}

					dstStop->func = (InfFunction*)memoryPool->allocate(sizeof(InfFunction) * funcCount);
					InfFunction* dstFunc = dstStop->func;
					const EditorInfFunction* srcFunc = srcStop->func.data();
					for (u32 f = 0; f < funcCount; f++, srcFunc++, dstFunc++)
					{
						const u32 clientCount = (u32)srcFunc->client.size();
						const u32 argCount = (u32)srcFunc->arg.size();
						dstFunc->code = srcFunc->funcId | (clientCount << 8u) | (argCount << 16u);

						dstFunc->client = (u32*)memoryPool->allocate(sizeof(u32) * clientCount);
						dstFunc->arg = (InfArg*)memoryPool->allocate(sizeof(InfArg) * argCount);
						for (u32 j = 0; j < clientCount; j++)
						{
							const u32 sectorId = srcFunc->client[j].sectorName != "" ? getSector(srcFunc->client[j].sectorName.c_str())->id : 0xffffu;
							const u32 wallId = srcFunc->client[j].wallId >= 0 ? (u32)srcFunc->client[j].wallId : 0xffffu;
							dstFunc->client[j] = sectorId | (wallId << 16u);
						}

						if (srcFunc->funcId < INF_MSG_LIGHTS)
						{
							for (u32 j = 0; j < argCount; j++)
							{
								dstFunc->arg[j].iValue = srcFunc->arg[j].iValue;
							}
						}
						else if (srcFunc->funcId == INF_MSG_ADJOIN)
						{
							dstFunc->arg[0].iValue = getSector(srcFunc->arg[0].sValue.c_str())->id;
							dstFunc->arg[1].iValue = srcFunc->arg[1].iValue;
							dstFunc->arg[2].iValue = getSector(srcFunc->arg[2].sValue.c_str())->id;
							dstFunc->arg[3].iValue = srcFunc->arg[3].iValue;
						}
						else if (srcFunc->funcId == INF_MSG_PAGE)
						{
							// TODO: This is wrong, fix me.
							dstFunc->arg[0].iValue = srcFunc->arg[0].iValue;
						}
						else if (srcFunc->funcId == INF_MSG_TEXT)
						{
							dstFunc->arg[0].iValue = srcFunc->arg[0].iValue;
						}
						else if (srcFunc->funcId == INF_MSG_TEXTURE)
						{
							dstFunc->arg[0].iValue = srcFunc->arg[0].iValue;
							dstFunc->arg[1].iValue = getSector(srcFunc->arg[1].sValue.c_str())->id;
						}
					}
				}

				//Slaves.
				dstClass->mergeStart = -1;
				dstClass->mergeStart = -1;
				dstClass->slaveCount = 0;
				dstClass->slaves = nullptr;

				if (dstItem->type == INF_ITEM_SECTOR)
				{
					// Search for all sectors with the same name as the current but different ids.
					// These will be added to the slaves at the "merge sectors" position.
					std::vector<u32> mergeSectors;
					const u32 sectorCount = (u32)s_editorLevel.sectors.size();
					const EditorSector* sector = s_editorLevel.sectors.data();
					const char* name = srcItem->sectorName.c_str();
					const u32 curSectorId = dstItem->id & 0xffffu;
					if (name[0])
					{
						for (u32 s = 0; s < sectorCount; s++, sector++)
						{
							if (sector->name[0] && strcasecmp(name, sector->name) == 0 && sector->id != curSectorId)
							{
								mergeSectors.push_back(s);
							}
						}
					}

					if (!mergeSectors.empty())
					{
						dstClass->mergeStart = (s32)srcClass->slaves.size();
					}

					dstClass->slaveCount = u32(srcClass->slaves.size() + mergeSectors.size());
					dstClass->slaves = (u16*)memoryPool->allocate(sizeof(u16)*dstClass->slaveCount);

					for (u32 s = 0; s < srcClass->slaves.size(); s++)
					{
						dstClass->slaves[s] = getSector(srcClass->slaves[s].c_str())->id;
					}
					for (u32 s = 0; s < mergeSectors.size(); s++)
					{
						dstClass->slaves[s + dstClass->mergeStart] = mergeSectors[s];
					}
				}
			}
		}
		DXL2_InfAsset::setupDoors();

		return true;
	}

	EditorLevel* getEditorLevelData()
	{
		return &s_editorLevel;
	}
		
	EditorInfItem* findInfItem(const EditorSector* sector, s32 wall)
	{
		if (sector->name[0] == 0) { return nullptr; }

		// This sector may have the same name as another...
		// For INF, we take only the first name in the list.
		const size_t sectorCount = s_editorLevel.sectors.size();
		const EditorSector* testSector = s_editorLevel.sectors.data();
		for (size_t s = 0; s < sectorCount; s++, testSector++)
		{
			if (strcasecmp(testSector->name, sector->name) == 0 && sector->id > testSector->id)
			{
				return nullptr;
			}
		}

		// Once that is resolved, look for a matching INF item.
		const size_t count = s_editorInf.item.size();
		EditorInfItem* item = s_editorInf.item.data();
		for (u32 i = 0; i < count; i++, item++)
		{
			if (strcasecmp(item->sectorName.c_str(), sector->name) == 0 && item->wallId == wall)
			{
				return item;
			}
		}
		return nullptr;
	}

	InfEditState* getInfEditState()
	{
		return &s_infEditState;
	}

	void determineSectorTypes()
	{
		const u32 sectorCount = (u32)s_editorLevel.sectors.size();
		EditorSector* sector = s_editorLevel.sectors.data();
		for (u32 s = 0; s < sectorCount; s++, sector++)
		{
			sector->infType = INF_NONE;
			const EditorInfItem* sectorItem = sector->infItem;
			if (sectorItem)
			{
				// Check to see if there are any messages or stops.
				sector->infType = INF_SELF;
				for (u32 c = 0; c < (u32)sectorItem->classData.size(); c++)
				{
					if (!sectorItem->classData[c].stop.empty())
					{
						sector->infType = INF_OTHER;
						break;
					}
				}
			}
			if (sector->flags[0] & SEC_FLAGS1_DOOR)
			{
				sector->infType = INF_OTHER;
			}

			const u32 wallCount = (u32)sector->walls.size();
			EditorWall* wall = sector->walls.data();
			for (u32 w = 0; w < wallCount; w++, wall++)
			{
				EditorInfItem* wallItem = wall->infItem;
				if (wallItem)
				{
					wall->infType = INF_OTHER;
					wall->triggerType = wallItem->classData[0].isubclass < TRIGGER_SWITCH1 ? TRIGGER_LINE : TRIGGER_SWITCH;
				}
				else
				{
					wall->infType = INF_NONE;
				}
			}
		}
	}

	// Pre-triangulate sectors and modify as need during editing.
	void triangulateSector(const EditorSector* sector, SectorTriangles* outTri)
	{
		const u32 wallCount = (u32)sector->walls.size();
		const Vec2f* vtx = sector->vertices.data();
		
		// TODO: Fuel Station sectors 376, 377 and 388 have strange contours that are specified out of order.
		// This causes the resulting polygons to be incorrect.
		
		// Find the contours.
		static Polygon contours[128];
		const EditorWall* wall = sector->walls.data();
		u32 start = wall->i0;
		u32 contourCount = 0;
		Polygon* curCon = &contours[contourCount];
		curCon->vtxCount = 0;
		contourCount++;

		for (u32 w = 0; w < wallCount; w++, wall++)
		{
			// Keep going until i1 == start
			curCon->vtx[curCon->vtxCount++] = vtx[wall->i0];
			if (wall->i1 == start)
			{
				if (w < wallCount - 1)
				{
					start = (wall + 1)->i0;
					curCon = &contours[contourCount];
					curCon->vtxCount = 0;
					contourCount++;
				}
			}
		}

		u32 triCount = 0;
		Triangle* triangle = DXL2_Polygon::decomposeComplexPolygon(contourCount, contours, &triCount);
		if (!triangle || triCount == 0) { return; }

		// Count the number of triangles
		outTri->count = triCount;
		outTri->vtx.resize(triCount * 3);

		u32 tIdx = 0;
		for (u32 p = 0; p < triCount; p++, triangle++)
		{
			outTri->vtx[p * 3 + 0] = triangle->vtx[0];
			outTri->vtx[p * 3 + 1] = triangle->vtx[1];
			outTri->vtx[p * 3 + 2] = triangle->vtx[2];
		}
	}

	EditorSector* getSector(const char* name)
	{
		if (s_editorLevel.sectors.empty()) { return nullptr; }

		const s32 sectorCount = (s32)s_editorLevel.sectors.size();
		EditorSector* sector = s_editorLevel.sectors.data();

		for (s32 i = 0; i < sectorCount; i++, sector++)
		{
			if (strcasecmp(sector->name, name) == 0)
			{
				return sector;
			}
		}
		return nullptr;
	}

	s32 findSector(s32 layer, const Vec2f* pos)
	{
		if (s_editorLevel.sectors.empty()) { return -1; }

		const s32 sectorCount = (s32)s_editorLevel.sectors.size();
		const EditorSector* sectors = s_editorLevel.sectors.data();

		for (s32 i = 0; i < sectorCount; i++)
		{
			if (sectors[i].layer != layer) { continue; }
			if (Geometry::pointInSector(pos, (u32)sectors[i].vertices.size(), sectors[i].vertices.data(), (u32)sectors[i].walls.size(), (u8*)sectors[i].walls.data(), sizeof(EditorWall)))
			{
				return i;
			}
		}
		return -1;
	}

	#define HEIGHT_EPS 0.5f

	s32 findSector(const Vec3f* pos)
	{
		const s32 sectorCount = (s32)s_editorLevel.sectors.size();
		const EditorSector* sectors = s_editorLevel.sectors.data();

		const Vec2f mapPos = { pos->x, pos->z };
		s32 insideCount = 0;
		s32 insideIndices[256];

		// sometimes objects can be in multiple valid sectors, so pick the best one.
		for (s32 i = 0; i < sectorCount; i++)
		{
			if (Geometry::pointInSector(&mapPos, (u32)sectors[i].vertices.size(), sectors[i].vertices.data(), (u32)sectors[i].walls.size(), (u8*)sectors[i].walls.data(), sizeof(EditorWall)))
			{
				assert(insideCount < 256);
				insideIndices[insideCount++] = i;
			}
		}
		// if the object isn't inside a sector than return.
		if (!insideCount) { return -1; }

		// First see if its actually inside any sectors based on height.
		// If so, pick the sector where it is closest to the floor.
		f32 minHeightDiff = FLT_MAX;
		s32 closestFit = -1;
		for (s32 i = 0; i < insideCount; i++)
		{
			const EditorSector* sector = &sectors[insideIndices[i]];
			if (pos->y >= sector->ceilAlt - HEIGHT_EPS && pos->y <= sector->floorAlt + HEIGHT_EPS)
			{
				f32 diff = fabsf(pos->y - sector->floorAlt);
				if (diff < minHeightDiff)
				{
					minHeightDiff = diff;
					closestFit = insideIndices[i];
				}
			}
		}
		if (closestFit >= 0)
		{
			return closestFit;
		}

		// If no valid height range is found, just pick whatever is closest and hope for the best.
		for (s32 i = 0; i < insideCount; i++)
		{
			const EditorSector* sector = &sectors[insideIndices[i]];
			const f32 diff = fabsf(pos->y - sector->floorAlt);
			if (diff < minHeightDiff)
			{
				minHeightDiff = diff;
				closestFit = insideIndices[i];
			}
		}

		//assert(0);
		return closestFit;
	}

	s32 findClosestWallInSector(const EditorSector* sector, const Vec2f* pos, f32 maxDistSq, f32* minDistToWallSq)
	{
		const u32 count = (u32)sector->walls.size();
		f32 minDistSq = FLT_MAX;
		s32 closestId = -1;
		const EditorWall* walls = sector->walls.data();
		const Vec2f* vertices = sector->vertices.data();
		for (u32 w = 0; w < count; w++)
		{
			const Vec2f* v0 = &vertices[walls[w].i0];
			const Vec2f* v1 = &vertices[walls[w].i1];

			Vec2f pointOnSeg;
			Geometry::closestPointOnLineSegment(*v0, *v1, *pos, &pointOnSeg);
			const Vec2f diff = { pointOnSeg.x - pos->x, pointOnSeg.z - pos->z };
			const f32 distSq = diff.x*diff.x + diff.z*diff.z;

			if (distSq < maxDistSq && distSq < minDistSq && (!minDistToWallSq || distSq < *minDistToWallSq))
			{
				minDistSq = distSq;
				closestId = s32(w);
			}
		}
		if (minDistToWallSq)
		{
			*minDistToWallSq = std::min(*minDistToWallSq, minDistSq);
		}
		return closestId;
	}

	// Find the closest wall to 'pos' that lies within sector 'sectorId'
	// Only accept walls within 'maxDist' and in sectors on layer 'layer'
	// Pass -1 to sectorId to choose any sector on the layer, sectorId will be set to the sector.
	s32 findClosestWall(s32* sectorId, s32 layer, const Vec2f* pos, f32 maxDist)
	{
		assert(sectorId && pos);
		const f32 maxDistSq = maxDist * maxDist;
		if (*sectorId >= 0)
		{
			return findClosestWallInSector(&s_editorLevel.sectors[*sectorId], pos, maxDistSq, nullptr);
		}

		const s32 sectorCount = (s32)s_editorLevel.sectors.size();
		const EditorSector* sectors = s_editorLevel.sectors.data();

		f32 minDistSq = FLT_MAX;
		s32 closestId = -1;
		for (s32 i = 0; i < sectorCount; i++)
		{
			if (sectors[i].layer != layer) { continue; }
			const s32 id = findClosestWallInSector(&sectors[i], pos, maxDistSq, &minDistSq);
			if (id >= 0) { *sectorId = i; closestId = id; }
		}

		return closestId;
	}

	f32 rayAabbIntersect(const Vec3f* aabb, const Vec3f* rayInv, const Vec3f* rayOrigin)
	{
		const Vec3f t0 = { (aabb[0].x - rayOrigin->x) * rayInv->x, (aabb[0].y - rayOrigin->y) * rayInv->y, (aabb[0].z - rayOrigin->z) * rayInv->z };
		const Vec3f t1 = { (aabb[1].x - rayOrigin->x) * rayInv->x, (aabb[1].y - rayOrigin->y) * rayInv->y, (aabb[1].z - rayOrigin->z) * rayInv->z };

		f32 tMin = std::min(t0.x, t1.x);
		tMin = std::max(tMin, std::min(t0.y, t1.y));
		tMin = std::max(tMin, std::min(t0.z, t1.z));

		f32 tMax = std::max(t0.x, t1.x);
		tMax = std::min(tMax, std::max(t0.y, t1.y));
		tMax = std::min(tMax, std::max(t0.z, t1.z));
		if (tMax < tMin) { return -1.0f; }

		return tMin >= 0.0f ? tMin : tMax;
	}

	// Traces a ray from outside the level.
	// The ray needs to ignore backfaces and report the first face/sector hit and position.
	bool traceRay(const Ray* ray, RayHitInfoLE* hitInfo)
	{
		if (s_editorLevel.sectors.empty()) { return false; }

		// start with a naive test.
		const s32 sectorCount = (s32)s_editorLevel.sectors.size();
		const EditorSector* sector = s_editorLevel.sectors.data();

		f32 maxDist = ray->maxDist;
		Vec3f origin = ray->origin;
		Vec2f p0xz = { origin.x, origin.z };
		Vec2f p1xz = { p0xz.x + ray->dir.x * maxDist, p0xz.z + ray->dir.z * maxDist };
		Vec2f dirxz = { ray->dir.x, ray->dir.z };

		f32 overallClosestHit = FLT_MAX;
		hitInfo->hitSectorId = -1;
		hitInfo->hitWallId = -1;
		hitInfo->hitPart = HIT_PART_MID;	// 0 = mid, 1 = lower, 2 = upper OR 0 = floor, 1 = ceiling
		hitInfo->hitPoint = { 0 };
		hitInfo->hitObjectId = -1;

		// TODO: Compute bounds for each sector and test ray vs. bounds before testing against the walls and planes.
		// See reference: https://github.com/erich666/GraphicsGems/blob/master/gems/RayBox.c
		// 1. Keep an AABB for each sector (3d)
		// 2. pick 3 candidate planes from the ray direction.
		// For each sector:
		// 1. compute plane intersections (if they exist) if (dir[i] != 0.0) { (plane[i] - origin[i])/dir[i] }
		// 2. find the maximum intersection.
		// 3. check to see if the intersection is actually inside the box.
		// Note if selecting entities, all of them in a sector can be added if the ray hits the box.
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if (sector->layer != ray->layer && ray->layer > -256) { continue; }

			const u32 wallCount = (u32)sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vertices.data();

			// Check the ray against all of the walls and take the closest hit, if any.
			f32 closestHit = FLT_MAX;
			s32 closestWallId = -1;
			for (u32 w = 0; w < wallCount; w++, wall++)
			{
				const s32 i0 = wall->i0;
				const s32 i1 = wall->i1;
				const Vec2f* v0 = &vtx[i0];
				const Vec2f* v1 = &vtx[i1];

				// Is the wall backfacing?
				Vec2f nrm = { -(v1->z - v0->z), v1->x - v0->x };
				if (DXL2_Math::dot(&dirxz, &nrm) < 0.0f) { continue; }

				f32 s, t;
				if (Geometry::lineSegmentIntersect(&p0xz, &p1xz, v0, v1, &s, &t))
				{
					if (s < closestHit)
					{
						// Make sure it isn't "above" the ray.
						const f32 yAtHit = origin.y + ray->dir.y * s * maxDist;
						if (yAtHit < sector->floorAlt + FLT_EPSILON && yAtHit > sector->ceilAlt - FLT_EPSILON)
						{
							// If this is an adjoin, make sure he shouldn't just keep going...
							bool canHit = true;
							if (wall->adjoin >= 0)
							{
								const EditorSector* next = &s_editorLevel.sectors[wall->adjoin];
								canHit = (yAtHit >= next->floorAlt) || (yAtHit <= next->ceilAlt) || (wall->flags[0] & WF1_ADJ_MID_TEX);
							}
							if (canHit)
							{
								closestHit = s;
								closestWallId = w;
							}
						}
					}
				}
			}

			wall = sector->walls.data();

			// Test the closest wall.
			if (closestWallId >= 0)
			{
				closestHit *= maxDist;
				const Vec3f hitPoint = { origin.x + ray->dir.x*closestHit, origin.y + ray->dir.y*closestHit, origin.z + ray->dir.z*closestHit };

				if (wall[closestWallId].adjoin >= 0)
				{
					// given the hit point, is it below the next floor or above the next ceiling?
					const EditorSector* next = &s_editorLevel.sectors[wall[closestWallId].adjoin];
					if ((hitPoint.y >= next->floorAlt || hitPoint.y <= next->ceilAlt) && closestHit < overallClosestHit)
					{
						overallClosestHit = closestHit;
						hitInfo->hitSectorId = s;
						hitInfo->hitWallId = closestWallId;
						hitInfo->hitPart = hitPoint.y >= next->floorAlt ? HIT_PART_BOT : HIT_PART_TOP;
						hitInfo->hitPoint = hitPoint;
					}
					else if ((wall[closestWallId].flags[0] & WF1_ADJ_MID_TEX) && closestHit < overallClosestHit)
					{
						overallClosestHit = closestHit;
						hitInfo->hitSectorId = s;
						hitInfo->hitWallId = closestWallId;
						hitInfo->hitPart = HIT_PART_MID;
						hitInfo->hitPoint = hitPoint;
					}
				}
				else if (closestHit < overallClosestHit)
				{
					overallClosestHit = closestHit;
					hitInfo->hitSectorId = s;
					hitInfo->hitWallId = closestWallId;
					hitInfo->hitPart = HIT_PART_MID;
					hitInfo->hitPoint = hitPoint;
				}
			}

			// Test the floor and ceiling planes.
			const Vec3f planeTest = { origin.x + ray->dir.x*maxDist, origin.y + ray->dir.y*maxDist, origin.z + ray->dir.z*maxDist };
			Vec3f hitPoint;
			if (origin.y < sector->floorAlt && ray->dir.y > 0.0f && Geometry::lineYPlaneIntersect(&origin, &planeTest, sector->floorAlt, &hitPoint))
			{
				const Vec3f diff = { hitPoint.x - origin.x, hitPoint.y - origin.y, hitPoint.z - origin.z };
				const f32 distSq = DXL2_Math::dot(&diff, &diff);
				if (overallClosestHit == FLT_MAX || distSq < overallClosestHit*overallClosestHit)
				{
					// The ray hit the plane but is the intersect point actually inside the sector?
					const Vec2f testPt = { hitPoint.x, hitPoint.z };
					if (Geometry::pointInSector(&testPt, (u32)sector->vertices.size(), vtx, wallCount, (u8*)wall, sizeof(EditorWall)))
					{
						overallClosestHit = sqrtf(distSq);
						hitInfo->hitSectorId = s;
						hitInfo->hitWallId = -1;
						hitInfo->hitPart = HIT_PART_FLOOR;
						hitInfo->hitPoint = hitPoint;
					}
				}
			}
			if (origin.y > sector->ceilAlt && ray->dir.y < 0.0f && Geometry::lineYPlaneIntersect(&origin, &planeTest, sector->ceilAlt, &hitPoint))
			{
				const Vec3f diff = { hitPoint.x - origin.x, hitPoint.y - origin.y, hitPoint.z - origin.z };
				const f32 distSq = DXL2_Math::dot(&diff, &diff);
				if (overallClosestHit == FLT_MAX || distSq < overallClosestHit*overallClosestHit)
				{
					// The ray hit the plane but is the intersect point actually inside the sector?
					const Vec2f testPt = { hitPoint.x, hitPoint.z };
					if (Geometry::pointInSector(&testPt, (u32)sector->vertices.size(), vtx, wallCount, (u8*)wall, sizeof(EditorWall)))
					{
						overallClosestHit = sqrtf(distSq);
						hitInfo->hitSectorId = s;
						hitInfo->hitWallId = -1;
						hitInfo->hitPart = HIT_PART_CEIL;
						hitInfo->hitPoint = hitPoint;
					}
				}
			}
		}

		// Hit objects.
		if (ray->objSelect)
		{
			const Vec3f rayInv = { 1.0f/ray->dir.x, 1.0f/ray->dir.y, 1.0f/ray->dir.z };
			f32 hitDist = (hitInfo->hitSectorId >= 0) ? DXL2_Math::distance(&ray->origin, &hitInfo->hitPoint) : ray->maxDist;

			sector = s_editorLevel.sectors.data();
			for (s32 s = 0; s < sectorCount; s++, sector++)
			{
				if (sector->layer != ray->layer && ray->layer > -256) { continue; }
				const size_t objCount = sector->objects.size();
				if (!objCount) { continue; }

				const EditorLevelObject* obj = sector->objects.data();
				for (size_t o = 0; o < objCount; o++, obj++)
				{
					if (obj->oclass != CLASS_3D)
					{
						// Test ray against AABB.
						const Vec3f aabb[] = { {obj->worldCen.x - obj->worldExt.x, obj->worldCen.y - obj->worldExt.y, obj->worldCen.z - obj->worldExt.z},
											   {obj->worldCen.x + obj->worldExt.x, obj->worldCen.y + obj->worldExt.y, obj->worldCen.z + obj->worldExt.z} };
						const f32 i = rayAabbIntersect(aabb, &rayInv, &ray->origin);
						if (i > 0.0f && i < hitDist)
						{
							hitDist = i;
							hitInfo->hitSectorId = s;
							hitInfo->hitObjectId = s32(o);
							hitInfo->hitWallId   = -1;
							hitInfo->hitPoint    = {ray->origin.x + ray->dir.x*i, ray->origin.y + ray->dir.y*i, ray->origin.z + ray->dir.z*i };
						}
					}
					else
					{
						// Transform the ray into object space and test against the local AABB.
						// This is the same as intersecting the ray with the OOBB in world space.
						const Vec3f* mat33 = obj->rotMtxT.m;
						Vec3f newDir;
						newDir.x = ray->dir.x*mat33[0].x + ray->dir.y*mat33[0].y + ray->dir.z*mat33[0].z;
						newDir.y = ray->dir.x*mat33[1].x + ray->dir.y*mat33[1].y + ray->dir.z*mat33[1].z;
						newDir.z = ray->dir.x*mat33[2].x + ray->dir.y*mat33[2].y + ray->dir.z*mat33[2].z;

						const Vec3f relRayOrigin = { ray->origin.x - obj->pos.x, ray->origin.y - obj->pos.y, ray->origin.z - obj->pos.z };
						Vec3f localRayOrigin;
						localRayOrigin.x = relRayOrigin.x*mat33[0].x + relRayOrigin.y*mat33[0].y + relRayOrigin.z*mat33[0].z;
						localRayOrigin.y = relRayOrigin.x*mat33[1].x + relRayOrigin.y*mat33[1].y + relRayOrigin.z*mat33[1].z;
						localRayOrigin.z = relRayOrigin.x*mat33[2].x + relRayOrigin.y*mat33[2].y + relRayOrigin.z*mat33[2].z;

						const Vec3f newRayInv = { 1.0f / newDir.x, 1.0f / newDir.y, 1.0f / newDir.z };
						const f32 i = rayAabbIntersect(obj->displayModel->localAabb, &newRayInv, &localRayOrigin);
						if (i > 0.0f && i < hitDist)
						{
							hitDist = i;
							hitInfo->hitSectorId = s;
							hitInfo->hitObjectId = s32(o);
							hitInfo->hitWallId   = -1;
							hitInfo->hitPoint    = { ray->origin.x + ray->dir.x*i, ray->origin.y + ray->dir.y*i, ray->origin.z + ray->dir.z*i };
						}
					}
				}
			}
		}

		return hitInfo->hitSectorId >= 0;
	}

	void convertObjectsToEditor(const LevelObjectData* objData)
	{
		const u32 count = objData->objectCount;
		const LevelObject* srcObj = objData->objects.data();
		for (u32 i = 0; i < objData->objectCount; i++, srcObj++)
		{
			// For each object, find the parent sector.
			const s32 sectorId = findSector(&srcObj->pos);
			if (sectorId >= 0)
			{
				s_editorLevel.sectors[sectorId].objects.push_back({});

				EditorLevelObject& dstObj = s_editorLevel.sectors[sectorId].objects.back();
				dstObj.oclass      = srcObj->oclass;
				dstObj.pos         = srcObj->pos;
				dstObj.orientation = srcObj->orientation;
				dstObj.difficulty  = srcObj->difficulty;
				dstObj.logics      = srcObj->logics;
				dstObj.generators  = srcObj->generators;

				dstObj.comFlags = srcObj->comFlags;
				dstObj.radius   = srcObj->radius;
				dstObj.height   = srcObj->height;

				if (srcObj->oclass == CLASS_SPRITE)
				{
					dstObj.dataFile = objData->sprites[srcObj->dataOffset];
				}
				else if (srcObj->oclass == CLASS_FRAME)
				{
					dstObj.dataFile = objData->frames[srcObj->dataOffset];
				}
				else if (srcObj->oclass == CLASS_3D)
				{
					dstObj.dataFile = objData->pods[srcObj->dataOffset];
				}
				else if (srcObj->oclass == CLASS_SOUND)
				{
					dstObj.dataFile = objData->sounds[srcObj->dataOffset];
				}
				else
				{
					dstObj.dataFile.clear();
				}

				dstObj.display = nullptr;
				dstObj.displayModel = nullptr;

				if (srcObj->oclass == CLASS_3D)
				{
					dstObj.displayModel = DXL2_Model::get(dstObj.dataFile.c_str());

					const f32 yaw   = dstObj.orientation.y * PI / 180.0f;
					const f32 pitch = dstObj.orientation.x * PI / 180.0f;
					const f32 roll  = dstObj.orientation.z * PI / 180.0f;
					DXL2_Math::buildRotationMatrix({ roll, yaw, pitch }, dstObj.rotMtx.m);
					dstObj.rotMtxT = DXL2_Math::transpose(dstObj.rotMtx);
				}
				else
				{
					dstObj.display = createObjectTexture(srcObj->oclass, dstObj.dataFile.c_str());

					f32 width  = dstObj.display ? (f32)dstObj.display->width  : 1.0f;
					f32 height = dstObj.display ? (f32)dstObj.display->height : 1.0f;
					// Half width
					f32 w = dstObj.display ? (f32)dstObj.display->width  * dstObj.display->scale.x / 8.0f : 1.0f;
					f32 h = dstObj.display ? (f32)dstObj.display->height * dstObj.display->scale.z / 8.0f : 1.0f;
					f32 y0 = dstObj.pos.y;
					if (dstObj.oclass == CLASS_SPIRIT || dstObj.oclass == CLASS_SAFE || dstObj.oclass == CLASS_SOUND)
					{
						w = 3.0f;
						h = 3.0f;
						if (dstObj.radius > 0.0f)
						{
							w = dstObj.radius * 2.0f;
						}
						if (dstObj.height != 0)
						{
							h = dstObj.height;
						}
					}
					else if (dstObj.display)
					{
						w = dstObj.display->scale.x * fabsf(dstObj.display->rect[0] - dstObj.display->rect[2]) * c_spriteTexelToWorldScale;
						h = dstObj.display->scale.z * fabsf(dstObj.display->rect[1] - dstObj.display->rect[3]) * c_spriteTexelToWorldScale;
						y0 += dstObj.display->scale.z * fabsf(dstObj.display->rect[1]) * c_spriteTexelToWorldScale;
					}

					// AABB
					dstObj.worldCen = { dstObj.pos.x, y0 - h*0.5f, dstObj.pos.z };
					dstObj.worldExt = { w*0.5f, h*0.5f, w*0.5f };
				}
			}
		}
	}

	void convertInfToEditor(const InfData* infData)
	{
		if (!infData)
		{
			s_editorInf.item.clear();
			return;
		}

		// Do not add flag based doors to the editor INF.
		const u32 itemCount = infData->doorStart >= 0 ? infData->doorStart : infData->itemCount;

		s_editorInf.item.resize(itemCount);
		EditorInfItem* dst = s_editorInf.item.data();
		const InfItem* src = infData->item;
		for (u32 i = 0; i < itemCount; i++, src++, dst++)
		{
			const u32 sectorId  = src->id & 0xffffu;
			const u32 srcWallId = (src->id >> 16u) & 0xffffu;
			const s32 wallId    = srcWallId == 0xffffu ? -1 : (s32)srcWallId;

			if (sectorId < 0xffffu && s_editorLevel.sectors[sectorId].name[0])
				dst->sectorName = s_editorLevel.sectors[sectorId].name;
			else
				dst->sectorName.clear();

			dst->wallId = wallId;
			dst->type = src->type;
			dst->classData.resize(src->classCount);

			EditorInfClassData* dstClass = dst->classData.data();
			const InfClassData* srcClass = src->classData;
			for (u32 c = 0; c < src->classCount; c++, srcClass++, dstClass++)
			{
				dstClass->iclass = srcClass->iclass;
				dstClass->isubclass = srcClass->isubclass;

				//Variables.
				dstClass->var.master = srcClass->var.master;
				dstClass->var.event = srcClass->var.event;
				dstClass->var.start = srcClass->var.start;
				dstClass->var.event_mask = srcClass->var.event_mask;
				dstClass->var.entity_mask = srcClass->var.entity_mask;
				dstClass->var.key = srcClass->var.key;
				dstClass->var.flags = srcClass->var.flags;
				dstClass->var.center = srcClass->var.center;
				dstClass->var.angle = srcClass->var.angle;
				dstClass->var.speed = srcClass->var.speed;
				for (u32 s = 0; s < 3; s++)
				{
					dstClass->var.sound[s] = srcClass->var.sound[s];
				}
				dstClass->var.target = srcClass->var.target >= 0 ? s_editorLevel.sectors[srcClass->var.target].name : "";

				//Stops.
				dstClass->stop.resize(srcClass->stopCount);
				EditorInfStop* dstStop = dstClass->stop.data();
				const InfStop* srcStop = srcClass->stop;
				for (u32 s = 0; s < srcClass->stopCount; s++, srcStop++, dstStop++)
				{
					const u32 funcCount = srcStop->code >> 8u;
					dstStop->stopValue0Type = srcStop->code & 0x0fu;
					dstStop->stopValue1Type = (srcStop->code >> 4u) & 0x0fu;
					dstStop->time = srcStop->time;

					if (dstStop->stopValue0Type == INF_STOP0_SECTORNAME)
					{
						dstStop->value0.sValue = s_editorLevel.sectors[srcStop->value0.iValue].name;
					}
					else
					{
						dstStop->value0.fValue = srcStop->value0.fValue;
					}

					dstStop->func.resize(funcCount);
					EditorInfFunction* dstFunc = dstStop->func.data();
					const InfFunction* srcFunc = srcStop->func;
					for (u32 f = 0; f < funcCount; f++, srcFunc++, dstFunc++)
					{
						const u32 funcId = (srcFunc->code) & 0xffu;
						const u32 clientCount = (srcFunc->code >> 8u) & 0xffu;
						const u32 argCount = srcFunc->code >> 16u;

						dstFunc->funcId = funcId;
						dstFunc->client.resize(clientCount);
						dstFunc->arg.resize(argCount);

						for (u32 j = 0; j < clientCount; j++)
						{
							const u32 sectorId = srcFunc->client[j] & 0xffffu;
							const u32 wallId = (srcFunc->client[j] >> 16u) & 0xffffu;

							dstFunc->client[j].sectorName = sectorId < 0xffffu ? s_editorLevel.sectors[sectorId].name : "";
							dstFunc->client[j].wallId = wallId == 0xffffu ? -1 : s32(wallId);
						}

						if (funcId < INF_MSG_LIGHTS)
						{
							for (u32 j = 0; j < argCount; j++)
							{
								dstFunc->arg[j].iValue = srcFunc->arg[j].iValue;
							}
						}
						else if (funcId == INF_MSG_ADJOIN)
						{
							dstFunc->arg[0].sValue = s_editorLevel.sectors[srcFunc->arg[0].iValue].name;
							dstFunc->arg[1].iValue = srcFunc->arg[1].iValue;
							dstFunc->arg[2].sValue = s_editorLevel.sectors[srcFunc->arg[2].iValue].name;
							dstFunc->arg[3].iValue = srcFunc->arg[3].iValue;
						}
						else if (funcId == INF_MSG_PAGE)
						{
							// TODO: This is wrong, fix me.
							dstFunc->arg[0].iValue = srcFunc->arg[0].iValue;
						}
						else if (funcId == INF_MSG_TEXT)
						{
							dstFunc->arg[0].iValue = srcFunc->arg[0].iValue;
						}
						else if (funcId == INF_MSG_TEXTURE)
						{
							dstFunc->arg[0].iValue = srcFunc->arg[0].iValue;
							dstFunc->arg[1].sValue = s_editorLevel.sectors[srcFunc->arg[1].iValue].name;
						}
					}
				}

				//Slaves.
				dstClass->slaves.clear();
				dstClass->slaves.reserve(srcClass->slaveCount);
				for (u32 s = 0; s < srcClass->slaveCount; s++)
				{
					const std::string name = s_editorLevel.sectors[srcClass->slaves[s]].name;
					if (strcasecmp(name.c_str(), dst->sectorName.c_str()) != 0)
					{
						dstClass->slaves.push_back(name);
					}
				}
			}
		}
	}
}

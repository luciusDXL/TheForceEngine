#include <cstring>

#include "spriteAsset_Jedi.h"
#include <TFE_System/system.h>
#include <TFE_FileSystem/physfswrapper.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <TFE_Settings/settings.h>
// TODO: dependency on JediRenderer, this should be refactored...
#include <TFE_Jedi/Renderer/rlimits.h>
//
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Jedi;

namespace TFE_Sprite_Jedi
{
	enum
	{
		PoolDataOffset = 7,
	};

	typedef std::map<std::string, JediFrame*> FrameMap;
	typedef std::map<std::string, JediWax*> SpriteMap;
	typedef std::map<const void*, const HdWax*> HdSpriteMap;
	typedef std::vector<JediFrame*> FrameList;
	typedef std::vector<JediWax*> SpriteList;
	typedef std::vector<HdWax*> HdSpriteList;
	typedef std::vector<std::string> NameList;

	static FrameMap    s_frames[POOL_COUNT];
	static SpriteMap   s_sprites[POOL_COUNT];
	static HdSpriteMap s_hdSprites[POOL_COUNT];
	static FrameList   s_frameList[POOL_COUNT];
	static SpriteList  s_spriteList[POOL_COUNT];
	static HdSpriteList s_hdSpriteList[POOL_COUNT];
	static NameList    s_frameNames[POOL_COUNT];
	static NameList    s_spriteNames[POOL_COUNT];
	static std::vector<u8> s_buffer;

	bool loadFrameHd(const char* name, const JediFrame* frame, AssetPool pool, HdWax* hdWax, const WaxCell* cell)
	{
		char hdPath[TFE_MAX_PATH];
		FileUtil::replaceExtension(name, "fxx", hdPath);

		// If the file doesn't exist, just return - there is no HD asset.
		vpFile file(VPATH_GAME, hdPath, false);
		if (!file)
			return false;
		// Load the raw data from disk.
		unsigned int fileSize = file.size();
		s_buffer.resize(fileSize);
		file.read(s_buffer.data(), fileSize);
		file.close();

		const u8* data = s_buffer.data();
		const s32 entryCount = *((s32*)data); data += 4;
		assert(entryCount == 1);

		hdWax->entryCount = 1;
		hdWax->cells = (HdWaxCell*)malloc(sizeof(HdWaxCell));
		assert(hdWax->cells);

		hdWax->cells[0].pixelCount = *((u32*)data); data += 4 * entryCount;
		hdWax->cells[0].id = 0;

		// Verify that the pixel count is double the original.
		const s32 targetPixelCount = cell->sizeX * cell->sizeY * 4;
		if (targetPixelCount != hdWax->cells[0].pixelCount)
		{
			free(hdWax->cells);
			hdWax->cells = nullptr;
			return false;
		}

		// Image data.
		const u32 size = sizeof(u32) * hdWax->cells[0].pixelCount;
		hdWax->cells[0].data = (u32*)malloc(size);
		memcpy(hdWax->cells[0].data, data, size);
		data += size;

		return true;
	}

	JediFrame* getFrame(const char* name, AssetPool pool)
	{
		FrameMap::iterator iFrame = s_frames[pool].find(name);
		if (iFrame != s_frames[pool].end())
		{
			return iFrame->second;
		}

		// It doesn't exist yet, try to load the frame.
		vpFile file(VPATH_GAME, name, false);
		if (!file)
			return nullptr;
		unsigned int len = file.size();
		s_buffer.resize(len);
		file.read(s_buffer.data(), len);
		file.close();

		const u8* data = s_buffer.data();

		// Determine ahead of time how much we need to allocate.
		const WaxFrame* base_frame = (WaxFrame*)data;
		const WaxCell* base_cell = WAX_CellPtr(data, base_frame);
		const u32 columnSize = base_cell->sizeX * sizeof(u32);

		// This is a "load in place" format in the original code.
		// We are going to allocate new memory and copy the data.
		u8* assetPtr = (u8*)malloc(s_buffer.size() + columnSize);
		JediFrame* asset = (JediFrame*)assetPtr;
		
		memcpy(asset, data, s_buffer.size());

		WaxFrame* frame = asset;
		WaxCell* cell = WAX_CellPtr(asset, frame);
		frame->pool = pool;
		cell->id = 0;

		// After load, the frame data has to be fixed up before rendering.
		// frame sizes remain in fixed point.
		frame->widthWS  = div16(intToFixed16(cell->sizeX), SPRITE_SCALE_FIXED);
		frame->heightWS = div16(intToFixed16(cell->sizeY), SPRITE_SCALE_FIXED);
		
		const s32 offsetX = -intToFixed16(frame->offsetX);
		const s32 offsetY =  intToFixed16(cell->sizeY) + intToFixed16(frame->offsetY);
		frame->offsetX = div16(offsetX, SPRITE_SCALE_FIXED);
		frame->offsetY = div16(offsetY, SPRITE_SCALE_FIXED);

		if (cell->compressed == 1)
		{
			// Update the column offset, it starts right after the cell data.
			cell->columnOffset = frame->cellOffset + sizeof(WaxCell);
		}
		else
		{
			u32* columns = (u32*)((u8*)asset + s_buffer.size());
			// Local pointer.
			cell->columnOffset = u32((u8*)columns - (u8*)asset);
			// Calculate column offsets.
			for (s32 c = 0; c < cell->sizeX; c++)
			{
				columns[c] = cell->sizeY * c;
			}
		}
		
		s_frames[pool][name] = asset;
		s_frameList[pool].push_back(asset);
		s_frameNames[pool].push_back(name);

		// HD Version
		bool canUseHdAsset = true;
		if (pool == POOL_LEVEL && !TFE_Settings::isHdAssetValid(name, HD_ASSET_TYPE_FME))
		{
			canUseHdAsset = false;
		}
		if (canUseHdAsset)
		{
			HdWax* hdWax = (HdWax*)malloc(sizeof(HdWax));
			if (loadFrameHd(name, asset, pool, hdWax, cell))
			{
				s_hdSpriteList[pool].push_back(hdWax);
				s_hdSprites[pool][asset] = hdWax;
			}
			else
			{
				free(hdWax);
			}
		}
		return asset;
	}

	JediFrame* loadFrameFromMemory(const u8* data, size_t size, bool transformOffsets)
	{
		// Determine ahead of time how much we need to allocate.
		const WaxFrame* base_frame = (WaxFrame*)data;
		const WaxCell* base_cell = WAX_CellPtr(data, base_frame);
		const u32 columnSize = base_cell->sizeX * sizeof(u32);

		// This is a "load in place" format in the original code.
		// We are going to allocate new memory and copy the data.
		u8* assetPtr = (u8*)malloc(size + columnSize);
		JediFrame* asset = (JediFrame*)assetPtr;

		memcpy(asset, data, size);

		WaxFrame* frame = asset;
		WaxCell* cell = WAX_CellPtr(asset, frame);

		// After load, the frame data has to be fixed up before rendering.
		// frame sizes remain in fixed point.
		frame->widthWS = div16(intToFixed16(cell->sizeX), SPRITE_SCALE_FIXED);
		frame->heightWS = div16(intToFixed16(cell->sizeY), SPRITE_SCALE_FIXED);

		const s32 offsetX = -intToFixed16(frame->offsetX);
		const s32 offsetY = intToFixed16(cell->sizeY) + intToFixed16(frame->offsetY);
		if (transformOffsets)
		{
			frame->offsetX = div16(offsetX, SPRITE_SCALE_FIXED);
			frame->offsetY = div16(offsetY, SPRITE_SCALE_FIXED);
		}

		if (cell->compressed == 1)
		{
			// Update the column offset, it starts right after the cell data.
			cell->columnOffset = frame->cellOffset + sizeof(WaxCell);
		}
		else
		{
			u32* columns = (u32*)((u8*)asset + size);
			// Local pointer.
			cell->columnOffset = u32((u8*)columns - (u8*)asset);
			// Calculate column offsets.
			for (s32 c = 0; c < cell->sizeX; c++)
			{
				columns[c] = cell->sizeY * c;
			}
		}
		return asset;
	}

	static std::vector<u32> s_cellOffsets;

	bool isUniqueCell(u32 offset)
	{
		const size_t count = s_cellOffsets.size();
		const u32* offsetList = s_cellOffsets.data();
		for (u32 i = 0; i < count; i++)
		{
			if (offsetList[i] == offset) { return false; }
		}
		s_cellOffsets.push_back(offset);

		return true;
	}

	void sprite_serializeSpritesAndFrames(vpFile* stream)
	{
		const bool modeWrite = serialization_getMode() == SMODE_WRITE;

		if (!modeWrite)
		{
			freeLevelData();
		}

		s32 frameCount, spriteCount;
		if (modeWrite)
		{
			frameCount = (s32)s_frameNames[POOL_LEVEL].size();
			spriteCount = (s32)s_spriteNames[POOL_LEVEL].size();
		}
		SERIALIZE(SaveVersionInit, frameCount, 0);
		SERIALIZE(SaveVersionInit, spriteCount, 0);

		std::string* frameNames  = s_frameNames[POOL_LEVEL].data();
		std::string* spriteNames = s_spriteNames[POOL_LEVEL].data();
		std::string name;
		for (s32 i = 0; i < frameCount; i++)
		{
			u8 size;
			if (modeWrite)
			{
				size = (u8)frameNames[i].length();
			}
			SERIALIZE(SaveVersionInit, size, 0);

			if (!modeWrite)
			{
				name.resize(size);
			}
			SERIALIZE_BUF(SaveVersionInit, modeWrite ? &frameNames[i][0] : &name[0], size);

			if (serialization_getMode() == SMODE_READ)
			{
				getFrame(name.c_str(), POOL_LEVEL);
			}
		}
		for (s32 i = 0; i < spriteCount; i++)
		{
			u8 size;
			if (modeWrite)
			{
				size = (u8)spriteNames[i].length();
			}
			SERIALIZE(SaveVersionInit, size, 0);
			if (!modeWrite)
			{
				name.resize(size);
			}
			SERIALIZE_BUF(SaveVersionInit, modeWrite ? &spriteNames[i][0] : &name[0], size);

			if (serialization_getMode() == SMODE_READ)
			{
				getWax(name.c_str(), POOL_LEVEL);
			}
		}
	}
		
	bool loadWaxHd(const char* name, const JediWax* wax, AssetPool pool, HdWax* hdWax)
	{
		char hdPath[TFE_MAX_PATH];
		FileUtil::replaceExtension(name, "wxx", hdPath);

		vpFile file(VPATH_GAME, hdPath, false);
		if (!file)
			return false;

		unsigned int size = file.size();
		s_buffer.resize(size);
		file.read(s_buffer.data(), size);
		file.close();

		const u8* data = s_buffer.data();
		const s32 entryCount = *((s32*)data); data += 4;
		assert(entryCount > 0);

		// Verify that the number of cells is correct.
		if (entryCount != (s32)s_cellOffsets.size())
		{
			return false;
		}

		hdWax->entryCount = entryCount;
		hdWax->cells = (HdWaxCell*)malloc(sizeof(HdWaxCell) * entryCount);
		assert(hdWax->cells);
		// Image data size x entryCount.
		for (s32 i = 0; i < entryCount; i++)
		{
			hdWax->cells[i].pixelCount = *((u32*)data); data += 4;
			hdWax->cells[i].id = i;

			// Verify that the sizes match expectations.
			WaxCell* cell = (WaxCell*)((u8*)wax + s_cellOffsets[i]);
			s32 targetPixelCount = 4 * cell->sizeX * cell->sizeY;
			if (targetPixelCount != hdWax->cells[i].pixelCount)
			{
				free(hdWax->cells);
				hdWax->cells = nullptr;
				return false;
			}
		}
		// Image data.
		for (s32 i = 0; i < entryCount; i++)
		{
			const u32 size = sizeof(u32) * hdWax->cells[i].pixelCount;
			hdWax->cells[i].data = (u32*)malloc(size);
			memcpy(hdWax->cells[i].data, data, size);
			data += size;
		}
		return true;
	}

	JediWax* getWax(const char* name, AssetPool pool)
	{
		SpriteMap::iterator iSprite = s_sprites[pool].find(name);
		if (iSprite != s_sprites[pool].end())
		{
			return iSprite->second;
		}

		// It doesn't exist yet, try to load the frame.
		vpFile file(VPATH_GAME, name, false);
		if (!file)
			return nullptr;
		unsigned int len = file.size();
		s_buffer.resize(len);
		file.read(s_buffer.data(), len);
		file.close();

		u8* data = s_buffer.data();
		Wax* srcWax = (Wax*)data;
		
		// every animation is filled out until the end, so no animations = no wax.
		if (!srcWax->animOffsets[0])
		{
			return nullptr;
		}
		s_cellOffsets.clear();

		// First determine the size to allocate (note that this will overallocate a bit because cells are shared).
		u32 cellId = 0;
		u32 sizeToAlloc = sizeof(JediWax) + (u32)s_buffer.size();
		const s32* animOffset = srcWax->animOffsets;
		for (s32 animIdx = 0; animIdx < 32 && animOffset[animIdx]; animIdx++)
		{
			const WaxAnim* anim = (WaxAnim*)(data + animOffset[animIdx]);
			const s32* viewOffsets = anim->viewOffsets;
			for (s32 v = 0; v < 32; v++)
			{
				const WaxView* view = (WaxView*)(data + viewOffsets[v]);
				const s32* frameOffset = view->frameOffsets;
				for (s32 f = 0; f < 32 && frameOffset[f]; f++)
				{
					WaxFrame* frame = (WaxFrame*)(data + frameOffset[f]);
					WaxCell* cell = frame->cellOffset ? (WaxCell*)(data + frame->cellOffset) : nullptr;
					bool unique = cell && isUniqueCell(frame->cellOffset);
					if (unique && cell->compressed == 0)
					{
						sizeToAlloc += cell->sizeX * sizeof(u32);
					}
					if (unique)
					{
						cell->id = cellId;
						cellId++;
					}
				}
			}
		}

		// Allocate and copy the data (this is a "copy in place" format... mostly.
		JediWax* asset = (JediWax*)malloc(sizeToAlloc);
		Wax* dstWax = asset;
		memcpy(dstWax, srcWax, s_buffer.size());

		// Loop through animation list until we reach 32 (maximum count) or a null animation.
		// This means that animations are contiguous.
		fixed16_16 scaledWidth, scaledHeight;

		u32 cellOffsetPtr = 0;
		fixed16_16 worldWidth, worldHeight;

		s32 animIdx = 0;
		for (; animIdx < 32 && animOffset[animIdx]; animIdx++)
		{
			WaxAnim* dstAnim = (WaxAnim*)((u8*)asset + animOffset[animIdx]);

			if (animIdx == 0)
			{
				scaledWidth  = div16(SPRITE_SCALE_FIXED, dstAnim->worldWidth);
				scaledHeight = div16(SPRITE_SCALE_FIXED, dstAnim->worldHeight);

				worldWidth  = dstAnim->worldWidth;
				worldHeight = dstAnim->worldHeight;

				dstWax->xScale = dstAnim->worldWidth;
				dstWax->yScale = dstAnim->worldHeight;
			}
			else
			{
				dstAnim->worldWidth = worldWidth;
				dstAnim->worldHeight = worldHeight;
			}

			const s32* viewOffsets = dstAnim->viewOffsets;
			for (s32 v = 0; v < 32; v++)
			{
				const WaxView* dstView = (WaxView*)((u8*)asset + viewOffsets[v]);
				const s32* frameOffset = dstView->frameOffsets;
				s32 frameCount = 0;
				for (s32 f = 0; f < 32 && frameOffset[f]; f++, frameCount++)
				{
					const WaxFrame* srcFrame = (WaxFrame*)(data + frameOffset[f]);
					WaxFrame* dstFrame = (WaxFrame*)((u8*)asset + frameOffset[f]);

					// Some frames are shared between animations, so we need to read from the source, unmodified data.
					dstFrame->offsetX = round16(mul16(dstAnim->worldWidth,  intToFixed16(srcFrame->offsetX)));
					dstFrame->offsetY = round16(mul16(dstAnim->worldHeight, intToFixed16(srcFrame->offsetY)));

					WaxCell* dstCell = dstFrame->cellOffset ? (WaxCell*)((u8*)asset + dstFrame->cellOffset) : nullptr;
					if (dstCell)
					{
						dstFrame->widthWS  = div16(intToFixed16(dstCell->sizeX), scaledWidth);
						dstFrame->heightWS = div16(intToFixed16(dstCell->sizeY), scaledHeight);
						assert(dstFrame->widthWS != 0 && dstFrame->heightWS != 0);

						if (dstCell->columnOffset == 0)
						{
							if (dstCell->compressed == 1)
							{
								// Update the column offset, it starts right after the cell data.
								dstCell->columnOffset = dstFrame->cellOffset + sizeof(WaxCell);
							}
							else
							{
								u32* columns = (u32*)((u8*)asset + s_buffer.size() + cellOffsetPtr);
								cellOffsetPtr += dstCell->sizeX * sizeof(u32);

								// Local pointer.
								dstCell->columnOffset = u32((u8*)columns - (u8*)asset);
								// Calculate column offsets.
								for (s32 c = 0; c < dstCell->sizeX; c++)
								{
									columns[c] = dstCell->sizeY * c;
								}
							}
						}

						dstFrame->offsetX = div16(-intToFixed16(dstFrame->offsetX), SPRITE_SCALE_FIXED);
						const s32 adjOffsetY = mul16(intToFixed16(dstCell->sizeY), dstAnim->worldHeight) + intToFixed16(dstFrame->offsetY);
						dstFrame->offsetY = div16(adjOffsetY, SPRITE_SCALE_FIXED);
					}
				}
				if (v == 0)
				{
					dstAnim->frameCount = frameCount;
					assert(frameCount);
				}
			}
		}
		asset->animCount = animIdx;
		asset->pool = u32(pool);

		s_sprites[pool][name] = asset;
		s_spriteList[pool].push_back(asset);
		s_spriteNames[pool].push_back(name);

		bool canUseHdAsset = true;
		if (pool == POOL_LEVEL && !TFE_Settings::isHdAssetValid(name, HD_ASSET_TYPE_WAX))
		{
			canUseHdAsset = false;
		}

		// HD Version
		if (canUseHdAsset)
		{
			HdWax* hdWax = (HdWax*)malloc(sizeof(HdWax));
			if (loadWaxHd(name, asset, pool, hdWax))
			{
				s_hdSpriteList[pool].push_back(hdWax);
				s_hdSprites[pool][asset] = hdWax;
			}
			else
			{
				free(hdWax);
			}
		}

		return asset;
	}
		
	const HdWax* getHdWaxData(const void* srcData)
	{
		const s32* srcData32 = (s32*)srcData;
		const u32 pool = srcData32[PoolDataOffset];

		HdSpriteMap::const_iterator iSprite = s_hdSprites[pool].find(srcData);
		if (iSprite != s_hdSprites[pool].end())
		{
			return iSprite->second;
		}
		return nullptr;
	}

	JediWax* loadWaxFromMemory(const u8* data, size_t size, bool transformOffsets)
	{
		const Wax* srcWax = (Wax*)data;

		// every animation is filled out until the end, so no animations = no wax.
		if (!srcWax->animOffsets[0])
		{
			return nullptr;
		}
		s_cellOffsets.clear();

		// First determine the size to allocate (note that this will overallocate a bit because cells are shared).
		u32 sizeToAlloc = sizeof(JediWax) + (u32)size;
		const s32* animOffset = srcWax->animOffsets;
		for (s32 animIdx = 0; animIdx < 32 && animOffset[animIdx]; animIdx++)
		{
			WaxAnim* anim = (WaxAnim*)(data + animOffset[animIdx]);
			const s32* viewOffsets = anim->viewOffsets;
			for (s32 v = 0; v < 32; v++)
			{
				const WaxView* view = (WaxView*)(data + viewOffsets[v]);
				const s32* frameOffset = view->frameOffsets;
				for (s32 f = 0; f < 32 && frameOffset[f]; f++)
				{
					const WaxFrame* frame = (WaxFrame*)(data + frameOffset[f]);
					const WaxCell* cell = frame->cellOffset ? (WaxCell*)(data + frame->cellOffset) : nullptr;
					if (cell && cell->compressed == 0 && isUniqueCell(frame->cellOffset))
					{
						sizeToAlloc += cell->sizeX * sizeof(u32);
					}
				}
			}
		}

		// Allocate and copy the data (this is a "copy in place" format... mostly.
		JediWax* asset = (JediWax*)malloc(sizeToAlloc);
		Wax* dstWax = asset;
		memcpy(dstWax, srcWax, size);

		// Loop through animation list until we reach 32 (maximum count) or a null animation.
		// This means that animations are contiguous.
		fixed16_16 scaledWidth, scaledHeight;

		u32 cellOffsetPtr = 0;
		fixed16_16 worldWidth, worldHeight;

		s32 animIdx = 0;
		for (; animIdx < 32 && animOffset[animIdx]; animIdx++)
		{
			WaxAnim* dstAnim = (WaxAnim*)((u8*)asset + animOffset[animIdx]);

			if (animIdx == 0)
			{
				scaledWidth  = div16(SPRITE_SCALE_FIXED, dstAnim->worldWidth);
				scaledHeight = div16(SPRITE_SCALE_FIXED, dstAnim->worldHeight);

				worldWidth  = dstAnim->worldWidth;
				worldHeight = dstAnim->worldHeight;

				dstWax->xScale = dstAnim->worldWidth;
				dstWax->yScale = dstAnim->worldHeight;
			}
			else
			{
				dstAnim->worldWidth  = worldWidth;
				dstAnim->worldHeight = worldHeight;
			}

			const s32* viewOffsets = dstAnim->viewOffsets;
			for (s32 v = 0; v < 32; v++)
			{
				const WaxView* dstView = (WaxView*)((u8*)asset + viewOffsets[v]);
				const s32* frameOffset = dstView->frameOffsets;
				s32 frameCount = 0;
				for (s32 f = 0; f < 32 && frameOffset[f]; f++, frameCount++)
				{
					const WaxFrame* srcFrame = (WaxFrame*)(data + frameOffset[f]);
					WaxFrame* dstFrame = (WaxFrame*)((u8*)asset + frameOffset[f]);

					// Some frames are shared between animations, so we need to read from the source, unmodified data.
					if (transformOffsets)
					{
						dstFrame->offsetX = round16(mul16(dstAnim->worldWidth, intToFixed16(srcFrame->offsetX)));
						dstFrame->offsetY = round16(mul16(dstAnim->worldHeight, intToFixed16(srcFrame->offsetY)));
					}
					else
					{
						dstFrame->offsetX = srcFrame->offsetX;
						dstFrame->offsetY = srcFrame->offsetY;
					}

					WaxCell* dstCell = dstFrame->cellOffset ? (WaxCell*)((u8*)asset + dstFrame->cellOffset) : nullptr;
					if (dstCell)
					{
						dstFrame->widthWS = div16(intToFixed16(dstCell->sizeX), scaledWidth);
						dstFrame->heightWS = div16(intToFixed16(dstCell->sizeY), scaledHeight);
						assert(dstFrame->widthWS != 0 && dstFrame->heightWS != 0);

						if (dstCell->columnOffset == 0)
						{
							if (dstCell->compressed == 1)
							{
								// Update the column offset, it starts right after the cell data.
								dstCell->columnOffset = dstFrame->cellOffset + sizeof(WaxCell);
							}
							else
							{
								u32* columns = (u32*)((u8*)asset + size + cellOffsetPtr);
								cellOffsetPtr += dstCell->sizeX * sizeof(u32);

								// Local pointer.
								dstCell->columnOffset = u32((u8*)columns - (u8*)asset);
								// Calculate column offsets.
								for (s32 c = 0; c < dstCell->sizeX; c++)
								{
									columns[c] = dstCell->sizeY * c;
								}
							}
						}

						if (transformOffsets)
						{
							dstFrame->offsetX = div16(-intToFixed16(dstFrame->offsetX), SPRITE_SCALE_FIXED);
							const s32 adjOffsetY = mul16(intToFixed16(dstCell->sizeY), dstAnim->worldHeight) + intToFixed16(dstFrame->offsetY);
							dstFrame->offsetY = div16(adjOffsetY, SPRITE_SCALE_FIXED);
						}
					}
				}
				if (v == 0)
				{
					dstAnim->frameCount = frameCount;
					assert(frameCount);
				}
				else
				{
					assert(frameCount == dstAnim->frameCount);
				}
			}
		}
		asset->animCount = animIdx;
		return asset;
	}
				
	const std::vector<JediWax*>& getWaxList(AssetPool pool)
	{
		return s_spriteList[pool];
	}

	const std::vector<JediFrame*>& getFrameList(AssetPool pool)
	{
		return s_frameList[pool];
	}

	void freePool(AssetPool pool)
	{
		const size_t frameCount = s_frameList[pool].size();
		JediFrame** frameList = s_frameList[pool].data();
		for (size_t i = 0; i < frameCount; i++)
		{
			free(frameList[i]);
		}
		s_frames[pool].clear();
		s_frameList[pool].clear();
		s_frameNames[pool].clear();

		const size_t waxCount = s_spriteList[pool].size();
		JediWax** waxList = s_spriteList[pool].data();
		for (size_t i = 0; i < waxCount; i++)
		{
			free(waxList[i]);
		}
		s_sprites[pool].clear();
		s_spriteList[pool].clear();
		s_spriteNames[pool].clear();

		const size_t hdWaxCount = s_hdSpriteList[pool].size();
		HdWax** hdWaxList = s_hdSpriteList[pool].data();
		for (size_t i = 0; i < hdWaxCount; i++)
		{
			for (s32 e = 0; e < hdWaxList[i]->entryCount; e++)
			{
				free(hdWaxList[i]->cells[e].data);
			}
			free(hdWaxList[i]->cells);
			free(hdWaxList[i]);
		}
		s_hdSpriteList[pool].clear();
		s_hdSprites[pool].clear();
	}

	void freeAll()
	{
		for (s32 p = 0; p < POOL_COUNT; p++)
		{
			freePool(AssetPool(p));
		}
	}

	void freeLevelData()
	{
		freePool(POOL_LEVEL);
	}

	bool getWaxIndex(JediWax* wax, s32* index, AssetPool* pool)
	{
		for (s32 p = 0; p < POOL_COUNT; p++)
		{
			const size_t waxCount = s_spriteList[p].size();
			JediWax** waxList = s_spriteList[p].data();
			for (size_t i = 0; i < waxCount; i++)
			{
				if (waxList[i] == wax)
				{
					*index = s32(i);
					*pool = AssetPool(p);
					return true;
				}
			}
		}
		return false;
	}

	JediWax* getWaxByIndex(s32 index, AssetPool pool)
	{
		if (pool >= POOL_COUNT || index >= (s32)s_spriteList[pool].size())
		{
			return nullptr;
		}
		return s_spriteList[pool][index];
	}

	bool getFrameIndex(JediFrame* frame, s32* index, AssetPool* pool)
	{
		for (s32 p = 0; p < POOL_COUNT; p++)
		{
			const size_t frameCount = s_frameList[p].size();
			JediFrame** frameList = s_frameList[p].data();
			for (size_t i = 0; i < frameCount; i++)
			{
				if (frameList[i] == frame)
				{
					*index = s32(i);
					*pool = AssetPool(p);
					return true;
				}
			}
		}
		return false;
	}

	JediFrame* getFrameByIndex(s32 index, AssetPool pool)
	{
		if (pool >= POOL_COUNT || index >= (s32)s_frameList[pool].size())
		{
			return nullptr;
		}
		return s_frameList[pool][index];
	}
}

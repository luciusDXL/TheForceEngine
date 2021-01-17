#include "voxelAsset.h"
#include <TFE_System/system.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Asset/paletteAsset.h>
#include <TFE_Archive/archive.h>
#include <TFE_System/parser.h>

// TODO: dependency on JediRenderer, this should be refactored...
#include <TFE_JediRenderer/fixedPoint.h>
#include <TFE_JediRenderer/rlimits.h>

#include <assert.h>
#include <map>
#include <algorithm>

using namespace TFE_JediRenderer;

namespace TFE_VoxelModel
{
	enum CapMasks
	{
		CAP_MASK_NEG_Y = (1 << 4),
		CAP_MASK_POS_Y = (1 << 5),
	};

	typedef std::map<std::string, VoxelModel*> ModelMap;
	static const u16 c_emptyVoxel = 0xffff;

	static ModelMap s_models;
	static std::vector<char> s_buffer;
	static const char* c_defaultArchive = "Mods/Voxels.zip";
	static std::vector<std::vector<u16>> s_tempColumnBuffers;
	static u32 s_voxelPal[256];
	static u32 s_paletteRemap[256];
	static bool s_yUp = true;

	bool loadMagicaVoxelModel(VoxelModel* model, const char* name);
	void remapPalette();
		
	void rleCompressColumn(s32 height, const u16* src, VoxelColumn* dst, bool remapColors)
	{
		// Maximum height.
		u8 columnOut[1024];
		u8 subColumnMask[256] = { 0 };
		u32 subColumnCount = 0;
		u32 columnSize = 0;
		bool columnHasData = false;
		for (s32 y = 0; y < height;)
		{
			// Check for an empty run.
			u32 emptyCount = 0;
			while (((src[y]&0xff) == 0 || src[y] == c_emptyVoxel) && y < height && emptyCount < 127)
			{
				emptyCount++;
				y++;
			}
			if (emptyCount)
			{
				columnOut[columnSize++] = 0x80 | emptyCount;
			}

			// Check for a non-empty run
			u32 nonEmptyCount = 0;
			u32 countIndex = 0;
			s32 yStart = 0;
			while ((src[y]&0xff) != 0 && src[y] != c_emptyVoxel && y < height && nonEmptyCount < 127)
			{
				if (nonEmptyCount == 0)
				{
					// prime the count.
					countIndex = columnSize;
					yStart = y;
					columnOut[columnSize++] = 0x00;
					columnHasData = true;
				}
				columnOut[columnSize++] = remapColors ? s_paletteRemap[src[y]&0xff] : (src[y]&0xff);
				nonEmptyCount++;
				y++;
			}
			if (nonEmptyCount)
			{
				columnOut[countIndex] = nonEmptyCount;

				// Add the "cap" mask - this tells us if the top and bottom caps should be rendered (2-bits per run).
				u8 capMask = 0;
				if ((src[yStart]>>8) & CAP_MASK_NEG_Y)
				{
					capMask |= NEG_Y;
				}
				if ((src[yStart + nonEmptyCount - 1]>>8) & CAP_MASK_POS_Y)
				{
					capMask |= POS_Y;
				}
				// Store as 2-bits per sub-column.
				subColumnMask[subColumnCount>>2] |= (capMask << ((subColumnCount & 3) * 2));
				subColumnCount++;
			}
						
			// Then add the per-voxel face masks (4-bits per voxel).
			for (s32 i = 0; i < nonEmptyCount; i+=2)
			{
				const u8 mask0 = (src[yStart + i + 0] >> 8);
				const u8 mask1 = (i+1 < nonEmptyCount) ? (src[yStart + i + 1] >> 8) : 0;
				columnOut[columnSize++] = mask0 | (mask1 << 4);
			}
		}

		u32 subColumnMaskBytes = (subColumnCount + 3) >> 2;
		u32 allocSize = subColumnMaskBytes + columnSize;

		dst->dataSize = columnHasData ? (columnSize | (subColumnCount << 16)) : 0;
		dst->data = nullptr;
		if (columnSize && columnHasData)
		{
			dst->data = (u8*)malloc(allocSize);
			memcpy(dst->data, columnOut, columnSize);
			memcpy(dst->data + columnSize, subColumnMask, subColumnMaskBytes);
		}
	}

	void processColumns(VoxelModel* model, bool remapColor)
	{
		for (s32 z = 0; z < model->depth; z++)
		{
			for (s32 x = 0; x < model->width; x++)
			{
				const s32 index = z*model->width + x;
				u16* columnData = s_tempColumnBuffers[index].data();

				// check the neighbors.
				u16* left = nullptr;
				u16* right = nullptr;
				u16* top = nullptr;
				u16* bot = nullptr;
				if (x > 0)
				{
					left = s_tempColumnBuffers[index - 1].data();
				}
				if (x < model->width - 1)
				{
					right = s_tempColumnBuffers[index + 1].data();
				}
				if (z > 0)
				{
					top = s_tempColumnBuffers[index - model->width].data();
				}
				if (z < model->depth - 1)
				{
					bot = s_tempColumnBuffers[index + model->width].data();
				}

				for (s32 y = 0; y < model->height; y++)
				{
					const u8 value = columnData[y] & 0xff;
					if (!value) { continue; }
					
					u32 mask = 0;
					if (!left || (left && (left[y] & 0xff) == 0))    { mask |= NEG_X; }
					if (!right || (right && (right[y] & 0xff) == 0)) { mask |= POS_X; }
					if (!top || (top && (top[y] & 0xff) == 0))       { mask |= NEG_Z; }
					if (!bot || (bot && (bot[y] & 0xff) == 0))       { mask |= POS_Z; }

					const s32 last = model->height - 1;
					if (y == 0 || (y > 0 && (columnData[y-1] & 0xff) == 0))       { mask |= CAP_MASK_NEG_Y; }
					if (y == last || (y < last && (columnData[y+1] & 0xff) == 0)) { mask |= CAP_MASK_POS_Y; }

					// Remove interior voxels.
					if (mask == 0 && y > 0 && y < model->height - 1 && (columnData[y - 1] & 0xff) != 0 && (columnData[y + 1] & 0xff) != 0)
					{
						// solid interior voxel but should be skipped.
						columnData[y] = c_emptyVoxel;
					}
					else
					{
						columnData[y] |= (mask << 8);
					}
				}

				rleCompressColumn(model->height, columnData, &model->columns[index], remapColor);
			}
		}
	}

	// Create a single 1x64x1 voxel model.
	// This is for testing rendering for a single column.
	VoxelModel* createTestVoxel()
	{
		VoxelModel* model = new VoxelModel;
		model->width = 16;
		model->depth = 16;
		model->height = 32;

		model->worldWidth  = f32(model->width) * 0.1f;
		model->worldHeight = f32(model->height) * 0.1f;
		model->worldDepth  = f32(model->depth) * 0.1f;
		model->radius = std::max(model->worldWidth, model->worldDepth);

		const s32 columnCount = model->width * model->height;
		model->columns = (VoxelColumn*)malloc(columnCount * sizeof(VoxelColumn));
		memset(model->columns, 0, columnCount * sizeof(VoxelColumn));

		// Temporary column data.
		s_tempColumnBuffers.resize(columnCount);
		for (u32 i = 0; i < columnCount; i++)
		{
			s_tempColumnBuffers[i].resize(model->height);
			memset(s_tempColumnBuffers[i].data(), 0, model->height * 2);
		}

		u8 testData1[32];
		u8 testData2[32];
		for (s32 i = 0; i < 32; i++)
		{
			testData1[i] = i;
		}
		for (s32 i = 0; i < 32; i++)
		{
			testData2[i] = i;
			if (i >= 14 && i < 18) { testData2[i] = 0; }
		}

		for (s32 x = 0; x < model->width; x++)
		{
			for (s32 z = 0; z < model->depth; z++)
			{
				const u32 index = z*model->width + x;

				u16* out = s_tempColumnBuffers[index].data();
				if (x == 0 || x == model->width-1 || z == 0 || z == model->depth-1)
				{
					for (s32 i = 0; i < model->height; i++) { out[i] = (x >= 6 && x < 9) ? testData2[i] : testData1[i]; }
				}
			}
		}

		processColumns(model, false);

		s_models["test"] = model;
		return model;
	}

	VoxelModel* get(const char* name, bool yUp)
	{
		char testName[TFE_MAX_PATH];
		sprintf(testName, "%s_%s", yUp ? "y" : "z", name);
				
		ModelMap::iterator iModel = s_models.find(testName);
		if (iModel != s_models.end())
		{
			return iModel->second;
		}

		if (strcasecmp(name, "test") == 0)
		{
			return createTestVoxel();
		}

		// It doesn't exist yet, try to load the font.
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultArchive, ARCHIVE_ZIP, name, s_buffer))
		{
			return nullptr;
		}

		VoxelModel* model = new VoxelModel;
		s_yUp = yUp;

		////////////////////////////////////////////////////////////////
		// Load and parse the model.
		////////////////////////////////////////////////////////////////
		model->width = 0;
		model->height = 0;
		model->depth = 0;
		s_tempColumnBuffers.clear();

		if (strstr(name, ".vox"))
		{
			loadMagicaVoxelModel(model, name);
		}
		
		////////////////////////////////////////////////////////////////
		// Post process the model.
		////////////////////////////////////////////////////////////////
		if (s_tempColumnBuffers.empty() || !model->width || !model->height || !model->depth)
		{
			free(model);
			return nullptr;
		}

		model->worldWidth  = f32(model->width) * 0.1f;
		model->worldHeight = f32(model->height) * 0.1f;
		model->worldDepth  = f32(model->depth) * 0.1f;
		model->radius = std::max(model->worldWidth, model->worldDepth);

		remapPalette();

		// Compress columns.
		processColumns(model, true);

		s_models[testName] = model;
		return model;
	}

	void freeAll()
	{
		ModelMap::iterator iModel = s_models.begin();
		for (; iModel != s_models.end(); ++iModel)
		{
			delete iModel->second;
		}
		s_models.clear();
	}

	struct VoxHeader
	{
		char id[4];
		s32 version;
	};

	struct VoxChunk
	{
		char id[4];
		s32 chunkSize;
		s32 childChunkSize;
	};

	static s32 s_modelCount = 1;
	static VoxelModel* s_model;
	
	bool isChunkId(const char* id, const char* testStr)
	{
		if (id[0] == testStr[0] && id[1] == testStr[1] && id[2] == testStr[2] && id[3] == testStr[3])
		{
			return true;
		}
		return false;
	}
		
	void parseChunkData(const VoxChunk* chunk, const u8* data)
	{
		if (isChunkId(chunk->id, "PACK"))
		{
			s_modelCount = *((s32*)data);
		}
		else if(isChunkId(chunk->id, "SIZE"))
		{
			const s32* srcData = (s32*)data;
			s_model->width  = srcData[0];
			s_model->height = srcData[s_yUp ? 1 : 2];
			s_model->depth  = srcData[s_yUp ? 2 : 1];

			const u32 columnCount = s_model->width * s_model->depth;

			s_model->columns = (VoxelColumn*)malloc(sizeof(VoxelColumn) * columnCount);
			memset(s_model->columns, 0, sizeof(VoxelColumn) * columnCount);

			s_tempColumnBuffers.resize(columnCount);
			for (u32 i = 0; i < columnCount; i++)
			{
				s_tempColumnBuffers[i].resize(s_model->height);
				memset(s_tempColumnBuffers[i].data(), 0, s_model->height * 2);
			}
		}
		else if (isChunkId(chunk->id, "XYZI"))
		{
			const u32* srcData = (u32*)data;
			u32 count = *srcData;
			srcData++;
			for (u32 i = 0; i < count; i++, srcData++)
			{
				const u32 value = *srcData;

				u8 x, y, z, index;
				x = value & 0xff;
				y = (value >> 8) & 0xff;
				z = (value >> 16) & 0xff;
				if (!s_yUp) { std::swap(y, z); }
				index = (value >> 24) & 0xff;

				u16* outBuffer = s_tempColumnBuffers[z*s_model->width + x].data();
				outBuffer[y] = index;
			}
		}
		else if (isChunkId(chunk->id, "RGBA"))
		{
			s_voxelPal[0] = 0;

			const u32* srcData = (u32*)data;
			for (s32 i = 0; i < 255; i++)
			{
				s_voxelPal[i + 1] = srcData[i];
			}
		}
		// Other chunks are ignored for now.
	}

	const u8* parseChunk(const u8* dataPtr)
	{
		const VoxChunk* chunk = (VoxChunk*)dataPtr;
		dataPtr += sizeof(VoxChunk);

		parseChunkData(chunk, dataPtr);
		dataPtr += chunk->chunkSize;

		// Handle children.
		const u8* end = chunk->childChunkSize ? (dataPtr + chunk->childChunkSize) : dataPtr;
		while (dataPtr < end)
		{
			dataPtr = parseChunk(dataPtr);
		}
		dataPtr = end;

		return dataPtr;
	}

	void remapPalette()
	{
		const u32* gamePal = TFE_Palette::get256("SECBASE.PAL")->colors;

		s_paletteRemap[0] = 0;
		for (u32 i = 1; i < 256; i++)
		{
			s_paletteRemap[i] = i;
			bool matchFound = false;
			for (u32 j = 1; j < 256; j++)
			{
				if (s_voxelPal[i] == gamePal[j])
				{
					s_paletteRemap[i] = j;
					matchFound = true;
					break;
				}
			}
			
			if (!matchFound)
			{
				TFE_System::logWrite(LOG_ERROR, "Voxel", "Voxel index %d has no matching color in the palette.", i);
			}
		}
	}

	bool loadMagicaVoxelModel(VoxelModel* model, const char* name)
	{
		if (s_buffer.empty()) { return false; }
		s_model = model;
		const size_t len = s_buffer.size();

		const u8* buffer = (u8*)s_buffer.data();
		const VoxHeader* header = (VoxHeader*)buffer;
		buffer += sizeof(VoxHeader);

		const VoxChunk* main = (VoxChunk*)buffer;
		buffer += sizeof(VoxChunk);

		s_modelCount = 1;

		const u8* mainData = buffer;
		const u8* dataPtr = mainData + main->chunkSize;
		const u8* end = dataPtr + main->childChunkSize;
		while (dataPtr < end)
		{
			dataPtr = parseChunk(dataPtr);
		}

		return true;
	}
};

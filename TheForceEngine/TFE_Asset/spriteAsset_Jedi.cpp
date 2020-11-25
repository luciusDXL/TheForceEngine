#include "spriteAsset_Jedi.h"
#include <TFE_System/system.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Archive/archive.h>
// TODO: dependency on JediRenderer, this should be refactored...
#include <TFE_JediRenderer/fixedPoint.h>
#include <TFE_JediRenderer/rlimits.h>
//
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_JediRenderer;

namespace TFE_Sprite_Jedi
{
	typedef std::map<std::string, JediFrame*> FrameMap;
	typedef std::map<std::string, JediWax*> SpriteMap;

	static FrameMap  s_frames;
	static SpriteMap s_sprites;
	static std::vector<u8> s_buffer;
	static const char* c_defaultGob = "SPRITES.GOB";
		
	JediFrame* getFrame(const char* name)
	{
		FrameMap::iterator iFrame = s_frames.find(name);
		if (iFrame != s_frames.end())
		{
			return iFrame->second;
		}

		// It doesn't exist yet, try to load the frame.
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, name, s_buffer))
		{
			return nullptr;
		}

		const u8* data = s_buffer.data();

		// Determine ahead of time how much we need to allocate.
		const WaxFrame* base_frame = (WaxFrame*)data;
		const WaxCell* base_cell = WAX_CellPtr(data, base_frame);
		const u32 columnSize = base_cell->sizeX * sizeof(u32);

		// This is a "load in place" format in the original code.
		// We are going to allocate new memory and copy the data.
		u8* assetPtr = (u8*)malloc(s_buffer.size() + columnSize + sizeof(JediFrame));
		JediFrame* asset = (JediFrame*)assetPtr;
		
		asset->basePtr = assetPtr + sizeof(JediFrame);
		asset->frame = (WaxFrame*)asset->basePtr;
		memcpy(asset->basePtr, data, s_buffer.size());

		WaxFrame* frame = asset->frame;
		WaxCell* cell = WAX_CellPtr(asset->basePtr, frame);

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
			u32* columns = (u32*)(assetPtr + sizeof(JediFrame) + s_buffer.size());
			// Local pointer.
			cell->columnOffset = u32((u8*)columns - (u8*)asset->basePtr);
			// Calculate column offsets.
			for (s32 c = 0; c < cell->sizeX; c++)
			{
				columns[c] = cell->sizeY * c;
			}
		}
		
		s_frames[name] = asset;
		return asset;
	}

	JediWax* getWax(const char* name)
	{
		SpriteMap::iterator iSprite = s_sprites.find(name);
		if (iSprite != s_sprites.end())
		{
			return iSprite->second;
		}

		// It doesn't exist yet, try to load the frame.
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, name, s_buffer))
		{
			return nullptr;
		}

		const u8* data = s_buffer.data();

		// First determine the size to allocate...
		// ...

		return nullptr;
	}

	void freeAll()
	{
		FrameMap::iterator iFrame = s_frames.begin();
		for (; iFrame != s_frames.end(); ++iFrame)
		{
			free(iFrame->second);
		}
		s_frames.clear();

		SpriteMap::iterator iSprite = s_sprites.begin();
		for (; iSprite != s_sprites.end(); ++iSprite)
		{
			free(iSprite->second);
		}
		s_sprites.clear();
	}
}

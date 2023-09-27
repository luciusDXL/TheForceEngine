#include "editorFrame.h"
#include "editor.h"
#include "editorColormap.h"
#include <TFE_DarkForces/mission.h>
#include <TFE_System/system.h>
#include <TFE_Archive/archive.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Renderer/rcommon.h>
#include <algorithm>
#include <vector>
#include <map>

namespace TFE_Editor
{
	typedef std::map<std::string, s32> FrameMap;
	typedef std::vector<EditorFrame> FrameList;
		
	static FrameMap s_framesLoaded;
	static FrameList s_frameList;
	
	void freeFrame(const char* name)
	{
		const size_t count = s_frameList.size();
		const EditorFrame* frame = s_frameList.data();
		for (size_t i = 0; i < count; i++, frame++)
		{
			if (strcasecmp(frame->name, name) == 0)
			{
				TFE_RenderBackend::freeTexture(frame->texGpu);
				s_frameList.erase(s_frameList.begin() + i);
				break;
			}
		}
		s_framesLoaded.clear();
		const s32 newCount = (s32)s_frameList.size();
		for (s32 i = 0; i < newCount; i++)
		{
			s_framesLoaded[s_frameList[i].name] = i;
		}
	}

	void freeCachedFrames()
	{
		const size_t count = s_frameList.size();
		const EditorFrame* frame = s_frameList.data();
		for (size_t i = 0; i < count; i++, frame++)
		{
			TFE_RenderBackend::freeTexture(frame->texGpu);
		}
		s_framesLoaded.clear();
		s_frameList.clear();
	}

	s32 findFrame(const char* name)
	{
		FrameMap::iterator iFrame = s_framesLoaded.find(name);
		if (iFrame != s_framesLoaded.end())
		{
			return iFrame->second;
		}
		return -1;
	}

	s32 allocateFrame(const char* name)
	{
		s32 index = (s32)s_frameList.size();
		s_frameList.push_back({});
		s_framesLoaded[name] = index;
		return index;
	}
				
	bool loadEditorFrame(FrameSourceType type, Archive* archive, const char* filename, const u32* palette, s32 palIndex, EditorFrame* frame)
	{
		if (!archive && !filename) { return false; }
		if (!frame) { return false; }
		buildIdentityTable();

		s32 id = findFrame(filename);
		if (id >= 0)
		{
			*frame = s_frameList[id];
			return true;
		}

		if (!archive->openFile(filename))
		{
			return false;
		}
		size_t len = archive->getFileLength();
		if (!len)
		{
			archive->closeFile();
			return false;
		}
		WorkBuffer& buffer = getWorkBuffer();
		buffer.resize(len);
		archive->readFile(buffer.data(), len);
		archive->closeFile();

		if (type == FRAME_FME)
		{
			JediFrame* frameData = TFE_Sprite_Jedi::loadFrameFromMemory(buffer.data(), len);
			if (!frameData)
			{
				return false;
			}
			
			const WaxCell* cell = WAX_CellPtr(frameData, frameData);
			WorkBuffer& buffer = getWorkBuffer();
			buffer.resize(cell->sizeX * cell->sizeY * 4);
			u32* imageBuffer = (u32*)buffer.data();

			u8* imageData = (u8*)cell + sizeof(WaxCell);
			u8* image = (cell->compressed == 1) ? imageData + (cell->sizeX * sizeof(u32)) : imageData;

			u8 columnWorkBuffer[WAX_DECOMPRESS_SIZE];
			const u32* columnOffset = (u32*)((u8*)frameData + cell->columnOffset);

			for (s32 x = 0; x < cell->sizeX; x++)
			{
				u8* column = (u8*)image + columnOffset[x];
				if (cell->compressed)
				{
					const u8* colPtr = (u8*)cell + columnOffset[x];
					sprite_decompressColumn(colPtr, columnWorkBuffer, cell->sizeY);
					column = columnWorkBuffer;
				}

				for (s32 y = 0; y < cell->sizeY; y++)
				{
					imageBuffer[y*cell->sizeX + x] = column[y] ? palette[s_remapTable[column[y]]] : 0;
				}
			}

			id = allocateFrame(filename);
			s_frameList[id].width  = cell->sizeX;
			s_frameList[id].height = cell->sizeY;
			s_frameList[id].worldWidth  = fixed16ToFloat(frameData->widthWS);
			s_frameList[id].worldHeight = fixed16ToFloat(frameData->heightWS);
			strcpy(s_frameList[id].name, filename);
			s_frameList[id].paletteIndex = palIndex;
			s_frameList[id].lightLevel = 32;
			s_frameList[id].texGpu = TFE_RenderBackend::createTexture(cell->sizeX, cell->sizeY, imageBuffer);
			free(frameData);

			*frame = s_frameList[id];
			return true;
		}

		return false;
	}

	bool loadEditorFrameLit(FrameSourceType type, Archive* archive, const char* filename, const u32* palette, s32 palIndex, const u8* colormap, s32 lightLevel, EditorFrame* texture)
	{
		s_remapTable = &colormap[lightLevel * 256];
		bool result = loadEditorFrame(type, archive, filename, palette, palIndex, texture);
		if (result)
		{
			texture->lightLevel = lightLevel;
		}
		s_remapTable = s_identityTable;

		return result;
	}
}
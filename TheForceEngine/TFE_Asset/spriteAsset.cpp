#include "spriteAsset.h"
#include <TFE_System/system.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Archive/archive.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

namespace TFE_Sprite
{
	#pragma pack(push)
	#pragma pack(1)
	struct WaxHeader
	{
		s32 Version;	// constant = 0x00100100
		s32 Nseqs;		// number of SEQUENCES
		s32 Nframes;	// number of FRAMES
		s32 Ncells;		// number of CELLS
		s32 Xscale;		// unused
		s32 Yscale;		// unused
		s32 XtraLight;	// unused
		s32 pad4;		// unused
		s32 WAXES[32];	// pointers to WAXES = different animations
	};

	struct Wax
	{
		s32 Wwidth;		// World Width
		s32 Wheight;	// World Height
		s32 FrameRate;	// Frames per second
		s32 Nframes;	// unused = 0
		s32 pad2;		// unused = 0
		s32 pad3;		// unused = 0
		s32 pad4;		// unused = 0
		s32 SEQS[32];	// pointers to SEQUENCES = views from different angles
	};

	struct Sequence
	{
		s32 pad1;			// unused = 0
		s32 pad2;			// unused = 0
		s32 pad3;			// unused = 0
		s32 pad4;			// unused = 0
		s32 FRAMES[32];		// pointers to FRAMES = the animation frames
	};

	struct FME_Frame
	{
		s32 InsertX;		// Insertion point, X coordinate
							// Negative values shift the cell left; Positive values shift the cell right
		s32 InsertY;		// Insertion point, Y coordinate
							// Negative values shift the cell up; Positive values shift the cell down
		s32 Flip;			// 0 = not flipped
							// 1 = flipped horizontally
		s32 Cell;			// pointer to CELL = single picture
		s32 UnitWidth;		// Unused
		s32 UnitHeight;		// Unused
		s32 pad3;			// Unused
		s32 pad4;			// Unused
	};

	struct FME_Cell
	{
		s32 SizeX;			// Size of the CELL, X value
		s32 SizeY;			// Size of the CELL, Y value
		s32 Compressed;		// 0 = not compressed
							// 1 = compressed
		s32 DataSize;		// Datasize for compressed CELL, equals length of the CELL. If not compressed, DataSize = 0
		s32 ColOffs;		// Always 0, because columns table follows just after
		s32 pad1;			// Unused
	};
	#pragma pack(pop)

	typedef std::map<std::string, Frame*> FrameMap;
	typedef std::map<std::string, Sprite*> SpriteMap;

	static FrameMap s_frames;
	static SpriteMap s_sprites;
	static std::vector<u8> s_buffer;
	static const char* c_defaultGob = "SPRITES.GOB";

	void loadCell(s32 w, s32 h, s16 compressed, s32 dataSize, const u8* srcData, u8* dstImage)
	{
		assert(srcData && dstImage);
		
		if (compressed == 0)
		{
			memcpy(dstImage, srcData, w * h);
			return;
		}

		const s32* col = (s32*)srcData;
		for (s32 x = 0; x < w; x++)
		{
			const s32 offs = col[x] - sizeof(FME_Cell);
			if (offs < 0 || offs >= dataSize) { break; }

			const u8 *colData = &srcData[offs];
			for (s32 y = 0, b = 0; y < h; )
			{
				if (colData[b] <= 128)
				{
					const u8 n = colData[b]; b++;
					for (s32 ii = 0; ii < n; ii++, y++, b++)
					{
						dstImage[x*h + y] = colData[b];
						assert(y < h);
					}
				}
				else
				{
					const u8 n = colData[b] - 128; b++;
					for (s32 ii = 0; ii < n; ii++, y++)
					{
						dstImage[x*h + y] = 0;
						assert(y < h);
					}
				}
			}
		}
	}

	Frame* getFrame(const char* name)
	{
		FrameMap::iterator iFrame = s_frames.find(name);
		if (iFrame != s_frames.end())
		{
			return iFrame->second;
		}

		// It doesn't exist yet, try to load the texture.
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, name, s_buffer))
		{
			return nullptr;
		}

		Frame* frame = new Frame;
		u8* data = s_buffer.data();
		FME_Frame* fme_frame = (FME_Frame*)data;
		FME_Cell* fme_cell = (FME_Cell *)&data[fme_frame->Cell];

		// Load the image.
		u8* image = new u8[fme_cell->SizeX * fme_cell->SizeY];
		const u8* srcData = (u8*)fme_cell + sizeof(FME_Cell);
		loadCell(fme_cell->SizeX, fme_cell->SizeY, fme_cell->Compressed, fme_cell->DataSize, srcData, image);

		frame->InsertX = fme_frame->InsertX;
		frame->InsertY = fme_frame->InsertY;
		frame->Flip = fme_frame->Flip;
		frame->width = fme_cell->SizeX;
		frame->height = fme_cell->SizeY;
		frame->image = image;

		// area.
		frame->rect[0] = frame->InsertX;
		frame->rect[1] = -frame->height - frame->InsertY;
		frame->rect[2] = frame->rect[0] + frame->width;
		frame->rect[3] = frame->rect[1] + frame->height;
		
		s_frames[name] = frame;
		return frame;
	}

	std::vector<s32> s_tempFrames;
	std::vector<s32> s_tempCells;

	s32 addCell(s32 offset)
	{
		const size_t count = s_tempCells.size();
		const s32* data = s_tempCells.data();
		for (size_t i = 0; i < count; i++)
		{
			if (offset == data[i]) { return s32(i); }
		}
		s32 index = (s32)s_tempCells.size();
		s_tempCells.push_back(offset);
		return index;
	}

	s32 addFrame(s32 offset)
	{
		const size_t count = s_tempFrames.size();
		const s32* data = s_tempFrames.data();
		for (size_t i = 0; i < count; i++)
		{
			if (offset == data[i]) { return s32(i); }
		}
		s32 index = (s32)s_tempFrames.size();
		s_tempFrames.push_back(offset);
		return index;
	}
	
	Sprite* getSprite(const char* name)
	{
		SpriteMap::iterator iSprite = s_sprites.find(name);
		if (iSprite != s_sprites.end())
		{
			return iSprite->second;
		}

		// It doesn't exist yet, try to load the sprite.
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, name, s_buffer))
		{
			return nullptr;
		}
		s32 len = (s32)s_buffer.size();

		Sprite* sprite = new Sprite;

		const u8* data = s_buffer.data();
		const WaxHeader* header = (WaxHeader*)data;

		//load the first wax for now...
		// Get the number of animations.
		s32 animationCount = 0;
		for (animationCount = 0; header->WAXES[animationCount] != 0 && header->WAXES[animationCount] < len; ) { animationCount++; }
		sprite->animationCount = animationCount;

		s_tempFrames.clear();
		s_tempCells.clear();

		// Go through the data and compute the allocation size.
		for (s32 i = 0; i < animationCount; i++)
		{
			const Wax* wax = (Wax*)(data + header->WAXES[i]);
			for (u32 a = 0; a < 32; a++)
			{
				if (!wax->SEQS[a]) { break; }
				const Sequence* seq = (Sequence *)(data + wax->SEQS[a]);

				for (u32 f = 0; f < 32; f++)
				{
					if (!seq->FRAMES[f]) { break; }
					addFrame(seq->FRAMES[f]);
				}
			}
		}
		const s32 totalFrameCount = (s32)s_tempFrames.size();
		const s32* frameIndex = s_tempFrames.data();
		for (s32 i = 0; i < totalFrameCount; i++)
		{
			FME_Frame* frame = (FME_Frame*)(data + frameIndex[i]);
			addCell(frame->Cell);
		}

		// Now allocate memory and get offsets.
		s32 memorySize = sizeof(SpriteAnim) * animationCount;
		s32 frameOffset = memorySize;
		memorySize += sizeof(Frame) * totalFrameCount;

		s32 imageOffset = memorySize;
		const s32* cellIndex = s_tempCells.data();
		const s32 cellCount = (s32)s_tempCells.size();
		for (s32 i = 0; i < cellCount; i++)
		{
			FME_Cell* cell = (FME_Cell*)(data + cellIndex[i]);
			memorySize += cell->SizeX * cell->SizeY;
		}

		// Fixup pointers.
		sprite->memory = new u8[memorySize];
		memset(sprite->memory, 0, memorySize);

		sprite->anim = (SpriteAnim*)sprite->memory;
		sprite->frames = (Frame*)(sprite->memory + frameOffset);
		sprite->imageData = sprite->memory + imageOffset;

		// Fixup animations.
		for (s32 i = 0; i < animationCount; i++)
		{
			const Wax* wax = (Wax*)(data + header->WAXES[i]);
			SpriteAnim* anim = &sprite->anim[i];
			assert(s32((u8*)anim - (u8*)sprite->memory) < memorySize);
			anim->worldWidth = wax->Wwidth;
			anim->worldHeight = wax->Wheight;
			anim->frameRate = wax->FrameRate;
			anim->angleCount = 0;
			for (u32 a = 0; a < 32; a++)
			{
				if (!wax->SEQS[a]) { break; }
				anim->angleCount++;

				SpriteAngle* angle = &anim->angles[a];
				assert(s32((u8*)angle - (u8*)sprite->memory) < memorySize);

				const Sequence* seq = (Sequence *)(data + wax->SEQS[a]);
				u32 frameCount = 0;

				for (u32 f = 0; f < 32; f++)
				{
					if (!seq->FRAMES[f]) { break; }
					angle->frameIndex[f] = addFrame(seq->FRAMES[f]);
					frameCount++;
				}
				angle->frameCount = frameCount;
			}
		}
		// Decompress images.
		u32 imagePtr = 0;
		std::vector<u32> cellImgPtr(cellCount);
		for (s32 i = 0; i < cellCount; i++)
		{
			FME_Cell* cell = (FME_Cell*)(data + cellIndex[i]);
			const u8* srcData = (u8*)cell + sizeof(FME_Cell);
			loadCell(cell->SizeX, cell->SizeY, cell->Compressed, cell->DataSize, srcData, &sprite->imageData[imagePtr]);
			assert(s32((u8*)&sprite->imageData[imagePtr] - (u8*)sprite->memory) + cell->SizeX*cell->SizeY <= memorySize);
			
			cellImgPtr[i] = imagePtr;
			imagePtr += cell->SizeX * cell->SizeY;
		}
		// Fixup frames.
		sprite->rect[0] = 32767;
		sprite->rect[1] = 32767;
		sprite->rect[2] = -32767;
		sprite->rect[3] = -32767;
		const u32* offsets = cellImgPtr.data();
		for (s32 i = 0; i < totalFrameCount; i++)
		{
			const FME_Frame* frame = (FME_Frame*)(data + frameIndex[i]);
			const FME_Cell* cell = (FME_Cell*)(data + frame->Cell);
			assert(s32((u8*)&sprite->frames[i] - (u8*)sprite->memory) + sizeof(Frame) <= memorySize);

			sprite->frames[i].InsertX = frame->InsertX;
			sprite->frames[i].InsertY = frame->InsertY;
			sprite->frames[i].Flip = frame->Flip;
			sprite->frames[i].width = cell->SizeX;
			sprite->frames[i].height = cell->SizeY;

			// area.
			sprite->frames[i].rect[0] =  sprite->frames[i].InsertX;
			sprite->frames[i].rect[1] = -sprite->frames[i].height - sprite->frames[i].InsertY;
			sprite->frames[i].rect[2] =  sprite->frames[i].rect[0] + sprite->frames[i].width;
			sprite->frames[i].rect[3] =  sprite->frames[i].rect[1] + sprite->frames[i].height;

			sprite->rect[0] = std::min(sprite->rect[0], sprite->frames[i].rect[0]);
			sprite->rect[1] = std::min(sprite->rect[1], sprite->frames[i].rect[1]);
			sprite->rect[2] = std::max(sprite->rect[2], sprite->frames[i].rect[2]);
			sprite->rect[3] = std::max(sprite->rect[3], sprite->frames[i].rect[3]);

			s32 cellIndex = addCell(frame->Cell);
			sprite->frames[i].image = sprite->imageData + offsets[cellIndex];
		}
		// Finally determine the bounding rectangle for each view/animation.
		for (s32 i = 0; i < animationCount; i++)
		{
			SpriteAnim* anim = &sprite->anim[i];
			anim->rect[0] =  32767;
			anim->rect[1] =  32767;
			anim->rect[2] = -32767;
			anim->rect[3] = -32767;
			for (s32 a = 0; a < anim->angleCount; a++)
			{
				for (s32 f = 0; f < anim->angles[a].frameCount; f++)
				{
					Frame* frame = &sprite->frames[anim->angles[a].frameIndex[f]];
					anim->rect[0] = std::min(anim->rect[0], frame->rect[0]);
					anim->rect[1] = std::min(anim->rect[1], frame->rect[1]);
					anim->rect[2] = std::max(anim->rect[2], frame->rect[2]);
					anim->rect[3] = std::max(anim->rect[3], frame->rect[3]);
				}
			}
		}

		s_sprites[name] = sprite;
		return sprite;
	}

	void freeAll()
	{
		FrameMap::iterator iFrame = s_frames.begin();
		for (; iFrame != s_frames.end(); ++iFrame)
		{
			Frame* frame = iFrame->second;
			if (frame)
			{
				delete[] frame->image;
				delete frame;
			}
		}
		s_frames.clear();

		SpriteMap::iterator iSprite = s_sprites.begin();
		for (; iSprite != s_sprites.end(); ++iSprite)
		{
			Sprite* sprite = iSprite->second;
			if (sprite)
			{
				delete[] sprite->memory;
				delete sprite;
			}
		}
		s_sprites.clear();
	}
}

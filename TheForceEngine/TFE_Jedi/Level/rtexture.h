#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_DarkForces/time.h>

struct BM_Header
{
	s16 width;
	s16 height;
	s16 uvWidth;
	s16 uvHeight;
	u8  flags;
	u8  logSizeY;
	s16 compressed;
	s32 dataSize;
	u8  pad[12];
};

enum OpacityFlags
{
	OPACITY_TRANS = FLAG_BIT(3),
	// TFE
	OPACITY_MASK = FLAG_BIT(5) - 1,
	INDEXED = FLAG_BIT(5),
	ALWAYS_FULLBRIGHT = FLAG_BIT(6),
	ENABLE_MIP_MAPS   = FLAG_BIT(7),
};

// was BM_SubHeader
#pragma pack(push)
#pragma pack(1)
struct TextureData
{
	u16 width;		// if = 1 then multiple BM in the file
	u16 height;		// EXCEPT if SizeY also = 1, in which case
					// it is a 1x1 BM
	s16 uvWidth;	// portion of texture actually used
	s16 uvHeight;	// portion of texture actually used

	u32 dataSize;	// Data size for compressed BM
					// excluding header and columns starts table
					// If not compressed, DataSize is unused

	u8 logSizeY;	// logSizeY = log2(SizeY)
					// logSizeY = 0 for weapons
	u8 animSetup;	// Added for TFE, a flag indicating that the animated texture has already been setup.
	u16 textureId;	// Added for TFE, replaces u8 pad1[3];

	u8* image;		// Image data.
	u32* columns;	// columns will be NULL except when compressed.

	// 4 bytes
	u8 flags = 0;
	u8 compressed;  // 0 = not compressed, 1 = compressed (RLE), 2 = compressed (RLE0)
	u8 palIndex = 1;// TFE - which palette to use.
	u8 pad3;

	// TFE
	s32 animIndex = -1;
	s32 frameIdx = 0;
	void* animPtr = nullptr;

	// HD Texture replacements.
	s32 scaleFactor = 1;			// Fill with scale factor.
	u8* hdAssetData = nullptr;		// True color asset data, size = width * height * scaleFactor * scaleFactor
};
#pragma pack(pop)

// Animated texture object.
struct AnimatedTexture
{
	s32 count;					// the number of things to iterate through.
	s32 frame;					// the current iteration index.
	Tick delay;					// the delay between iterations, in ticks.
	Tick nextTick;				// the next time this iterates.
	TextureData** frameList;	// the list of things to cycle through.
	TextureData** texPtr;		// iterates through a list every N seconds/ticks.
	TextureData* baseFrame;		// 
	u8* baseData;				// 
};

enum
{
	BM_ANIMATED_TEXTURE = -2,
};

struct MemoryRegion;

namespace TFE_Jedi
{
	void bitmap_setupAnimationTask();

	void bitmap_setAllocator(MemoryRegion* allocator);
	MemoryRegion* bitmap_getAllocator();
	void bitmap_clearLevelData();
	void bitmap_clearAll();

	// levelTexture bool was added for TFE to make serializing texture state easier.
	// if levelTexture is false, then textures are not serialized and not cleared at level end.
	TextureData* bitmap_load(const char* name, u32 decompress, AssetPool pool = POOL_LEVEL, bool addToCache = true);
	bool bitmap_setupAnimatedTexture(TextureData** texture, s32 index);

	Allocator* bitmap_getAnimatedTextures();
	TextureData** bitmap_getTextures(s32* textureCount, AssetPool pool);
	s32 bitmap_getLevelTextureIndex(const char* name);

	bool bitmap_getTextureIndex(TextureData* tex, s32* index, AssetPool* pool);
	TextureData* bitmap_getTextureByIndex(s32 index, AssetPool pool);
	const char* bitmap_getTextureName(s32 index, AssetPool pool);

	// Used for tools.
	TextureData* bitmap_loadFromMemory(const u8* data, size_t size, u32 decompress);
	Allocator* bitmap_getAnimTextureAlloc();

	// Serialization.
	void bitmap_serializeLevelTextures(Stream* stream);

	void bitmap_writeOpaque(const char* filePath, u16 width, u16 height, u8* image);
	void bitmap_writeTransparent(const char* filePath, u16 width, u16 height, u8* image);

	void bitmap_setCoreArchives(const char** coreArchives, s32 count);
}

#pragma once
//////////////////////////////////////////////////////////////////////
// Update sprite asset based on reverse engineering.
// Switch to this once the Jedi Renderer completely replaces the
// existing renderer.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/stream.h>
#include <vector>

// The original DOS code relied on 32-bit pointers and just swapped offsets for pointers at load time.
// In order to keep the original data intact, the original 32-bit offsets are kept but pointer
// translation is done at runtime.
//
// Note that this will cause extra overhead and should be refactored in the future.

#define WAX_MAX_ANIM 32
#define WAX_MAX_VIEWS 32
#define WAX_MAX_FRAMES 32
#define WAX_DECOMPRESS_SIZE 1024

#pragma pack(push)
#pragma pack(1)
struct WaxCell
{
	s32 sizeX;
	s32 sizeY;
	s32 compressed;
	s32 dataSize;
	u32 columnOffset;
	s32 textureId;		// TFE: Replace padding with textureID for the GPU renderer.
};

struct WaxFrame
{
	s32 offsetX;
	s32 offsetY;
	s32 flip;
	s32 cellOffset;
	s32 widthWS;
	s32 heightWS;
	s32 pad1;
	s32 pad2;
};

struct WaxView
{
	s32 pad1;
	s32 pad2;
	s32 pad3;
	s32 pad4;

	s32 frameOffsets[WAX_MAX_FRAMES];
};

struct WaxAnim
{
	s32 worldWidth;
	s32 worldHeight;
	s32 frameRate;
	s32 frameCount;
	s32 pad2;
	s32 pad3;
	s32 pad4;

	s32 viewOffsets[WAX_MAX_VIEWS];
};

struct Wax
{
	s32 version;	// constant = 0x00100100
	s32 animCount;
	s32 frameCount;
	s32 cellCount;
	s32 xScale;
	s32 yScale;
	s32 xtraLight;
	s32 pad4;
	s32 animOffsets[WAX_MAX_ANIM];
};
#pragma pack(pop)

#define WAX_AnimPtr(waxPtr, animId) ((waxPtr)->animOffsets[(animId)] ? (WaxAnim*)((u8*)(waxPtr) + (waxPtr)->animOffsets[(animId)]) : nullptr)
#define WAX_ViewPtr(waxPtr, jAnim, viewId) ((jAnim)->viewOffsets[(viewId)] ? (WaxView*)((u8*)(waxPtr) + (jAnim)->viewOffsets[(viewId)]) : nullptr)
#define WAX_FramePtr(waxPtr, jView, frameId) ((jView)->frameOffsets[(frameId)] ? (WaxFrame*)((u8*)(waxPtr) + (jView)->frameOffsets[(frameId)]) : nullptr)
#define WAX_CellPtr(waxPtr, jFrame) ((jFrame)->cellOffset ? (WaxCell*)((u8*)(waxPtr) + (jFrame)->cellOffset) : nullptr)

typedef Wax JediWax;
typedef WaxFrame JediFrame;

namespace TFE_Sprite_Jedi
{
	JediFrame* getFrame(const char* name, AssetPool pool = POOL_LEVEL);
	JediWax*   getWax(const char* name, AssetPool pool = POOL_LEVEL);
	void freeAll();
	void freeLevelData();

	const std::vector<JediWax*>& getWaxList(AssetPool pool = POOL_LEVEL);
	const std::vector<JediFrame*>& getFrameList(AssetPool pool = POOL_LEVEL);

	bool getWaxIndex(JediWax* wax, s32* index, AssetPool* pool);
	JediWax* getWaxByIndex(s32 index, AssetPool pool);

	bool getFrameIndex(JediFrame* frame, s32* index, AssetPool* pool);
	JediFrame* getFrameByIndex(s32 index, AssetPool pool);

	void sprite_serializeSpritesAndFrames(Stream* stream);
}

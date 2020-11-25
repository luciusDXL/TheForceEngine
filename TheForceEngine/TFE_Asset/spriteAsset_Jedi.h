#pragma once
//////////////////////////////////////////////////////////////////////
// Update sprite asset based on reverse engineering.
// Switch to this once the Jedi Renderer completely replaces the
// existing renderer.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

// The original DOS code relied on 32-bit pointers and just swapped offsets for pointers at load time.
// In order to keep the original data intact, the original 32-bit offsets are kept but pointer
// translation is done at runtime.
//
// Note that this will cause extra overhead and should be refactored in the future.

#define WAX_MAX_ANIM 32
#define WAX_MAX_VIEWS 32
#define WAX_MAX_FRAMES 32

#pragma pack(push)
#pragma pack(1)
struct WaxCell
{
	s32 sizeX;
	s32 sizeY;
	s32 compressed;
	s32 dataSize;
	u32 columnOffset;
	s32 pad1;
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

#define WAX_AnimPtr(basePtr, jWax, animId) (WaxAnim*)(basePtr + jWax.animOffsets[animId])
#define WAX_ViewPtr(basePtr, jAnim, viewId) (WaxView*)(basePtr + jAnim->vieOffets[viewId])
#define WAX_FramePtr(basePtr, jView, frameId) (WaxFrame*)(basePtr + jView->frameOffsets[frameId])
#define WAX_CellPtr(basePtr, jFrame) (WaxCell*)(basePtr + jFrame->cellOffset)

struct JediWax
{
	u8* basePtr;
	Wax* wax;
};

struct JediFrame
{
	u8* basePtr;
	WaxFrame* frame;
};

namespace TFE_Sprite_Jedi
{
	JediFrame* getFrame(const char* name);
	JediWax*   getWax(const char* name);
	void freeAll();
}

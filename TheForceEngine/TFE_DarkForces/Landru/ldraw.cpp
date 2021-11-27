#include "ldraw.h"
#include "lcanvas.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <assert.h>
#include <map>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	void deltaImage(s16* data, s16 x, s16 y)
	{
		u8* framebuffer = vfb_getCpuBuffer();
		const u32 stride = vfb_getStride();

		u8* srcImage = (u8*)data;
		while (1)
		{
			const s16* deltaLine = (s16*)srcImage;
			s16 sizeAndType = deltaLine[0];
			if (sizeAndType == 0)
			{
				break;
			}

			s16 xStart = deltaLine[1] + x;
			s16 yStart = deltaLine[2] + y;
			// Size of the Delta Line structure.
			srcImage += sizeof(s16) * 3;
						
			const JBool rle = (sizeAndType & 1) ? JTRUE : JFALSE;
			s32 pixelCount  = (sizeAndType >> 1) & 0x3fff;
			u8* dstImage = &framebuffer[yStart*stride + xStart];

			while (pixelCount > 0)
			{
				if (rle)
				{
					//read count byte...
					u8 count = *srcImage; srcImage++;
					if (!(count & 1)) // direct
					{
						count >>= 1;
						for (s32 p = 0; p < count; p++, dstImage++, srcImage++)
						{
							*dstImage = *srcImage;
						}
						pixelCount -= count;
					}
					else	//rle
					{
						count >>= 1;
						const u8 pixel = *srcImage; srcImage++;
						for (s32 p = 0; p < count; p++, dstImage++)
						{
							*dstImage = pixel;
						}
						pixelCount -= count;
					}
				}
				else
				{
					for (s32 p = 0; p < pixelCount; p++, dstImage++, srcImage++)
					{
						*dstImage = *srcImage;
					}
					pixelCount = 0;
				}
			}
		}
	}

	void deltaClip(s16* data, s16 x, s16 y)
	{
		u8* framebuffer = vfb_getCpuBuffer();
		const u32 stride = vfb_getStride();

		LRect clipRect;
		lcanvas_getClip(&clipRect);

		u8* srcImage = (u8*)data;
		while (1)
		{
			const s16* deltaLine = (s16*)srcImage;
			s16 sizeAndType = deltaLine[0];
			s16 xStart = deltaLine[1] + x;
			s16 yStart = deltaLine[2] + y;
			srcImage += 3 * sizeof(s16);

			if (sizeAndType == 0) { break; }

			const JBool rle = (sizeAndType & 1) ? JTRUE : JFALSE;
			s32 pixelCount = (sizeAndType >> 1) & 0x3fff;
			u8* dstImage = &framebuffer[yStart*stride];
			
			s16 xCur = xStart;
			s16 yCur = yStart;
			JBool writeRow = (yCur >= clipRect.top && yCur < clipRect.bottom) ? JTRUE : JFALSE;

			while (pixelCount > 0)
			{
				if (rle)
				{
					//read count byte...
					u8 count = *srcImage; srcImage++;
					if (!(count & 1)) // direct
					{
						count >>= 1;
						for (s32 p = 0; p < count; p++, srcImage++, xCur++)
						{
							if (writeRow && xCur >= clipRect.left && xCur < clipRect.right)
							{
								dstImage[xCur] = *srcImage;
							}
						}
						pixelCount -= count;
					}
					else	//rle
					{
						count >>= 1;
						const u8 pixel = *srcImage; srcImage++;
						for (s32 p = 0; p < count; p++, xCur++)
						{
							if (writeRow && xCur >= clipRect.left && xCur < clipRect.right)
							{
								dstImage[xCur] = pixel;
							}
						}
						pixelCount -= count;
					}
				}
				else
				{
					for (s32 p = 0; p < pixelCount; p++, srcImage++, xCur++)
					{
						if (writeRow && xCur >= clipRect.left && xCur < clipRect.right)
						{
							dstImage[xCur] = *srcImage;
						}
					}
					pixelCount = 0;
				}
			}
		}
	}

	void deltaFlip(s16* data, s16 x, s16 y, s16 w)
	{
		assert(0);
	}

	void deltaFlipClip(s16* data, s16 x, s16 y, s16 w)
	{
		assert(0);
	}
}
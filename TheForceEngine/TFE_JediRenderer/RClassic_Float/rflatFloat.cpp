#include <TFE_Asset/textureAsset.h>
#include "rflatFloat.h"
#include "rlightingFloat.h"
#include "redgePairFloat.h"
#include "rclassicFloat.h"
#include "fixedPoint20.h"
#include "../rscanline.h"
#include "../rsector.h"
#include "../redgePair.h"
#include "../rmath.h"
#include "../rcommon.h"
#include <assert.h>

namespace TFE_JediRenderer
{

namespace RClassic_Float
{
	static s32 s_scanlineX0;

	static fixed44_20 s_scanlineU0;
	static fixed44_20 s_scanlineV0;
	static fixed44_20 s_scanline_dUdX;
	static fixed44_20 s_scanline_dVdX;

	static s32 s_scanlineWidth;
	static const u8* s_scanlineLight;
	static u8* s_scanlineOut;

	static u8* s_ftexImage;
	static s32 s_ftexDataEnd;
	static s32 s_ftexHeight;
	static s32 s_ftexWidthMask;
	static s32 s_ftexHeightMask;
	static s32 s_ftexHeightLog2;
	
	void flat_addEdges(s32 length, s32 x0, f32 dyFloor_dx, f32 yFloor, f32 dyCeil_dx, f32 yCeil)
	{
		if (s_flatCount < MAX_SEG && length > 0)
		{
			const f32 lengthFloat = f32(length - 1);

			f32 yCeil1 = yCeil;
			if (dyCeil_dx != 0)
			{
				yCeil1 += dyCeil_dx*lengthFloat;
			}
			f32 yFloor1 = yFloor;
			if (dyFloor_dx != 0)
			{
				yFloor1 += dyFloor_dx*lengthFloat;
			}

			edgePair_setup(length, x0, dyFloor_dx, yFloor1, yFloor, dyCeil_dx, yCeil, yCeil1, s_flatEdge);

			if (s_flatEdge->yPixel_C1 - 1 > s_wallMaxCeilY)
			{
				s_wallMaxCeilY = s_flatEdge->yPixel_C1 - 1;
			}
			if (s_flatEdge->yPixel_F1 + 1 < s_wallMinFloorY)
			{
				s_wallMinFloorY = s_flatEdge->yPixel_F1 + 1;
			}
			if (s_wallMaxCeilY < s_windowMinY)
			{
				s_wallMaxCeilY = s_windowMinY;
			}
			if (s_wallMinFloorY > s_windowMaxY)
			{
				s_wallMinFloorY = s_windowMaxY;
			}

			s_flatEdge++;
			s_flatCount++;
		}
	}
		
	void drawScanline()
	{
		const fixed44_20 dVdX = s_scanline_dVdX;
		const fixed44_20 dUdX = s_scanline_dUdX;

		fixed44_20 V = s_scanlineV0;
		fixed44_20 U = s_scanlineU0;
		for (s32 i = s_scanlineWidth - 1; i >= 0; i--, U += dUdX, V += dVdX)
		{
			// Note this produces a distorted mapping if the texture is not 64x64.
			// This behavior matches the original.
			const u32 texel = ((floor20(U) & 63) * 64 + (floor20(V) & 63)) & s_ftexDataEnd;
			s_scanlineOut[i] = s_scanlineLight[s_ftexImage[texel]];
		}
	}

	void drawScanline_Fullbright()
	{
		const fixed44_20 dVdX = s_scanline_dVdX;
		const fixed44_20 dUdX = s_scanline_dUdX;

		fixed44_20 V = s_scanlineV0;
		fixed44_20 U = s_scanlineU0;
		for (s32 i = s_scanlineWidth - 1; i >= 0; i--, U += dUdX, V += dVdX)
		{
			// Note this produces a distorted mapping if the texture is not 64x64.
			// This behavior matches the original.
			const u32 texel = ((floor20(U) & 63) * 64 + (floor20(V) & 63)) & s_ftexDataEnd;
			s_scanlineOut[i] = s_ftexImage[texel];
		}
	}

	bool flat_setTexture(TextureFrame* tex)
	{
		if (!tex) { return false; }

		s_ftexHeight = tex->height;
		s_ftexWidthMask = tex->width - 1;
		s_ftexHeightMask = tex->height - 1;
		s_ftexHeightLog2 = tex->logSizeY;
		s_ftexImage = tex->image;
		s_ftexDataEnd = tex->width * tex->height - 1;

		return true;
	}
	
	void flat_drawCeiling(RSector* sector, EdgePair* edges, s32 count)
	{
		f32 textureOffsetU = s_cameraPosX - sector->ceilOffsetX.f32;
		f32 textureOffsetV = sector->ceilOffsetZ.f32 - s_cameraPosZ;

		f32 relCeil          =  sector->ceilingHeight.f32 - s_eyeHeight;
		f32 scaledRelCeil    =  relCeil * s_focalLenAspect;
		f32 cosScaledRelCeil =  scaledRelCeil * s_cosYaw;
		f32 negSinRelCeil    = -relCeil * s_sinYaw;
		f32 sinScaledRelCeil =  scaledRelCeil * s_sinYaw;
		f32 negCosRelCeil    = -relCeil * s_cosYaw;

		if (!flat_setTexture(sector->ceilTex)) { return; }

		for (s32 y = s_windowMinY; y <= s_wallMaxCeilY; y++)
		{
			const s32 yOffset = y * s_width;
			const s32 yShear = s_heightInPixelsBase - s_heightInPixels;
			// Calculate yRcp directly instead of using a table.
			const f32 yMinusHalf = f32(yShear + y) - s_halfHeightBase;
			const f32 yRcp = yMinusHalf != 0.0f ? 1.0f / yMinusHalf : 1.0f;
			const f32 z = scaledRelCeil * yRcp;

			s32 x = s_windowMinX;
			s32 left = 0;
			s32 right = 0;
			for (s32 i = 0; i < count;)
			{
				if (!flat_buildScanlineCeiling(i, count, x, y, left, right, s_scanlineWidth, edges))
				{
					break;
				}

				if (s_scanlineWidth > 0)
				{
					assert(left >= 0 && left + s_scanlineWidth <= s_width);
					assert(y >= 0 && y < s_height);
					s_scanlineX0  = left;
					s_scanlineOut = &s_display[left + yOffset];

					const f32 worldToTexelScale = 8.0f;
					f32 rightClip = f32(right - s_screenXMid) * s_aspectScale;
					f32 v0 = (cosScaledRelCeil - (negSinRelCeil*rightClip)) * yRcp;
					f32 u0 = (sinScaledRelCeil + (negCosRelCeil*rightClip)) * yRcp;

					s_scanlineV0 = floatToFixed20((v0 - textureOffsetV) * worldToTexelScale);
					s_scanlineU0 = floatToFixed20((u0 - textureOffsetU) * worldToTexelScale);

					const f32 worldTexelScaleAspect = yRcp * worldToTexelScale * s_aspectScale;
					s_scanline_dVdX =  floatToFixed20(negSinRelCeil * worldTexelScaleAspect);
					s_scanline_dUdX = -floatToFixed20(negCosRelCeil * worldTexelScaleAspect);
					s_scanlineLight =  computeLighting(z, 0);
					
					if (s_scanlineLight)
					{
						drawScanline();
					}
					else
					{
						drawScanline_Fullbright();
					}
				}
			} // while (i < count)
		}
	}
		
	void flat_drawFloor(RSector* sector, EdgePair* edges, s32 count)
	{
		f32 textureOffsetU = s_cameraPosX - sector->floorOffsetX.f32;
		f32 textureOffsetV = sector->floorOffsetZ.f32 - s_cameraPosZ;

		f32 relFloor       = sector->floorHeight.f32 - s_eyeHeight;
		f32 scaledRelFloor = relFloor * s_focalLenAspect;

		f32 cosScaledRelFloor = scaledRelFloor * s_cosYaw;
		f32 negSinRelFloor    =-relFloor * s_sinYaw;
		f32 sinScaledRelFloor = scaledRelFloor * s_sinYaw;
		f32 negCosRelFloor    =-relFloor * s_cosYaw;

		if (!flat_setTexture(sector->floorTex)) { return; }

		for (s32 y = s_wallMinFloorY; y <= s_windowMaxY; y++)
		{
			const s32 yOffset = y * s_width;
			const s32 yShear = s_heightInPixelsBase - s_heightInPixels;
			// Calculate yRcp directly instead of using a table.
			const f32 yMinusHalf = f32(yShear + y) - s_halfHeightBase;
			const f32 yRcp = (yMinusHalf != 0.0f) ? 1.0f / yMinusHalf : 1.0f;
			const f32 z = scaledRelFloor * yRcp;

			s32 x = s_windowMinX;
			s32 left = 0;
			s32 right = 0;
			for (s32 i = 0; i < count;)
			{
				s32 winMaxX = s_windowMaxX;

				// Search for the left edge of the scanline.
				if (!flat_buildScanlineFloor(i, count, x, y, left, right, s_scanlineWidth, edges))
				{
					break;
				}

				if (s_scanlineWidth > 0)
				{
					assert(left >= 0 && left + s_scanlineWidth <= s_width);
					assert(y >= 0 && y < s_height);
					s_scanlineX0 = left;
					s_scanlineOut = &s_display[left + yOffset];

					const f32 worldToTexelScale = 8.0f;
					f32 rightClip = f32(right - s_screenXMid) * s_aspectScale;
					f32 v0 = (cosScaledRelFloor - (negSinRelFloor * rightClip)) * yRcp;
					f32 u0 = (sinScaledRelFloor + (negCosRelFloor * rightClip)) * yRcp;
					s_scanlineV0 = floatToFixed20((v0 - textureOffsetV) * worldToTexelScale);
					s_scanlineU0 = floatToFixed20((u0 - textureOffsetU) * worldToTexelScale);

					const f32 worldTexelScaleAspect = yRcp * worldToTexelScale * s_aspectScale;
					s_scanline_dVdX =  floatToFixed20(negSinRelFloor * worldTexelScaleAspect);
					s_scanline_dUdX = -floatToFixed20(negCosRelFloor * worldTexelScaleAspect);
					s_scanlineLight = computeLighting(z, 0);

					if (s_scanlineLight)
					{
						drawScanline();
					}
					else
					{
						drawScanline_Fullbright();
					}
				}
			} // while (i < count)
		}
	}
}  // RClassic_Float

}  // TFE_JediRenderer
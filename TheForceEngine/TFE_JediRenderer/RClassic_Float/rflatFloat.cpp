#include <TFE_Asset/textureAsset.h>
#include "rflatFloat.h"
#include "rlightingFloat.h"
#include "redgePairFloat.h"
#include "rclassicFloat.h"
#include "fixedPoint20.h"
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
	
	void clipScanline(s32* left, s32* right, s32 y)
	{
		s32 x0 = *left;
		s32 x1 = *right;
		if (x0 > s_windowMaxX || x1 < s_windowMinX)
		{
			*left = x1 + 1;
			return;
		}
		if (x0 < s_windowMinX) { x0 = s_windowMinX; *left  = x0; }
		if (x1 > s_windowMaxX) { x1 = s_windowMaxX; *right = x1; }

		// s_windowMaxCeil and s_windowMinFloor overlap and y is inside that overlap.
		if (y < s_windowMaxCeil && y > s_windowMinFloor)
		{
			// Find the left side of the scanline.
 			s32* top = &s_windowTop[x0];
			s32* bot = &s_windowBot[x0];
			while (x0 <= x1)
			{
				if (y >= *top && y <= *bot)
				{
					break;
				}
				x0++;
				top++;
				bot++;
			};
			*left = x0;
			if (x0 > x1)
			{
				return;
			}

			// Find the right side of the scanline.
			top = &s_windowTop[x1];
			bot = &s_windowBot[x1];
			while (1)
			{
				if ((y >= *top && y <= *bot) || (x0 > x1))
				{
					*right = x1;
					return;
				}
				x1--;
				top--;
				bot--;
			};
		}
		// y is on the ceiling plane.
		if (y < s_windowMaxCeil)
		{
			s32* top = &s_windowTop[x0];
			while (*top > y && x1 >= x0)
			{
				x0++;
				top++;
			}
			*left = x0;
			if (x0 <= x1)
			{
				s32* top = &s_windowTop[x1];
				while (*top > y && x1 >= x0)
				{
					x1--;
					top--;
				}
				*right = x1;
			}
		}
		// y is on the floor plane.
		else if (y > s_windowMinFloor)
		{
			s32* bot = &s_windowBot[x0];
			while (*bot < y && x0 <= x1)
			{
				x0++;
				bot++;
			}
			*left = x0;

			if (x0 <= x1)
			{
				bot = &s_windowBot[x1];
				while (*bot < y && x1 >= x0)
				{
					x1--;
					bot--;
				}
				*right = x1;
			}
		}
	}
		
	// This produces functionally identical results to the original but splits apart the U/V and dUdx/dVdx into seperate variables
	// to account for C vs ASM differences.
	void drawScanline()
	{
		fixed44_20 V = s_scanlineV0;
		fixed44_20 U = s_scanlineU0;
		fixed44_20 dVdX = s_scanline_dVdX;
		fixed44_20 dUdX = s_scanline_dUdX;

		// Note this produces a distorted mapping if the texture is not 64x64.
		// This behavior matches the original.
		u32 texel = (floor20(U) & 63) * 64 + (floor20(V) & 63);
		texel &= s_ftexDataEnd;
		U += dUdX;
		V += dVdX;

		for (s32 i = s_scanlineWidth - 1; i >= 0; i--)
		{
			u8 c = s_scanlineLight[s_ftexImage[texel]];
			texel = (floor20(U) & 63) * 64 + (floor20(V) & 63);
			texel &= s_ftexDataEnd;
			U += dUdX;
			V += dVdX;
			s_scanlineOut[i] = c;
		}
	}

	void drawScanline_Fullbright()
	{
		fixed44_20 V = s_scanlineV0;
		fixed44_20 U = s_scanlineU0;
		fixed44_20 dVdX = s_scanline_dVdX;
		fixed44_20 dUdX = s_scanline_dUdX;

		// Note this produces a distorted mapping if the texture is not 64x64.
		// This behavior matches the original.
		u32 texel = (floor20(U) & 63) * 64 + (floor20(V) & 63);
		texel &= s_ftexDataEnd;
		U += dUdX;
		V += dVdX;

		for (s32 i = s_scanlineWidth - 1; i >= 0; i--)
		{
			u8 c = s_ftexImage[texel];
			texel = (floor20(U) & 63) * 64 + (floor20(V) & 63);
			texel &= s_ftexDataEnd;
			U += dUdX;
			V += dVdX;
			s_scanlineOut[i] = c;
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

	bool flat_buildScanelineCeiling(s32& i, s32 count, s32& x, s32 y, s32& left, s32& right, const EdgePair* edges)
	{
		// Search for the left edge of the scanline.
		s32 hasLeft = 0;
		s32 hasRight = 0;
		while (i < count && hasLeft == 0)
		{
			const EdgePair* edge = &edges[i];
			if (y < edge->yPixel_C0)	// Y is above the current edge, so start at left = x
			{
				left = x;
				i++;
				hasLeft = -1;
				x = edge->x1 + 1;
			}
			else if (y >= edge->yPixel_C1)	// Y is inside the current edge, so step to the end (left not set yet).
			{
				x = edge->x1 + 1;
				i++;
				if (i >= count)
				{
					hasLeft = -1;
					left = x;
				}
			}
			else if (edge->dyCeil_dx.f16_16 > 0)  // find the left intersection.
			{
				x = edge->x0;
				s32 ey = s_columnTop[x];
				while (x < s_windowMaxX && y > ey)
				{
					x++;
					ey = s_columnTop[x];
				};

				left = x;
				x = edge->x1 + 1;
				hasLeft = -1;
				i++;
			}
			else
			{
				left = x;
				hasLeft = -1;
			}
		}  // while (i < count && hasLeft == 0)

		if (i < count)
		{
			// Search for the right edge of the scanline.
			while (i < count && hasRight == 0)
			{
				const EdgePair* edge = &edges[i];
				if (y < edge->yPixel_C0)		// Y is above the current edge, so move on to the next edge.
				{
					x = edge->x1 + 1;
					i++;
					if (i >= count)
					{
						right = x;
						hasRight = -1;
					}
				}
				else if (y >= edge->yPixel_C1)	// Y is below the current edge so it must be the end.
				{
					right = x - 1;
					x = edge->x1 + 1;
					i++;
					hasRight = -1;
				}
				else
				{
					if (edge->dyCeil_dx.f16_16 >= 0)
					{
						hasRight = -1;
						right = x;
						break;
					}
					else
					{
						x = edge->x0;
						s32 ey = s_columnTop[x];
						while (x < s_windowMaxX && ey >= y)
						{
							x++;
							ey = s_columnTop[x];
						}
						right = x;
						x = edge->x1 + 1;
						i++;
						hasRight = -1;
						break;
					}
				}
			}
		}  // if (i < count)
		else
		{
			if (hasLeft == 0) { return false; }
			right = x;
		}

		clipScanline(&left, &right, y);
		s_scanlineWidth = right - left + 1;
		return true;
	}

	bool flat_buildScanelineFloor(s32& i, s32 count, s32& x, s32 y, s32& left, s32& right, const EdgePair* edges)
	{
		// Search for the left edge of the scanline.
		s32 hasLeft = 0;
		s32 hasRight = 0;
		while (i < count && hasLeft == 0)
		{
			const EdgePair* edge = &edges[i];
			if (y >= edge->yPixel_F0)	// Y is above the current edge, so start at left = x
			{
				left = x;
				i++;
				hasLeft = -1;
				x = edge->x1 + 1;
			}
			else if (y < edge->yPixel_F1)	// Y is inside the current edge, so step to the end (left not set yet).
			{
				x = edge->x1 + 1;
				i++;
				if (i >= count)
				{
					hasLeft = -1;
					left = x;
				}
			}
			else if (edge->dyFloor_dx.f16_16 < 0)  // find the left intersection.
			{
				x = edge->x0;
				s32 ey = s_columnBot[x];
				while (x < s_windowMaxX && y < ey)
				{
					x++;
					ey = s_columnBot[x];
				};

				left = x;
				x = edge->x1 + 1;
				hasLeft = -1;
				i++;
			}
			else
			{
				left = x;
				hasLeft = -1;
			}
		}  // while (i < count && hasLeft == 0)

		if (i < count)
		{
			// Search for the right edge of the scanline.
			while (i < count && hasRight == 0)
			{
				const EdgePair* edge = &edges[i];
				if (y >= edge->yPixel_F0)		// Y is above the current edge, so move on to the next edge.
				{
					x = edge->x1 + 1;
					i++;
					if (i >= count)
					{
						right = x;
						hasRight = -1;
					}
				}
				else if (y < edge->yPixel_F1)	// Y is below the current edge so it must be the end.
				{
					right = x - 1;
					x = edge->x1 + 1;
					i++;
					hasRight = -1;
				}
				else
				{
					if (edge->dyFloor_dx.f16_16 <= 0)
					{
						hasRight = -1;
						right = x;
						break;
					}
					else
					{
						x = edge->x0;
						s32 ey = s_columnBot[x];
						while (x < s_windowMaxX && ey <= y)
						{
							x++;
							ey = s_columnBot[x];
						}
						right = x;
						x = edge->x1 + 1;
						i++;
						hasRight = -1;
						break;
					}
				}
			}
		}  // if (i < count)
		else
		{
			if (hasLeft == 0) { return false; }
			right = x;
		}

		clipScanline(&left, &right, y);
		s_scanlineWidth = right - left + 1;
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
			s32 x = s_windowMinX;
			s32 yOffset = y * s_width;
			s32 yShear = s_heightInPixelsBase - s_heightInPixels;
			f32 yRcp = s_rcp_yMinusHalfHeight[yShear + y + s_height];
			f32 z = scaledRelCeil * yRcp;

			s32 left = 0;
			s32 right = 0;
			for (s32 i = 0; i < count;)
			{
				if (!flat_buildScanelineCeiling(i, count, x, y, left, right, edges))
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
			s32 x = s_windowMinX;
			s32 yOffset = y * s_width;
			s32 yShear = s_heightInPixelsBase - s_heightInPixels;
			f32 yRcp = s_rcp_yMinusHalfHeight[yShear + y + s_height];
			f32 z = scaledRelFloor * yRcp;

			s32 left = 0;
			s32 right = 0;
			for (s32 i = 0; i < count;)
			{
				s32 winMaxX = s_windowMaxX;

				// Search for the left edge of the scanline.
				if (!flat_buildScanelineFloor(i, count, x, y, left, right, edges))
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
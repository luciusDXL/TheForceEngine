#include "rflat.h"
#include "rlighting.h"
#include "rsector.h"
#include "fixedPoint.h"
#include "rmath.h"
#include "rcommon.h"
#include <TFE_Asset/textureAsset.h>
#include <assert.h>

using namespace RendererClassic;
using namespace RClassicLighting;
using namespace FixedPoint;
using namespace RMath;

namespace RClassicFlat
{
	static s32 s_scanlineX0;
	static fixed16 s_scanlineU0;
	static fixed16 s_scanlineV0;
	static fixed16 s_scanline_dUdX;
	static fixed16 s_scanline_dVdX;
	static s32 s_scanlineWidth;
	static const u8* s_scanlineLight;
	static u8* s_scanlineOut;

	static u8* s_ftexImage;
	static s32 s_ftexDataEnd;
	static s32 s_ftexHeight;
	static s32 s_ftexWidthMask;
	static s32 s_ftexHeightMask;
	static s32 s_ftexHeightLog2;

	void flat_setup(s32 length, s32 x0, fixed16 dyFloor_dx, fixed16 yFloor1, fixed16 yFloor, fixed16 dyCeil_dx, fixed16 yCeil, fixed16 yCeil1, FlatEdges* flat)
	{
		const s32 yF0 = round16(yFloor);
		const s32 yF1 = round16(yFloor1);
		const s32 yC0 = round16(yCeil);
		const s32 yC1 = round16(yCeil1);

		flat->yCeil0 = yCeil;
		flat->yCeil1 = yCeil1;
		flat->dyCeil_dx = dyCeil_dx;
		if (yC0 < yC1)
		{
			flat->yPixel_C0 = yC0;
			flat->yPixel_C1 = yC1;
		}
		else
		{
			flat->yPixel_C0 = yC1;
			flat->yPixel_C1 = yC0;
		}

		flat->yFloor0 = yFloor;
		flat->yFloor1 = yFloor1;
		flat->dyFloor_dx = dyFloor_dx;
		if (yF0 > yF1)
		{
			flat->yPixel_F0 = yF0;
			flat->yPixel_F1 = yF1;
		}
		else
		{
			flat->yPixel_F0 = yF1;
			flat->yPixel_F1 = yF0;
		}

		flat->lengthInPixels = length;
		flat->x0 = x0;
		flat->x1 = x0 + length - 1;
	}

	void flat_addEdges(s32 length, s32 x0, fixed16 dyFloor_dx, fixed16 yFloor, fixed16 dyCeil_dx, fixed16 yCeil)
	{
		if (s_flatCount < MAX_SEG && length > 0)
		{
			const fixed16 lengthFixed = intToFixed16(length - 1);

			fixed16 yCeil1 = yCeil;
			if (dyCeil_dx != 0)
			{
				yCeil1 += mul16(dyCeil_dx, lengthFixed);
			}
			fixed16 yFloor1 = yFloor;
			if (dyFloor_dx != 0)
			{
				yFloor1 += mul16(dyFloor_dx, lengthFixed);
			}

			flat_setup(length, x0, dyFloor_dx, yFloor1, yFloor, dyCeil_dx, yCeil, yCeil1, s_lowerFlatEdge);

			if (s_lowerFlatEdge->yPixel_C1 - 1 > s_wallMaxCeilY)
			{
				s_wallMaxCeilY = s_lowerFlatEdge->yPixel_C1 - 1;
			}
			if (s_lowerFlatEdge->yPixel_F1 + 1 < s_wallMinFloorY)
			{
				s_wallMinFloorY = s_lowerFlatEdge->yPixel_F1 + 1;
			}
			if (s_wallMaxCeilY < s_windowMinY)
			{
				s_wallMaxCeilY = s_windowMinY;
			}
			if (s_wallMinFloorY > s_windowMaxY)
			{
				s_wallMinFloorY = s_windowMaxY;
			}

			s_lowerFlatEdge++;
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
	
		// Shouldn't hit yet.
		if (y < s_yMin && y > s_yMax)
		{
			// TODO
			assert(0);
		}
		if (y < s_yMin)
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
				s32* top = &s_windowTop[x0];
				while (*top > y && x1 >= x0)
				{
					x1--;
					top--;
				}
				*right = x1;
			}
		}
		else if (y > s_yMax)
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
		
#if ENABLE_HIGH_PRECISION_FIXED_POINT == 0
	// This produces functionally identical results to the original but splits apart the U/V and dUdx/dVdx into seperate variables
	// to account for C vs ASM differences.
	void drawScanline()
	{
		// Convert from 16.16 coordinates to 6.10 coordinates
		s32 V = s32(s_scanlineV0 >> SUB_TEXEL_SHIFT);
		s32 U = s32(s_scanlineU0 >> SUB_TEXEL_SHIFT);
		s32 dVdX = s32(s_scanline_dVdX >> SUB_TEXEL_SHIFT);
		s32 dUdX = s32(s_scanline_dUdX >> SUB_TEXEL_SHIFT);

		// Note this produces a distorted mapping if the texture is not 64x64.
		// This behavior matches the original.
		u32 texel = ((U >> 10) & 63) * 64 + ((V >> 10) & 63);
		texel &= s_ftexDataEnd;
		U += dUdX;
		V += dVdX;

		for (s32 i = s_scanlineWidth - 1; i >= 0; i--)
		{
			u8 c = s_scanlineLight[s_ftexImage[texel]];
			texel = ((U >> 10) & 63) * 64 + ((V >> 10) & 63);
			texel &= s_ftexDataEnd;
			U += dUdX;
			V += dVdX;
			s_scanlineOut[i] = c;
		}
	}

	void drawScanline_Fullbright()
	{
		// Convert from 16.16 coordinates to 6.10 coordinates
		s32 V = s32(s_scanlineV0 >> SUB_TEXEL_SHIFT);
		s32 U = s32(s_scanlineU0 >> SUB_TEXEL_SHIFT);
		s32 dVdX = s32(s_scanline_dVdX >> SUB_TEXEL_SHIFT);
		s32 dUdX = s32(s_scanline_dUdX >> SUB_TEXEL_SHIFT);

		// Note this produces a distorted mapping if the texture is not 64x64.
		// This behavior matches the original.
		u32 texel = ((U >> 10) & 63)*64 + ((V >> 10) & 63);
		texel &= s_ftexDataEnd;
		U += dUdX;
		V += dVdX;

		for (s32 i = s_scanlineWidth - 1; i >= 0; i--)
		{
			u8 c = s_ftexImage[texel];
			texel = ((U >> 10) & 63)*64 + ((V >> 10) & 63);
			texel &= s_ftexDataEnd;
			U += dUdX;
			V += dVdX;
			s_scanlineOut[i] = c;
		}
	}
#else
	// This produces functionally identical results to the original but splits apart the U/V and dUdx/dVdx into seperate variables
	// to account for C vs ASM differences.
	void drawScanline()
	{
		fixed16 V = s_scanlineV0;
		fixed16 U = s_scanlineU0;
		fixed16 dVdX = s_scanline_dVdX;
		fixed16 dUdX = s_scanline_dUdX;

		// Note this produces a distorted mapping if the texture is not 64x64.
		// This behavior matches the original.
		u32 texel = (floor16(U) & 63) * 64 + (floor16(V) & 63);
		texel &= s_ftexDataEnd;
		U += dUdX;
		V += dVdX;

		for (s32 i = s_scanlineWidth - 1; i >= 0; i--)
		{
			u8 c = s_scanlineLight[s_ftexImage[texel]];
			texel = (floor16(U) & 63) * 64 + (floor16(V) & 63);
			texel &= s_ftexDataEnd;
			U += dUdX;
			V += dVdX;
			s_scanlineOut[i] = c;
		}
	}

	void drawScanline_Fullbright()
	{
		fixed16 V = s_scanlineV0;
		fixed16 U = s_scanlineU0;
		fixed16 dVdX = s_scanline_dVdX;
		fixed16 dUdX = s_scanline_dUdX;

		// Note this produces a distorted mapping if the texture is not 64x64.
		// This behavior matches the original.
		u32 texel = (floor16(U) & 63) * 64 + (floor16(V) & 63);
		texel &= s_ftexDataEnd;
		U += dUdX;
		V += dVdX;

		for (s32 i = s_scanlineWidth - 1; i >= 0; i--)
		{
			u8 c = s_ftexImage[texel];
			texel = (floor16(U) & 63) * 64 + (floor16(V) & 63);
			texel &= s_ftexDataEnd;
			U += dUdX;
			V += dVdX;
			s_scanlineOut[i] = c;
		}
	}
#endif

	void flat_drawCeiling(RSector* sector, FlatEdges* edges, s32 count)
	{
		fixed16 textureOffsetU = s_cameraPosX - sector->ceilOffsetX;
		fixed16 textureOffsetV = sector->ceilOffsetZ - s_cameraPosZ;

		fixed16 relCeil          =  sector->ceilingHeight - s_eyeHeight;
		fixed16 scaledRelCeil    =  mul16(relCeil, s_focalLenAspect);
		fixed16 cosScaledRelCeil =  mul16(scaledRelCeil, s_cosYaw);
		fixed16 negSinRelCeil    = -mul16(relCeil, s_sinYaw);
		fixed16 sinScaledRelCeil =  mul16(scaledRelCeil, s_sinYaw);
		fixed16 negCosRelCeil    = -mul16(relCeil, s_cosYaw);

		TextureFrame* ceilTex = sector->ceilTex;
		s_ftexHeight     = ceilTex->height;
		s_ftexWidthMask  = ceilTex->width  - 1;
		s_ftexHeightMask = ceilTex->height - 1;
		s_ftexHeightLog2 = ceilTex->logSizeY;
		s_ftexImage      = ceilTex->image;
		s_ftexDataEnd    = ceilTex->width * ceilTex->height - 1;

		for (s32 y = s_windowMinY; y <= s_wallMaxCeilY; y++)
		{
			s32 x = s_windowMinX;
			s32 yOffset = y * s_width;
			// TODO: setup y shear
			s32 yShear = 0; // s_heightInPixelsConst - s_heightInPixels;
			fixed16 yRcp = s_rcp_yMinusHalfHeight[yShear + y];
			fixed16 z = mul16(scaledRelCeil, yRcp);

			s32 left = 0;
			s32 right = 0;
			for (s32 i = 0; i < count;)
			{
				s32 winMaxX = s_windowMaxX;

				// Search for the left edge of the scanline.
				s32 hasLeft = 0;
				s32 hasRight = 0;
				while (i < count && hasLeft == 0)
				{
					FlatEdges* edge = &edges[i];
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
					else if (edge->dyCeil_dx > 0)  // find the left intersection.
					{
						x = edge->x0;
						s32 ey = s_columnTop[x];
						while (x < winMaxX && y > ey)
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
						FlatEdges* edge = &edges[i];
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
							if (edge->dyCeil_dx >= 0)
							{
								hasRight = -1;
								right = x;
								break;
							}
							else
							{
								x = edge->x0;
								s32 ey = s_columnTop[x];
								while (x < winMaxX && ey >= y)
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
					if (hasLeft == 0) { break; }
					right = x;
				}

				clipScanline(&left, &right, y);
				s_scanlineWidth = right - left + 1;
				if (s_scanlineWidth > 0)
				{
					assert(left >= 0 && left + s_scanlineWidth <= s_width);
					assert(y >= 0 && y < s_height);
					s_scanlineX0  = left;
					s_scanlineOut = &s_display[left + yOffset];
					fixed16 rightClip = intToFixed16(right - s_screenXMid);

					fixed16 v0 = mul16(cosScaledRelCeil - mul16(negSinRelCeil, rightClip), yRcp);
					s_scanlineV0 = (v0 - textureOffsetV) * 8;

					fixed16 u0 = mul16(sinScaledRelCeil + mul16(negCosRelCeil, rightClip), yRcp);
					s_scanlineU0 = (u0 - textureOffsetU) * 8;

					const fixed16 worldToTexelScale = fixed16(8);
					s_scanline_dVdX =  mul16(negSinRelCeil, yRcp) * worldToTexelScale;
					s_scanline_dUdX = -mul16(negCosRelCeil, yRcp) * worldToTexelScale;
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

	// TODO: Spot check against DOS code.
	void flat_drawFloor(RSector* sector, FlatEdges* edges, s32 count)
	{
		fixed16 textureOffsetU = s_cameraPosX - sector->floorOffsetX;
		fixed16 textureOffsetV = sector->floorOffsetZ - s_cameraPosZ;

		fixed16 relFloor       = sector->floorHeight - s_eyeHeight;
		fixed16 scaledRelFloor = mul16(relFloor, s_focalLenAspect);

		fixed16 cosScaledRelFloor = mul16(scaledRelFloor, s_cosYaw);
		fixed16 negSinRelFloor    =-mul16(relFloor, s_sinYaw);
		fixed16 sinScaledRelFloor = mul16(scaledRelFloor, s_sinYaw);
		fixed16 negCosRelFloor    =-mul16(relFloor, s_cosYaw);

		TextureFrame* floorTex = sector->floorTex;
		s_ftexHeight     = floorTex->height;
		s_ftexWidthMask  = floorTex->width - 1;
		s_ftexHeightMask = floorTex->height - 1;
		s_ftexHeightLog2 = floorTex->logSizeY;
		s_ftexImage      = floorTex->image;
		s_ftexDataEnd    = floorTex->width * floorTex->height - 1;

		for (s32 y = s_wallMinFloorY; y <= s_windowMaxY; y++)
		{
			s32 x = s_windowMinX;
			s32 yOffset = y * s_width;
			// TODO: setup y shear
			s32 yShear = 0; // s_heightInPixelsConst - s_heightInPixels;
			fixed16 yRcp = s_rcp_yMinusHalfHeight[yShear + y];
			fixed16 z = mul16(scaledRelFloor, yRcp);

			s32 left = 0;
			s32 right = 0;
			for (s32 i = 0; i < count;)
			{
				s32 winMaxX = s_windowMaxX;

				// Search for the left edge of the scanline.
				s32 hasLeft = 0;
				s32 hasRight = 0;
				while (i < count && hasLeft == 0)
				{
					FlatEdges* edge = &edges[i];
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
					else if (edge->dyFloor_dx < 0)  // find the left intersection.
					{
						x = edge->x0;
						s32 ey = s_columnBot[x];
						while (x < winMaxX && y < ey)
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
						FlatEdges* edge = &edges[i];
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
							if (edge->dyFloor_dx <= 0)
							{
								hasRight = -1;
								right = x;
								break;
							}
							else
							{
								x = edge->x0;
								s32 ey = s_columnBot[x];
								while (x < winMaxX && ey <= y)
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
					if (hasLeft == 0) { break; }
					right = x;
				}

				clipScanline(&left, &right, y);
				s_scanlineWidth = right - left + 1;
				if (s_scanlineWidth > 0)
				{
					assert(left >= 0 && left + s_scanlineWidth <= s_width);
					assert(y >= 0 && y < s_height);
					s_scanlineX0 = left;
					s_scanlineOut = &s_display[left + yOffset];
					fixed16 rightClip = intToFixed16(right - s_screenXMid);
					fixed16 worldToTexelScale = fixed16(8);

					fixed16 v0 = mul16(cosScaledRelFloor - mul16(negSinRelFloor, rightClip), yRcp);
					fixed16 u0 = mul16(sinScaledRelFloor + mul16(negCosRelFloor, rightClip), yRcp);
					s_scanlineV0 = (v0 - textureOffsetV) * worldToTexelScale;
					s_scanlineU0 = (u0 - textureOffsetU) * worldToTexelScale;

					s_scanline_dVdX =  mul16(negSinRelFloor, yRcp) * worldToTexelScale;
					s_scanline_dUdX = -mul16(negCosRelFloor, yRcp) * worldToTexelScale;
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
}
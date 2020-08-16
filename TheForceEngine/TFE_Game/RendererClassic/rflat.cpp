#include "rflat.h"
#include "rlighting.h"
#include "rsector.h"
#include "redgePair.h"
#include "fixedPoint.h"
#include "rmath.h"
#include "rcommon.h"
#include <TFE_Asset/textureAsset.h>
#include <assert.h>

using namespace RendererClassic;
using namespace RClassicLighting;
using namespace RClassicEdgePair;
using namespace FixedPoint;
using namespace RMath;

namespace RClassicFlat
{
	static s32 s_scanlineX0;

	static fixed16_16 s_scanlineU0;
	static fixed16_16 s_scanlineV0;
	static fixed16_16 s_scanline_dUdX;
	static fixed16_16 s_scanline_dVdX;
	// Higher precision scanline variables.
	static fixed48_16 s_scanlineU0_HQ;
	static fixed48_16 s_scanlineV0_HQ;
	static fixed48_16 s_scanline_dUdX_HQ;
	static fixed48_16 s_scanline_dVdX_HQ;

	static s32 s_scanlineWidth;
	static const u8* s_scanlineLight;
	static u8* s_scanlineOut;

	static u8* s_ftexImage;
	static s32 s_ftexDataEnd;
	static s32 s_ftexHeight;
	static s32 s_ftexWidthMask;
	static s32 s_ftexHeightMask;
	static s32 s_ftexHeightLog2;
	
	void flat_addEdges(s32 length, s32 x0, fixed16_16 dyFloor_dx, fixed16_16 yFloor, fixed16_16 dyCeil_dx, fixed16_16 yCeil)
	{
		if (s_flatCount < MAX_SEG && length > 0)
		{
			const fixed16_16 lengthFixed = intToFixed16(length - 1);

			fixed16_16 yCeil1 = yCeil;
			if (dyCeil_dx != 0)
			{
				yCeil1 += mul16(dyCeil_dx, lengthFixed);
			}
			fixed16_16 yFloor1 = yFloor;
			if (dyFloor_dx != 0)
			{
				yFloor1 += mul16(dyFloor_dx, lengthFixed);
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
		fixed16_16 V = s_scanlineV0;
		fixed16_16 U = s_scanlineU0;
		fixed16_16 dVdX = s_scanline_dVdX;
		fixed16_16 dUdX = s_scanline_dUdX;

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
		fixed16_16 V = s_scanlineV0;
		fixed16_16 U = s_scanlineU0;
		fixed16_16 dVdX = s_scanline_dVdX;
		fixed16_16 dUdX = s_scanline_dUdX;

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

	// This produces functionally identical results to the original but splits apart the U/V and dUdx/dVdx into seperate variables
	// to account for C vs ASM differences.
	void drawScanlineHQ()
	{
		fixed44_20 V = s_scanlineV0_HQ;
		fixed44_20 U = s_scanlineU0_HQ;
		fixed44_20 dVdX = s_scanline_dVdX_HQ;
		fixed44_20 dUdX = s_scanline_dUdX_HQ;

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

	void drawScanline_FullbrightHQ()
	{
		fixed44_20 V = s_scanlineV0_HQ;
		fixed44_20 U = s_scanlineU0_HQ;
		fixed44_20 dVdX = s_scanline_dVdX_HQ;
		fixed44_20 dUdX = s_scanline_dUdX_HQ;

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
			else if (edge->dyCeil_dx > 0)  // find the left intersection.
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
			else if (edge->dyFloor_dx < 0)  // find the left intersection.
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

	void flat_drawCeiling_HQ(RSector* sector, EdgePair* edges, s32 count)
	{
		fixed48_16 textureOffsetU = fixed48_16(s_cameraPosX) - fixed48_16(sector->ceilOffsetX);
		fixed48_16 textureOffsetV = fixed48_16(sector->ceilOffsetZ) - fixed48_16(s_cameraPosZ);

		fixed48_16 relCeil = sector->ceilingHeight - s_eyeHeight;
		fixed48_16 scaledRelCeil    =  mul16(relCeil, fixed48_16(s_focalLenAspect));
		fixed48_16 cosScaledRelCeil =  mul16(scaledRelCeil, fixed48_16(s_cosYaw));
		fixed48_16 negSinRelCeil    = -mul16(relCeil, fixed48_16(s_sinYaw));
		fixed48_16 sinScaledRelCeil =  mul16(scaledRelCeil, fixed48_16(s_sinYaw));
		fixed48_16 negCosRelCeil    = -mul16(relCeil, fixed48_16(s_cosYaw));

		if (!flat_setTexture(sector->ceilTex)) { return; }

		for (s32 y = s_windowMinY; y <= s_wallMaxCeilY; y++)
		{
			s32 x = s_windowMinX;
			s32 yOffset = y * s_width;
			s32 yShear = s_heightInPixelsBase - s_heightInPixels;
			fixed44_20 yRcp = s_rcp_yMinusHalfHeightHQ[yShear + y + s_height];
			fixed44_20 z = mul20(scaledRelCeil << 4, yRcp);

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
					s_scanlineX0 = left;
					s_scanlineOut = &s_display[left + yOffset];
					fixed44_20 rightClip = intToFixed20(right - s_screenXMid);
					const fixed44_20 worldToTexelScale = fixed44_20(8);

					fixed44_20 v0 = mul20((cosScaledRelCeil<<4ll) - mul20((negSinRelCeil<<4ll), rightClip), yRcp);
					fixed44_20 u0 = mul20((sinScaledRelCeil<<4ll) + mul20((negCosRelCeil<<4ll), rightClip), yRcp);

					s_scanlineV0_HQ = (v0 - (textureOffsetV<<4ll)) * worldToTexelScale;
					s_scanlineU0_HQ = (u0 - (textureOffsetU<<4ll)) * worldToTexelScale;
					s_scanline_dVdX_HQ =  mul20((negSinRelCeil<<4ll), yRcp) * worldToTexelScale;
					s_scanline_dUdX_HQ = -mul20((negCosRelCeil<<4ll), yRcp) * worldToTexelScale;
									   
					s_scanlineLight = computeLighting(fixed16_16(z>>4ll), 0);

					if (s_scanlineLight)
					{
						drawScanlineHQ();
					}
					else
					{
						drawScanline_FullbrightHQ();
					}
				}
			} // while (i < count)
		}
	}

	void flat_drawCeiling(RSector* sector, EdgePair* edges, s32 count)
	{
		fixed16_16 textureOffsetU = s_cameraPosX - sector->ceilOffsetX;
		fixed16_16 textureOffsetV = sector->ceilOffsetZ - s_cameraPosZ;

		fixed16_16 relCeil          =  sector->ceilingHeight - s_eyeHeight;
		fixed16_16 scaledRelCeil    =  mul16(relCeil, s_focalLenAspect);
		fixed16_16 cosScaledRelCeil =  mul16(scaledRelCeil, s_cosYaw);
		fixed16_16 negSinRelCeil    = -mul16(relCeil, s_sinYaw);
		fixed16_16 sinScaledRelCeil =  mul16(scaledRelCeil, s_sinYaw);
		fixed16_16 negCosRelCeil    = -mul16(relCeil, s_cosYaw);

		if (!flat_setTexture(sector->ceilTex)) { return; }

		for (s32 y = s_windowMinY; y <= s_wallMaxCeilY; y++)
		{
			s32 x = s_windowMinX;
			s32 yOffset = y * s_width;
			s32 yShear = s_heightInPixelsBase - s_heightInPixels;
			fixed16_16 yRcp = s_rcp_yMinusHalfHeight[yShear + y + s_height];
			fixed16_16 z = mul16(scaledRelCeil, yRcp);

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
					fixed16_16 rightClip = intToFixed16(right - s_screenXMid);

					fixed16_16 v0 = mul16(cosScaledRelCeil - mul16(negSinRelCeil, rightClip), yRcp);
					s_scanlineV0 = (v0 - textureOffsetV) * 8;

					fixed16_16 u0 = mul16(sinScaledRelCeil + mul16(negCosRelCeil, rightClip), yRcp);
					s_scanlineU0 = (u0 - textureOffsetU) * 8;

					const fixed16_16 worldToTexelScale = fixed16_16(8);
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
		
	// High Quality / Resolution version of flat_drawFloor that handles
	// higher resolutions and higher sub-texel precision.
	void flat_drawFloor_HQ(RSector* sector, EdgePair* edges, s32 count)
	{
		fixed48_16 textureOffsetU = fixed48_16(s_cameraPosX) - fixed48_16(sector->floorOffsetX);
		fixed48_16 textureOffsetV = fixed48_16(sector->floorOffsetZ) - fixed48_16(s_cameraPosZ);

		fixed48_16 relFloor       = sector->floorHeight - s_eyeHeight;
		fixed48_16 scaledRelFloor = mul16(relFloor, fixed48_16(s_focalLenAspect));

		fixed48_16 cosScaledRelFloor =  mul16(scaledRelFloor, fixed48_16(s_cosYaw));
		fixed48_16 negSinRelFloor    = -mul16(relFloor, fixed48_16(s_sinYaw));
		fixed48_16 sinScaledRelFloor =  mul16(scaledRelFloor, fixed48_16(s_sinYaw));
		fixed48_16 negCosRelFloor    = -mul16(relFloor, fixed48_16(s_cosYaw));

		if (!flat_setTexture(sector->floorTex)) { return; }

		for (s32 y = s_wallMinFloorY; y <= s_windowMaxY; y++)
		{
			s32 x = s_windowMinX;
			s32 yOffset = y * s_width;
			s32 yShear = s_heightInPixelsBase - s_heightInPixels;
			fixed44_20 yRcp = s_rcp_yMinusHalfHeightHQ[yShear + y + s_height];
			fixed44_20 z = mul20(scaledRelFloor<<4, yRcp);

			s32 left = 0;
			s32 right = 0;
			for (s32 i = 0; i < count;)
			{
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
					fixed44_20 rightClip = intToFixed20(right - s_screenXMid);
					fixed44_20 worldToTexelScale = fixed44_20(8);

					fixed44_20 v0 = mul20((cosScaledRelFloor<<4ll) - mul20((negSinRelFloor<<4ll), rightClip), yRcp);
					fixed44_20 u0 = mul20((sinScaledRelFloor<<4ll) + mul20((negCosRelFloor<<4ll), rightClip), yRcp);
					s_scanlineV0_HQ = (v0 - (textureOffsetV<<4ll)) * worldToTexelScale;
					s_scanlineU0_HQ = (u0 - (textureOffsetU<<4ll)) * worldToTexelScale;

					s_scanline_dVdX_HQ =  mul20((negSinRelFloor<<4ll), yRcp) * worldToTexelScale;
					s_scanline_dUdX_HQ = -mul20((negCosRelFloor<<4ll), yRcp) * worldToTexelScale;
					s_scanlineLight = computeLighting(fixed16_16(z>>4ll), 0);

					if (s_scanlineLight)
					{
						drawScanlineHQ();
					}
					else
					{
						drawScanline_FullbrightHQ();
					}
				}
			} // while (i < count)
		}
	}
		
	void flat_drawFloor(RSector* sector, EdgePair* edges, s32 count)
	{
		fixed16_16 textureOffsetU = s_cameraPosX - sector->floorOffsetX;
		fixed16_16 textureOffsetV = sector->floorOffsetZ - s_cameraPosZ;

		fixed16_16 relFloor       = sector->floorHeight - s_eyeHeight;
		fixed16_16 scaledRelFloor = mul16(relFloor, s_focalLenAspect);

		fixed16_16 cosScaledRelFloor = mul16(scaledRelFloor, s_cosYaw);
		fixed16_16 negSinRelFloor    =-mul16(relFloor, s_sinYaw);
		fixed16_16 sinScaledRelFloor = mul16(scaledRelFloor, s_sinYaw);
		fixed16_16 negCosRelFloor    =-mul16(relFloor, s_cosYaw);

		if (!flat_setTexture(sector->floorTex)) { return; }

		for (s32 y = s_wallMinFloorY; y <= s_windowMaxY; y++)
		{
			s32 x = s_windowMinX;
			s32 yOffset = y * s_width;
			s32 yShear = s_heightInPixelsBase - s_heightInPixels;
			fixed16_16 yRcp = s_rcp_yMinusHalfHeight[yShear + y + s_height];
			fixed16_16 z = mul16(scaledRelFloor, yRcp);

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
					fixed16_16 rightClip = intToFixed16(right - s_screenXMid);
					fixed16_16 worldToTexelScale = fixed16_16(8);

					fixed16_16 v0 = mul16(cosScaledRelFloor - mul16(negSinRelFloor, rightClip), yRcp);
					fixed16_16 u0 = mul16(sinScaledRelFloor + mul16(negCosRelFloor, rightClip), yRcp);
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
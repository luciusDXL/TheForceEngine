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
	void flat_setup(s32 length, s32 x0, s32 dyFloor_dx, s32 yFloor1, s32 yFloor, s32 dyCeil_dx, s32 yCeil, s32 yCeil1, FlatEdges* flat)
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

	void flat_addEdges(s32 length, s32 x0, s32 dyFloor_dx, s32 yFloor, s32 dyCeil_dx, s32 yCeil)
	{
		if (s_flatCount < MAX_SEG && length > 0)
		{
			const s32 lengthFixed = (length - 1) << 16;

			s32 yCeil1 = yCeil;
			if (dyCeil_dx != 0)
			{
				yCeil1 += mul16(dyCeil_dx, lengthFixed);
			}
			s32 yFloor1 = yFloor;
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
			u8* top = &s_windowTop[x0];
			while (*top > y && x1 >= x0)
			{
				x0++;
				top++;
			}
			*left = x0;
			if (x0 <= x1)
			{
				u8* top = &s_windowTop[x0];
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
			u8* bot = &s_windowBot[x0];
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

	static s32 s_scanlineX0;
	static s32 s_scanlineU0;
	static s32 s_scanlineV0;
	static s32 s_scanline_dUdX;
	static s32 s_scanline_dVdX;
	static s32 s_scanlineWidth;
	static const u8* s_scanlineLight;
	static u8* s_scanlineOut;

	static u8* s_ftexImage;
	static s32 s_ftexDataEnd;
	static s32 s_ftexWidthMask;
	static s32 s_ftexHeightMask;
	static s32 s_ftexHeightLog2;

	void drawScanline()
	{
		// Convert from 16.16 coordinates to 6.10 coordinates -
		// allowing for 64 texels + 10 bits sub-texel resolution.
		// This means we can store both U and V coordinates in a single 32 bit value.
		s32 V = (s_scanlineV0 << 10) & 0xffff0000;
		s32 U = (s_scanlineU0 >> 6) & 0x0000ffff;
		s32 uv = V | U;
		s32 dVdX = (s_scanline_dVdX << 10) && 0xffff0000;
		s32 dUdX = (s_scanline_dUdX >> 6) & 0x0000ffff;
		s32 dUVdX = dVdX | dUdX;

		// bottom 10 bits of code address | top 22 bits of (V|U)
		// v offset = uv >> 26; i.e. top 6 bits (integer part)
		// (uv >> 10) * 64 should probably be ((uv >> 10)&63) * 64 to account for upper part, but the mask at the end fixes it up.
		s32 texel = ((uv >> 10) * 64) | (uv >> 26);
		texel &= s_ftexDataEnd;
		uv += dUVdX;

		for (s32 i = s_scanlineWidth - 1; i >= 0; i--)
		{
			u8 c = s_scanlineLight[s_ftexImage[texel]];
			texel = ((uv >> 10) * 64) | (uv >> 26);
			texel &= s_ftexDataEnd;
			uv += dUVdX;
			s_scanlineOut[i] = c;
		}
	}

	void drawScanline_Fullbright()
	{
		// Convert from 16.16 coordinates to 6.10 coordinates -
		// allowing for 64 texels + 10 bits sub-texel resolution.
		// This means we can store both U and V coordinates in a single 32 bit value.
		s32 V = (s_scanlineV0 << 10) & 0xffff0000;
		s32 U = (s_scanlineU0 >> 6) & 0x0000ffff;
		s32 uv = V | U;
		s32 dVdX = (s_scanline_dVdX << 10) && 0xffff0000;
		s32 dUdX = (s_scanline_dUdX >> 6) & 0x0000ffff;
		s32 dUVdX = dVdX | dUdX;

		// bottom 10 bits of code address | top 22 bits of (V|U)
		// v offset = uv >> 26; i.e. top 6 bits (integer part)
		// (uv >> 10) * 64 should probably be ((uv >> 10)&63) * 64 to account for upper part, but the mask at the end fixes it up.
		s32 texel = ((uv >> 10) * 64) | (uv >> 26);
		texel &= s_ftexDataEnd;
		uv += dUVdX;

		for (s32 i = s_scanlineWidth - 1; i >= 0; i--)
		{
			u8 c = s_ftexImage[texel];
			texel = ((uv >> 10) * 64) | (uv >> 26);
			texel &= s_ftexDataEnd;
			uv += dUVdX;
			s_scanlineOut[i] = c;
		}
	}

	void flat_drawCeiling(RSector* sector, FlatEdges* edges, s32 count)
	{
		s32 textureOffsetU = s_cameraPosX - sector->ceilOffsetX;
		s32 textureOffsetV = sector->ceilOffsetZ - s_cameraPosZ;

		s32 relCeil          =  sector->ceilingHeight - s_eyeHeight;
		s32 scaledRelCeil    =  mul16(relCeil, s_focalLength);
		s32 cosScaledRelCeil =  mul16(scaledRelCeil, s_cosYaw);
		s32 negSinRelCeil    = -mul16(relCeil, s_sinYaw);
		s32 sinScaledRelCeil =  mul16(scaledRelCeil, s_sinYaw);
		s32 negCosRelCeil    = -mul16(relCeil, s_cosYaw);

		TextureFrame* ceilTex = sector->ceilTex;
		s_ftexWidthMask  = ceilTex->width  - 1;
		s_ftexHeightMask = ceilTex->height - 1;
		s_ftexHeightLog2 = ceilTex->logSizeY;
		s_ftexImage = ceilTex->image;
		s_ftexDataEnd = ceilTex->width * ceilTex->height - 1;

		for (s32 y = s_windowMinY; y <= s_wallMaxCeilY; y++)
		{
			s32 x = s_windowMinX;
			s32 colOffset = y * s_width;
			// TODO: setup y shear
			s32 yShear = 0; // s_heightInPixelsConst - s_heightInPixels;
			s32 yRcp   = s_rcp_yMinusHalfHeight[yShear + y];
			s32 z = mul16(scaledRelCeil, yRcp);

			s32 left = 0;
			s32 right = 0;
			s32 i = 0;
			while (i < count)
			{
				s32 winMaxX = s_windowMaxX;
				s32 validScanline = 0;

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
						validScanline = -1;
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
					else  // we have to find the left intersection.
					{
						s32 dydxCeil = edge->dyCeil_dx;
						if (dydxCeil != 0)
						{
							s32 ey;
							if (dydxCeil > 0)
							{
								x = edge->x0;
								ey = s_columnTop[x];
								while (x < winMaxX && y > ey)
								{
									x++;
									ey = s_columnTop[x];
								};
							}
							else  // dydxCeil < 0
							{
								x = edge->x0;
								ey = s_columnTop[x];
								while (x < winMaxX && y >= ey)//ey >= y)
								{
									x++;
									ey = s_columnTop[x];
								};
							}
							left = x;
							x = edges[i].x1 + 1;
							hasLeft = -1;
							i++;
						}
						else
						{
							left = x;
							hasLeft = -1;
							validScanline = -1;
						}
					}
				}  // while (i < count && hasLeft == 0)

				if (i < count)
				{
					s_windowMaxX = winMaxX;

					// Search for the right edge of the scanline.
					while (i < count && hasRight == 0)
					{
						FlatEdges* edge = &edges[i];
						if (y < edge->yPixel_C0)		// Y is above the current edge, so move on to the next edge.
						{
							x = edge->x1 + 1;	//
							i++;
							if (i >= count)
							{
								right = x;
								hasRight = -1;
							}
							s_windowMaxX = winMaxX;
						}
						else if (y >= edge->yPixel_C1)	// Y is below the current edge so it must be the end.
						{
							right = x;
							x = edge->x1 + 1;
							i++;
							hasRight = -1;
							s_windowMaxX = winMaxX;
						}
						else
						{
							if (edge->dyCeil_dx >= 0)
							{
								hasRight = -1;
								right = x;
								s_windowMaxX = x;
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
								i++;
								right = x;
								x = edges[i].x1 + 1;
								s_windowMaxX = winMaxX;
								hasRight = -1;
								break;
							}
						}
					}
				}  // if (i < count)
				else
				{
					//if (validScanline == 0) { break; }
					right = x;
				}

				s_windowMaxX = winMaxX;
				//if (validScanline == 0) { break; }

				clipScanline(&left, &right, y);
				s_scanlineWidth = right - left + 1;
				if (s_scanlineWidth > 0)
				{
					assert(left >= 0 && left + s_scanlineWidth <= 320);
					assert(y >= 0 && y < 200);
					s_scanlineX0 = left;
					s_scanlineOut = &s_display[left + colOffset];
					s32 rightFixed = (right - s_screenXMid) << 16;
					s32 v0 = mul16(cosScaledRelCeil - mul16(negSinRelCeil, rightFixed), yRcp);
					s_scanlineV0 = (v0 - textureOffsetV) * 8;

					s32 u0 = mul16(sinScaledRelCeil + mul16(negCosRelCeil, rightFixed), yRcp);
					s_scanlineU0 = (u0 - textureOffsetU) * 8;

					s_scanline_dVdX =  mul16(negSinRelCeil, yRcp) * 8;
					s_scanline_dUdX = -mul16(negCosRelCeil, yRcp) * 8;
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

	void flat_drawFloor(RSector* sector, FlatEdges* edges, s32 count)
	{
		// TODO
	}
}
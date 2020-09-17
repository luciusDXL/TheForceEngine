#include "rscanline.h"
#include "rcommon.h"
#include <TFE_System/system.h>

namespace TFE_JediRenderer
{
	void clipScanline(s32* left, s32* right, s32 y);

	bool flat_buildScanlineCeiling(s32& i, s32 count, s32& x, s32 y, s32& left, s32& right, s32& scanlineLength, const EdgePair* edges)
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
		scanlineLength = right - left + 1;
		return true;
	}

	bool flat_buildScanlineFloor(s32& i, s32 count, s32& x, s32 y, s32& left, s32& right, s32& scanlineLength, const EdgePair* edges)
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
		scanlineLength = right - left + 1;
		return true;
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
		if (x0 < s_windowMinX) { x0 = s_windowMinX; *left = x0; }
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
}

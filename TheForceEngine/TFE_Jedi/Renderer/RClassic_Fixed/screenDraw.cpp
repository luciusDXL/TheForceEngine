#include <TFE_System/profiler.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>

#include "screenDraw.h"

namespace TFE_Jedi
{
	enum ClipLines
	{
		CLIP_LEFT  = 0x0001,
		CLIP_RIGHT = 0x0010,
		CLIP_TOP   = 0x1000,
		CLIP_BOT   = 0x0100,
	};

	void screen_drawPoint(ScreenRect* rect, s32 x, s32 z, u8 color, u8* framebuffer)
	{
		if (x < rect->left || x > rect->right || z < rect->top || z > rect->bot) { return; }
		framebuffer[z*320+x] = color;
	}

	void screen_drawLine(ScreenRect* rect, s32 x0, s32 z0, s32 x1, s32 z1, u8 color, u8* framebuffer)
	{
		if (!screen_clipLineToRect(rect, &x0, &z0, &x1, &z1))
		{
			return;
		}

		s32 x = x0, z = z0;
		s32 dx = x1 - x;
		s32 dz = z1 - z;
		s32 xDir = (dx > 0) ? 1 : -1;
		s32 zDir = (dz > 0) ? 1 : -1;

		if (xDir < 0) { dz = -dz; }
		if (zDir < 0) { dx = -dx; }

		// Place the first point.
		framebuffer[z0*320 + x0] = color;

		s32 dist = 0;
		while (x != x1 || z != z1)
		{
			s32 absXDist = abs(dx + dist);
			s32 absZDist = abs(dz + dist);
			if (absZDist < absXDist)
			{
				dist += dz;
				x += xDir;
			}
			else
			{
				dist += dx;
				z += zDir;
			}
			framebuffer[z*320 + x] = color;
		}
	}

	void screen_drawCircle(ScreenRect* rect, s32 x, s32 z, s32 r, s32 stepAngle, u8 color, u8* framebuffer)
	{
		s32 rPixel = floor16(r + HALF_16);
		s32 x1 = x + rPixel;
		s32 x0 = x - rPixel;
		s32 z1 = z + rPixel;
		s32 z0 = z - rPixel;
		if (x1 < rect->left || x0 > rect->right || z1 < rect->top  || z0 > rect->bot)
		{
			return;
		}

		fixed16_16 cosAngle, sinAngle;
		sinCosFixed(0, &sinAngle, &cosAngle);
		s32 cx0 = x + floor16(mul16(r, cosAngle) + HALF_16);
		s32 cz0 = z + floor16(mul16(r, sinAngle) + HALF_16);

		s32 curAngle = stepAngle;
		while (curAngle <= 16384)
		{
			sinCosFixed(curAngle, &sinAngle, &cosAngle);
			s32 cx1 = x + floor16(mul16(r, cosAngle) + HALF_16);
			s32 cz1 = z + floor16(mul16(r, sinAngle) + HALF_16);

			screen_drawLine(rect, cx0, cz0, cx1, cz1, color, framebuffer);

			curAngle += stepAngle;
			cx0 = cx1;
			cz0 = cz1;
		}
	}
		
	JBool screen_clipLineToRect(ScreenRect* rect, s32* x0, s32* z0, s32* x1, s32* z1)
	{
		u32 clip0 = 0;
		if (*x0 < rect->left)       { clip0 = CLIP_LEFT;  }
		else if (*x0 > rect->right) { clip0 = CLIP_RIGHT; }
		if (*z0 < rect->top)        { clip0 |= CLIP_TOP;  }
		else if (*z0 > rect->bot)   { clip0 |= CLIP_BOT;  }

		u32 clip1 = 0;
		if (*x1 < rect->left)       { clip1 = CLIP_LEFT;  }
		else if (*x1 > rect->right) { clip1 = CLIP_RIGHT; }
		if (*z1 < rect->top)        { clip1 |= CLIP_TOP;  }
		else if (*z1 > rect->bot)   { clip1 |= CLIP_BOT;  }

		// Triavially accept or reject.
		if (!clip0 && !clip1)
		{
			return JTRUE;
		}
		else if (clip1 & clip0)
		{
			return JFALSE;
		}

		fixed16_16 z0F = intToFixed16(*z0);
		fixed16_16 x1F = intToFixed16(*x1);
		fixed16_16 z1F = intToFixed16(*z1);
		fixed16_16 x0F = intToFixed16(*x0);
		JBool isVert = (x0F == x1F) ? JTRUE : JFALSE;
		JBool isHorz = (z0F == z1F) ? JTRUE : JFALSE;
		while (1)
		{
			u32 clip = clip0 ? clip0 : clip1;
			fixed16_16 dx = x1F - x0F;
			fixed16_16 dz = z1F - z0F;
			fixed16_16 clipX, clipZ;
			if (clip & (CLIP_TOP | CLIP_BOT))
			{
				fixed16_16 clipLine = intToFixed16((clip & CLIP_TOP) ? rect->top : rect->bot);
				if (isVert)
				{
					clipX = x0F;
				}
				else
				{
					clipX = mul16(div16(dx, dz), (clipLine - z0F)) + x0F;
				}
				clipZ = clipLine;
			}
			else if (clip & (CLIP_RIGHT | CLIP_LEFT))
			{
				fixed16_16 clipLine = intToFixed16((clip & CLIP_RIGHT) ? rect->right : rect->left);
				if (isHorz)
				{
					clipZ = z0F;
				}
				else
				{
					clipZ = mul16(div16(dz, dx), (clipLine - x0F)) + z0F;
				}
				clipX = clipLine;
			}
			s32 newX = floor16(clipX);
			s32 newZ = floor16(clipZ);
			if (clip0 == clip)
			{
				x0F = clipX;
				z0F = clipZ;
			}
			else
			{
				x1F = clipX;
				z1F = clipZ;
			}

			u32 newClip = 0;
			if (newX < rect->left)       { newClip = CLIP_LEFT;  }
			else if (newX > rect->right) { newClip = CLIP_RIGHT; }
			if (newZ < rect->top)        { newClip|= CLIP_TOP;   }
			else if (newZ > rect->bot)   { newClip|= CLIP_BOT;   }
			
			if (clip0 == clip) { clip0 = newClip; }
			else               { clip1 = newClip; }

			if (!clip0 && !clip1)
			{
				*x0 = floor16(x0F);
				*z0 = floor16(z0F);
				*x1 = floor16(x1F);
				*z1 = floor16(z1F);
				return JTRUE;
			}
			else if (clip1&clip0)
			{
				return JFALSE;
			}
		}
		return JFALSE;
	}

}  // TFE_Jedi
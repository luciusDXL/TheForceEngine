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

	static u8 s_transColor = 0;

	void screen_drawPoint(ScreenRect* rect, s32 x, s32 z, u8 color, u8* framebuffer)
	{
		if (x < rect->left || x > rect->right || z < rect->top || z > rect->bot) { return; }
		framebuffer[z*vfb_getStride()+x] = color;
	}

	void screen_drawLine(ScreenRect* rect, s32 x0, s32 z0, s32 x1, s32 z1, u8 color, u8* framebuffer)
	{
		if (!screen_clipLineToRect(rect, &x0, &z0, &x1, &z1))
		{
			return;
		}

		const u32 stride = vfb_getStride();
		s32 x = x0, z = z0;
		s32 dx = x1 - x;
		s32 dz = z1 - z;
		s32 xDir = (dx > 0) ? 1 : -1;
		s32 zDir = (dz > 0) ? 1 : -1;

		if (xDir < 0) { dz = -dz; }
		if (zDir > 0) { dx = -dx; }

		// Place the first point.
		framebuffer[z0*stride + x0] = color;

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
			framebuffer[z*stride + x] = color;
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

	/////////////////////////////////////////////////////////
	// Original drawing code assumes 320x200
	/////////////////////////////////////////////////////////

	void textureBlitColumnOpaque(u8* image, u8* outBuffer, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride)
		{
			outBuffer[offset] = image[i];
		}
	}

	void textureBlitColumnTrans(u8* image, u8* outBuffer, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride)
		{
			if (image[i] != s_transColor) { outBuffer[offset] = image[i]; }
		}
	}

	void textureBlitColumnOpaqueRow(u8* image, u8* outBuffer, s32 imageStride, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		s32 yOffset = end * imageStride;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, yOffset -= imageStride)
		{
			outBuffer[offset] = image[yOffset];
		}
	}

	void textureBlitColumnTransRow(u8* image, u8* outBuffer, s32 imageStride, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		s32 yOffset = end * imageStride;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, yOffset -= imageStride)
		{
			if (image[yOffset] != s_transColor) { outBuffer[offset] = image[yOffset]; }
		}
	}

	//////////////////////////////////
	// Image blit functions
	void imageBlitColumnOpaque(u8* image, u8* outBuffer, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, image++)
		{
			outBuffer[offset] = *image;
		}
	}

	void imageBlitColumnTrans(u8* image, u8* outBuffer, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride)
		{
			u8 color = *image;
			if (color != s_transColor) { outBuffer[offset] = color; }
		}
	}

	void imageBlitColumnOpaqueRow(u8* image, u8* outBuffer, s32 imageStride, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		s32 yOffset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, yOffset += imageStride)
		{
			outBuffer[offset] = image[yOffset];
		}
	}

	void imageBlitColumnTransRow(u8* image, u8* outBuffer, s32 imageStride, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		s32 yOffset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, yOffset += imageStride)
		{
			u8 color = image[yOffset];
			if (color != s_transColor) { outBuffer[offset] = color; }
		}
	}

	/////////////////////////////////

	void textureBlitColumnOpaqueLit(u8* image, u8* outBuffer, s32 yPixelCount, const u8* atten)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride)
		{
			outBuffer[offset] = atten[image[i]];
		}
	}

	void textureBlitColumnTransLit(u8* image, u8* outBuffer, s32 yPixelCount, const u8* atten)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride)
		{
			if (image[i] != s_transColor) { outBuffer[offset] = atten[image[i]]; }
		}
	}

	void blitTextureToScreen(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, u8* output, JBool forceTransparency, JBool forceOpaque)
	{
		s32 x1 = x0 + texture->width  - 1;
		s32 y1 = y0 + texture->height - 1;

		// Cull if outside of the draw rect.
		if (x1 < rect->x0 || y1 < rect->y0 || x0 > rect->x1 || y0 > rect->y1) { return; }

		s32 srcX = 0, srcY = 0;
		if (y0 < rect->y0)
		{
			y0 = rect->y0;
		}
		if (y1 > rect->y1)
		{
			srcY = y1 - rect->y1;
			y1 = rect->y1;
		}

		if (x0 < rect->x0)
		{
			srcX = x0 - rect->x0;
			x0 = rect->x0;
		}
		if (x1 > rect->x1)
		{
			x1 = rect->x1;
		}

		s32 yPixelCount = y1 - y0 + 1;
		if (yPixelCount <= 0) { return; }

		u8* buffer = texture->image + texture->height*srcX + srcY;
		const u32 stride = vfb_getStride();
		if (!forceOpaque && ((texture->flags & OPACITY_TRANS) || forceTransparency))
		{
			for (s32 col = x0; col <= x1; col++, buffer += texture->height)
			{
				textureBlitColumnTrans(buffer, output + y0 * stride + col, yPixelCount);
			}
		}
		else
		{
			for (s32 col = x0; col <= x1; col++, buffer += texture->height)
			{
				textureBlitColumnOpaque(buffer, output + y0 * stride + col, yPixelCount);
			}
		}
	}

	void blitTextureToScreenLit(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, const u8* atten, u8* output, JBool forceTransparency)
	{
		s32 x1 = x0 + texture->width - 1;
		s32 y1 = y0 + texture->height - 1;

		// Cull if outside of the draw rect.
		if (x1 < rect->x0 || y1 < rect->y0 || x0 > rect->x1 || y0 > rect->y1) { return; }

		s32 srcX = 0, srcY = 0;
		if (y0 < rect->y0)
		{
			y0 = rect->y0;
		}
		if (y1 > rect->y1)
		{
			srcY = y1 - rect->y1;
			y1 = rect->y1;
		}

		if (x0 < rect->x0)
		{
			srcX = x0 - rect->x0;
			x0 = rect->x0;
		}
		if (x1 > rect->x1)
		{
			x1 = rect->x1;
		}

		s32 yPixelCount = y1 - y0 + 1;
		if (yPixelCount <= 0) { return; }

		u8* buffer = texture->image + texture->height*srcX + srcY;
		const u32 stride = vfb_getStride();
		if ((texture->flags & OPACITY_TRANS) || forceTransparency)
		{
			for (s32 col = x0; col <= x1; col++, buffer += texture->height)
			{
				textureBlitColumnTransLit(buffer, output + y0 * stride + col, yPixelCount, atten);
			}
		}
		else
		{
			for (s32 col = x0; col <= x1; col++, buffer += texture->height)
			{
				textureBlitColumnOpaqueLit(buffer, output + y0 * stride + col, yPixelCount, atten);
			}
		}
	}

	/////////////////////////////////////////////////////////
	// The "scaled" variants allow for scaling.
	/////////////////////////////////////////////////////////
	void textureBlitColumnOpaqueScaled(u8* image, u8* outBuffer, s32 yPixelCount, fixed16_16 vCoord, fixed16_16 vStep)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, vCoord += vStep)
		{
			s32 v = floor16(vCoord);
			outBuffer[offset] = image[v];
		}
	}

	void textureBlitColumnTransScaled(u8* image, u8* outBuffer, s32 yPixelCount, fixed16_16 vCoord, fixed16_16 vStep)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, vCoord += vStep)
		{
			s32 v = floor16(vCoord);
			if (image[v]) { outBuffer[offset] = image[v]; }
		}
	}

	void textureBlitColumnTransIScaled(u8* image, u8* outBuffer, s32 yPixelCount, s32 scale, s32 v0)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		s32 vStep = 0;
		s32 v = v0;
		for (s32 i = end; i >= 0; i--, offset += stride)
		{
			if (image[v]) { outBuffer[offset] = image[v]; }

			vStep++;
			if (vStep >= scale)
			{
				vStep = 0;
				v--;
			}
		}
	}

	void textureBlitColumnOpaqueLitScaled(u8* image, u8* outBuffer, s32 yPixelCount, const u8* atten, fixed16_16 vCoord, fixed16_16 vStep)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, vCoord += vStep)
		{
			s32 v = floor16(vCoord);
			outBuffer[offset] = atten[image[v]];
		}
	}

	void textureBlitColumnTransLitScaled(u8* image, u8* outBuffer, s32 yPixelCount, const u8* atten, fixed16_16 vCoord, fixed16_16 vStep)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, vCoord += vStep)
		{
			s32 v = floor16(vCoord);
			if (image[v]) { outBuffer[offset] = atten[image[v]]; }
		}
	}

	void textureBlitColumnOpaqueScaledRow(u8* image, u8* outBuffer, s32 yPixelCount, s32 imageStride, fixed16_16 vCoord, fixed16_16 vStep)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, vCoord += vStep)
		{
			s32 v = floor16(vCoord)*imageStride;
			outBuffer[offset] = image[v];
		}
	}
		
	void screenDraw_setTransColor(u8 color)
	{
		s_transColor = color;
	}

	void textureBlitColumnTransScaledRow(u8* image, u8* outBuffer, s32 yPixelCount, s32 imageStride, fixed16_16 vCoord, fixed16_16 vStep)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, vCoord += vStep)
		{
			s32 v = floor16(vCoord)*imageStride;
			if (image[v] != s_transColor) { outBuffer[offset] = image[v]; }
		}
	}

	void textureBlitColumnOpaqueLitScaledRow(u8* image, u8* outBuffer, s32 yPixelCount, s32 imageStride, const u8* atten, fixed16_16 vCoord, fixed16_16 vStep)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, vCoord += vStep)
		{
			s32 v = floor16(vCoord)*imageStride;
			outBuffer[offset] = atten[image[v]];
		}
	}

	void textureBlitColumnTransLitScaledRow(u8* image, u8* outBuffer, s32 yPixelCount, s32 imageStride, const u8* atten, fixed16_16 vCoord, fixed16_16 vStep)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		const u32 stride = vfb_getStride();
		for (s32 i = end; i >= 0; i--, offset += stride, vCoord += vStep)
		{
			s32 v = floor16(vCoord)*imageStride;
			if (image[v]) { outBuffer[offset] = atten[image[v]]; }
		}
	}

	void blitTextureToScreenScaled(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale, u8* output, JBool forceTransparency)
	{
		ScreenImage image =
		{
			texture->width,
			texture->height,
			texture->image,
			(forceTransparency || (texture->flags & OPACITY_TRANS)) ? JTRUE : JFALSE,
			JTRUE
		};
		blitTextureToScreenScaled(&image, rect, x0, y0, xScale, yScale, output);
	}

	void blitTextureToScreenIScale(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, s32 scale, u8* output)
	{
		s32 x1 = x0 + (texture->width  - 1)*scale;
		s32 y1 = y0 + (texture->height - 1)*scale;
		s32 yPixelCount = y1 - y0 + 1;
		if (yPixelCount <= 0) { return; }

		s32 uStep = scale;
		s32 u = 0;
		const u32 stride = vfb_getStride();
		for (s32 col = x0; col <= x1; col++)
		{
			u8* buffer = texture->image + u*texture->height;
			textureBlitColumnTransIScaled(buffer, output + y0 * stride + col, yPixelCount, scale, texture->height - 1);

			uStep++;
			if (uStep >= scale)
			{
				uStep = 0;
				u++;
			}
		}
	}

	void blitTextureToScreen(ScreenImage* texture, DrawRect* rect, s32 x0, s32 y0, u8* output)
	{
		s32 x1 = x0 + texture->width - 1;
		s32 y1 = y0 + texture->height - 1;

		// Cull if outside of the draw rect.
		if (x1 < rect->x0 || y1 < rect->y0 || x0 > rect->x1 || y0 > rect->y1) { return; }

		s32 srcX = 0, srcY = 0;
		if (y0 < rect->y0)
		{
			y0 = rect->y0;
		}
		if (y1 > rect->y1)
		{
			srcY = y1 - rect->y1;
			y1 = rect->y1;
		}

		if (x0 < rect->x0)
		{
			srcX = x0 - rect->x0;
			x0 = rect->x0;
		}
		if (x1 > rect->x1)
		{
			x1 = rect->x1;
		}

		s32 yPixelCount = y1 - y0 + 1;
		if (yPixelCount <= 0) { return; }

		const u32 stride = vfb_getStride();
		output += y0 * stride;
		if (texture->columnOriented)
		{
			u8* buffer = texture->image + texture->height*srcX + srcY;
			if (texture->trans)
			{
				for (s32 col = x0; col <= x1; col++, buffer += texture->height)
				{
					imageBlitColumnTrans(buffer, output + col, yPixelCount);
				}
			}
			else
			{
				for (s32 col = x0; col <= x1; col++, buffer += texture->height)
				{
					imageBlitColumnOpaque(buffer, output + col, yPixelCount);
				}
			}
		}
		else
		{
			u8* buffer = texture->image + texture->width*srcY + srcX;
			if (texture->trans)
			{
				for (s32 col = x0; col <= x1; col++, buffer++)
				{
					imageBlitColumnTransRow(buffer, output + col, texture->width, yPixelCount);
				}
			}
			else
			{
				for (s32 col = x0; col <= x1; col++, buffer++)
				{
					imageBlitColumnOpaqueRow(buffer, output + col, texture->width, yPixelCount);
				}
			}
		}
	}

	void blitTextureToScreenScaled(ScreenImage* texture, DrawRect* rect, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale, u8* output)
	{
		s32 x1 = x0 + floor16(mul16(intToFixed16(texture->width - 1),  xScale));
		s32 y1 = y0 + floor16(mul16(intToFixed16(texture->height - 1), yScale));
		fixed16_16 u0 = 0, v1 = 0;
		fixed16_16 u1 = intToFixed16(texture->width  - 1);
		fixed16_16 v0 = intToFixed16(texture->height - 1);
		fixed16_16 uStep =  div16(intToFixed16(texture->width),  intToFixed16(x1 - x0 + 1));
		fixed16_16 vStep = -div16(intToFixed16(texture->height), intToFixed16(y1 - y0 + 1));

		if (!texture->columnOriented)
		{
			swap(v0, v1);
			vStep = -vStep;
		}

		// Cull if outside of the draw rect.
		if (x1 < rect->x0 || y1 < rect->y0 || x0 > rect->x1 || y0 > rect->y1) { return; }

		if (y0 < rect->y0)
		{
			v0 += vStep * (rect->y0 - y0);
			y0 = rect->y0;
		}
		if (y1 > rect->y1)
		{
			y1 = rect->y1;
		}

		if (x0 < rect->x0)
		{
			u0 += uStep * (rect->x0 - x0);
			x0 = rect->x0;
		}
		if (x1 > rect->x1)
		{
			x1 = rect->x1;
		}

		s32 yPixelCount = y1 - y0 + 1;
		if (yPixelCount <= 0) { return; }

		const u32 stride = vfb_getStride();
		if (texture->columnOriented)
		{
			fixed16_16 u = u0;
			if (texture->trans)
			{
				for (s32 col = x0; col <= x1; col++, u += uStep)
				{
					u8* buffer = texture->image + floor16(u)*texture->height;
					textureBlitColumnTransScaled(buffer, output + y0 * stride + col, yPixelCount, v0, vStep);
				}
			}
			else
			{
				for (s32 col = x0; col <= x1; col++, u += uStep)
				{
					u8* buffer = texture->image + floor16(u)*texture->height;
					textureBlitColumnOpaqueScaled(buffer, output + y0 * stride + col, yPixelCount, v0, vStep);
				}
			}
		}
		else
		{
			const s32 imageStride = texture->width;
			fixed16_16 u = u0;
			if (texture->trans)
			{
				for (s32 col = x0; col <= x1; col++, u += uStep)
				{
					u8* buffer = texture->image + floor16(u);
					textureBlitColumnTransScaledRow(buffer, output + y0 * stride + col, yPixelCount, imageStride, v0, vStep);
				}
			}
			else
			{
				for (s32 col = x0; col <= x1; col++, u += uStep)
				{
					u8* buffer = texture->image + floor16(u);
					textureBlitColumnOpaqueScaledRow(buffer, output + y0 * stride + col, yPixelCount, imageStride, v0, vStep);
				}
			}
		}
	}

	void blitTextureToScreenLitScaled(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale, const u8* atten, u8* output, JBool forceTransparency)
	{
		ScreenImage image = 
		{
			texture->width,
			texture->height,
			texture->image,
			forceTransparency || (texture->flags & OPACITY_TRANS) ? JTRUE : JFALSE,
			JTRUE
		};
		blitTextureToScreenLitScaled(&image, rect, x0, y0, xScale, yScale, atten, output);
	}

	void blitTextureToScreenLitScaled(ScreenImage* texture, DrawRect* rect, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale, const u8* atten, u8* output)
	{
		s32 x1 = x0 + floor16(mul16(intToFixed16(texture->width - 1), xScale));
		s32 y1 = y0 + floor16(mul16(intToFixed16(texture->height - 1), yScale));
		fixed16_16 u0 = 0, v1 = 0;
		fixed16_16 u1 = intToFixed16(texture->width - 1);
		fixed16_16 v0 = intToFixed16(texture->height - 1);
		fixed16_16 uStep =  div16(intToFixed16(texture->width - 1), intToFixed16(x1 - x0));
		fixed16_16 vStep = -div16(intToFixed16(texture->height - 1), intToFixed16(y1 - y0));

		if (!texture->columnOriented)
		{
			swap(v0, v1);
			vStep = -vStep;
		}

		// Cull if outside of the draw rect.
		if (x1 < rect->x0 || y1 < rect->y0 || x0 > rect->x1 || y0 > rect->y1) { return; }

		if (y0 < rect->y0)
		{
			v0 += vStep * (rect->y0 - y0);
			y0 = rect->y0;
		}
		if (y1 > rect->y1)
		{
			y1 = rect->y1;
		}

		if (x0 < rect->x0)
		{
			u0 += uStep * (rect->x0 - x0);
			x0 = rect->x0;
		}
		if (x1 > rect->x1)
		{
			x1 = rect->x1;
		}

		s32 yPixelCount = y1 - y0 + 1;
		if (yPixelCount <= 0) { return; }

		const u32 stride = vfb_getStride();
		if (texture->columnOriented)
		{
			fixed16_16 u = u0;
			if (texture->trans)
			{
				for (s32 col = x0; col <= x1; col++, u += uStep)
				{
					u8* buffer = texture->image + floor16(u)*texture->height;
					textureBlitColumnTransLitScaled(buffer, output + y0 * stride + col, yPixelCount, atten, v0, vStep);
				}
			}
			else
			{
				for (s32 col = x0; col <= x1; col++, u += uStep)
				{
					u8* buffer = texture->image + floor16(u)*texture->height;
					textureBlitColumnOpaqueLitScaled(buffer, output + y0 * stride + col, yPixelCount, atten, v0, vStep);
				}
			}
		}
		else
		{
			const s32 imageStride = texture->width;
			fixed16_16 u = u0;
			if (texture->trans)
			{
				for (s32 col = x0; col <= x1; col++, u += uStep)
				{
					u8* buffer = texture->image + floor16(u);
					textureBlitColumnTransLitScaledRow(buffer, output + y0 * stride + col, yPixelCount, imageStride, atten, v0, vStep);
				}
			}
			else
			{
				for (s32 col = x0; col <= x1; col++, u += uStep)
				{
					u8* buffer = texture->image + floor16(u);
					textureBlitColumnOpaqueLitScaledRow(buffer, output + y0 * stride + col, yPixelCount, imageStride, atten, v0, vStep);
				}
			}
		}
	}

}  // TFE_Jedi
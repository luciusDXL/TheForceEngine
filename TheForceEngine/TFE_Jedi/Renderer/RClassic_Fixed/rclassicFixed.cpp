#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "rcommonFixed.h"
#include "rlightingFixed.h"
#include "rflatFixed.h"
#include "../redgePair.h"
#include "rsectorFixed.h"
#include "../rcommon.h"

namespace TFE_Jedi
{

namespace RClassic_Fixed
{
	static JBool s_fullDetail = JTRUE;
	static fixed16_16 s_viewWidthFixed;
	static fixed16_16 s_viewHeightFixed;
	static fixed16_16 s_minScreenX_Fixed;
	static fixed16_16 s_maxScreenX_Fixed;
	static fixed16_16 s_windowMinYFixed;
	static fixed16_16 s_windowMaxYFixed;
	static fixed16_16 s_screenWidthFract;
	static fixed16_16 s_oneOverWidthFract;
	static fixed16_16 s_worldX;
	static fixed16_16 s_worldY;
	static fixed16_16 s_worldZ;
	static fixed16_16 s_xOffset;
	static fixed16_16 s_zOffset;
	static angle14_32 s_worldYaw;
	static angle14_32 s_maxPitch = 8192;
	static s32 s_screenWidth;
	static s32 s_screenHeight;
	static s32 s_pixelCount;
	static s32 s_visionEffect;
	static u32 s_pixelMask;
	static RSector* s_sector;
	
	void computeSkyOffsets();

	void setVisionEffect(s32 effect)
	{
		s_visionEffect = effect;
	}

	void transformPointByCamera(vec3_fixed* worldPoint, vec3_fixed* viewPoint)
	{
		viewPoint->x = mul16(worldPoint->x, RClassic_Fixed::s_cosYaw_Fixed) + mul16(worldPoint->z, RClassic_Fixed::s_sinYaw_Fixed) + RClassic_Fixed::s_xCameraTrans_Fixed;
		viewPoint->y = worldPoint->y - RClassic_Fixed::s_eyeHeight_Fixed;
		viewPoint->z = mul16(worldPoint->z, RClassic_Fixed::s_cosYaw_Fixed) + mul16(worldPoint->x, RClassic_Fixed::s_negSinYaw_Fixed) + RClassic_Fixed::s_zCameraTrans_Fixed;
	}

#if 0
	void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId)
	{
		// TODO: Remap input pitch to match Dark Forces limits, it should range from -1.5315f, 1.5315f (scale 1.95)

		// Convert camera parameters to fixed point.
		const fixed16_16 yawFixed = floatAngleToFixed(yaw);
		const fixed16_16 pchFixed = floatAngleToFixed(pitch);

		const fixed16_16 cosYaw = floatToFixed16(cosf(yaw));
		const fixed16_16 sinYaw = floatToFixed16(sinf(yaw));
		const fixed16_16 sinPitch = floatToFixed16(sinf(pitch));

		const fixed16_16 xFixed = floatToFixed16(x);
		const fixed16_16 yFixed = floatToFixed16(y);
		const fixed16_16 zFixed = floatToFixed16(z);

		s_cosYaw_Fixed = cosYaw;
		s_sinYaw_Fixed = sinYaw;
		s_negSinYaw_Fixed = -sinYaw;
		s_cameraYaw_Fixed = yawFixed;
		s_cameraPitch_Fixed = pchFixed;

		s_zCameraTrans_Fixed = mul16(-zFixed, s_cosYaw_Fixed) + mul16(-xFixed, s_negSinYaw_Fixed);
		s_xCameraTrans_Fixed = mul16(-xFixed, s_cosYaw_Fixed) + mul16(-zFixed, s_sinYaw_Fixed);
		s_eyeHeight_Fixed = yFixed;

		s_cameraPosX_Fixed = xFixed;
		s_cameraPosY_Fixed = yFixed;
		s_cameraPosZ_Fixed = zFixed;

		s_cameraMtx_Fixed[0] = s_cosYaw_Fixed;
		s_cameraMtx_Fixed[2] = s_negSinYaw_Fixed;
		s_cameraMtx_Fixed[4] = ONE_16;
		s_cameraMtx_Fixed[6] = s_sinYaw_Fixed;
		s_cameraMtx_Fixed[8] = s_cosYaw_Fixed;
		
		const fixed16_16 pitchOffset = mul16(sinPitch, s_focalLenAspect_Fixed);
		s_halfHeight_Fixed = s_halfHeightBase_Fixed + pitchOffset;
		s_screenYMid = s_screenYMidBase + floor16(pitchOffset);
		computeSkyOffsets();

		fixed16_16 yMaxFixed = (s_screenYMidBase - 2) << 16;	// should be 198
		s_yPlaneBot_Fixed =  div16((yMaxFixed >> 1) - pitchOffset, s_focalLength_Fixed);
		s_yPlaneTop_Fixed = -div16((yMaxFixed >> 1) + pitchOffset, s_focalLength_Fixed);

		vec3_fixed worldPoint = { 0, 0, 0 };
		vec3_fixed viewPoint;
		transformPointByCamera(&worldPoint, &viewPoint);

		// Setup the camera lights
		for (s32 i = 0; i < s_lightCount; i++)
		{
			vec3_fixed* srcDir = &s_cameraLight[i].lightWS;
			vec3_fixed* dstDir = &s_cameraLight[i].lightVS;

			vec3_fixed dst;
			transformPointByCamera(srcDir, &dst);

			dstDir->x = viewPoint.x - dst.x;
			dstDir->y = viewPoint.y - dst.y;
			dstDir->z = viewPoint.z - dst.z;

			normalizeVec3(dstDir, dstDir);
		}
	}
#endif

	void setCameraWorldPos(fixed16_16 x, fixed16_16 z, fixed16_16 eyeHeight)
	{
		s_worldX = x;
		s_worldZ = z;
		s_worldY = eyeHeight;
	}
			
	void computeCameraTransform(RSector* sector, angle14_32 pitch, angle14_32 yaw, fixed16_16 camX, fixed16_16 camY, fixed16_16 camZ)
	{
		s_cameraPosX_Fixed = camX;
		s_cameraPosZ_Fixed = camZ;
		s_eyeHeight_Fixed = camY;

		s_sector = sector;

		s_cameraYaw_Fixed = yaw;
		s_cameraPitch_Fixed = pitch;

		s_xOffset = -camX;
		s_zOffset = -camZ;
		sinCosFixed(-yaw, &s_sinYaw_Fixed, &s_cosYaw_Fixed);

		s_negSinYaw_Fixed = -s_sinYaw_Fixed;
		if (s_maxPitch != s_cameraPitch_Fixed)
		{
			fixed16_16 sinPitch   = sinFixed(s_cameraPitch_Fixed);
			fixed16_16 rHalfWidth = mul16(sinPitch, s_halfWidth_Fixed);
			s_projOffsetY = s_projOffsetYBase + rHalfWidth;
			s_screenYMid = s_screenYMidBase + floor16(rHalfWidth);

			// yMax*0.5 / halfWidth; ~pixel Aspect
			s_yPlaneBot_Fixed =  div16((s_viewHeightFixed >> 1) - rHalfWidth, s_halfWidth_Fixed);
			s_yPlaneTop_Fixed = -div16((s_viewHeightFixed >> 1) + rHalfWidth, s_halfWidth_Fixed);
		}

		s_zCameraTrans_Fixed = mul16(s_zOffset, s_cosYaw_Fixed) + mul16(s_xOffset, s_negSinYaw_Fixed);
		s_xCameraTrans_Fixed = mul16(s_xOffset, s_cosYaw_Fixed) + mul16(s_zOffset, s_sinYaw_Fixed);
		setCameraWorldPos(s_cameraPosX_Fixed, s_cameraPosZ_Fixed, s_eyeHeight_Fixed);
		s_worldYaw = s_cameraYaw_Fixed;

		// Camera Transform:
		s_cameraMtx_Fixed[0] = s_cosYaw_Fixed;
		s_cameraMtx_Fixed[2] = s_negSinYaw_Fixed;
		s_cameraMtx_Fixed[6] = s_sinYaw_Fixed;
		s_cameraMtx_Fixed[8] = s_cosYaw_Fixed;
	}

	void computeSkyOffsets()
	{
		fixed16_16 parallax[2];
		TFE_Jedi::getSkyParallax(&parallax[0], &parallax[1]);

		// angles range from -16384 to 16383; multiply by 4 to convert to [-1, 1) range.
		s_skyYawOffset_Fixed = mul16(s_cameraYaw_Fixed * ANGLE_TO_FIXED_SCALE, parallax[0]);
		s_skyPitchOffset_Fixed = -mul16(s_cameraPitch_Fixed * ANGLE_TO_FIXED_SCALE, parallax[1]);
	}
		
	void setupScreenParameters(s32 w, s32 h, s32 x0, s32 y0)	// eax, edx, ebx, ecx
	{
		s32 pixelCount = w * h;
		s_viewWidthFixed  = intToFixed16(w);
		s_viewHeightFixed = intToFixed16(h);
		s_screenWidth  = w;
		s_screenHeight = h;
		s_minScreenX = x0;
		s_minScreenX_Fixed = intToFixed16(x0);
		s_maxScreenX = x0 + w - 1;
		s_maxScreenX_Fixed = intToFixed16(s_maxScreenX);
		s_minScreenY = y0;
		s_windowMinYFixed = intToFixed16(y0);
		s_windowMaxYFixed = intToFixed16(y0 + h - 1);
		s_fullDetail = JTRUE;

		s_maxScreenY = y0 + h - 1;
		s_pixelCount = pixelCount;
	}
		
	void setupProjectionParameters(fixed16_16 halfWidthFixed, s32 xc, s32 yc)
	{
		s_halfWidth_Fixed = halfWidthFixed;
		s_screenXMid = xc;
		s_screenYMid = yc;
		s_screenYMidBase = yc;

		s_projOffsetX = intToFixed16(xc);
		s_projOffsetY = intToFixed16(yc);
		s_projOffsetYBase = s_projOffsetY;

		s_oneOverHalfWidth = div16(ONE_16, halfWidthFixed);

		// TFE
		s_focalLength_Fixed = s_halfWidth_Fixed;
		s_focalLenAspect_Fixed = s_halfWidth_Fixed;
	}

	void setWidthFraction(fixed16_16 widthFract)
	{
		s_screenWidthFract = widthFract;
		s_oneOverWidthFract = div16(ONE_16, widthFract);
	}

	void buildRcpYTable()
	{
		s32 yMid = s_screenYMid;
		for (s32 yOffset = -400; yOffset < 400; yOffset++)
		{
			if (yOffset == yMid)
			{
				s_rcpY[yMid + 400] = ONE_16;
			}
			else
			{
				s_rcpY[yOffset + 400] = div16(ONE_16, intToFixed16(yOffset - yMid));
			}
		}
	}

	void buildProjectionTables(s32 xc, s32 yc, s32 w, s32 h)
	{
		s32 halfHeight = h >> 1;
		s32 halfWidth  = w >> 1;

		setupScreenParameters(w, h, xc - halfWidth, yc - halfHeight);
		setupProjectionParameters(intToFixed16(halfWidth), xc, yc);

		s32 widthFract = div16(intToFixed16(w), FIXED(320));
		setWidthFraction(widthFract);

		EdgePair* flatEdge = &s_flatEdgeList[s_flatCount];
		s_flatEdge = flatEdge;

		flat_addEdges(s_screenWidth, s_minScreenX, 0, s_windowMaxYFixed, 0, s_windowMinYFixed);
		
		s_columnTop = (s32*)realloc(s_columnTop, s_width * sizeof(s32));
		s_columnBot = (s32*)realloc(s_columnBot, s_width * sizeof(s32));
		s_depth1d_all_Fixed = (fixed16_16*)realloc(s_depth1d_all_Fixed, s_width * sizeof(fixed16_16) * (MAX_ADJOIN_DEPTH + 1));
		s_windowTop_all = (s32*)realloc(s_windowTop, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));
		s_windowBot_all = (s32*)realloc(s_windowBot, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));

		memset(s_windowTop_all, s_minScreenY, 320);
		memset(s_windowBot_all, s_maxScreenY, 320);

		// Build tables
		s_column_Z_Over_X = (fixed16_16*)realloc(s_column_Z_Over_X, s_width * sizeof(fixed16_16));
		s_column_X_Over_Z = (fixed16_16*)realloc(s_column_X_Over_Z, s_width * sizeof(fixed16_16));
		s_skyTable_Fixed = (fixed16_16*)realloc(s_skyTable_Fixed, s_width * sizeof(fixed16_16));

		// Here we assume a 90 degree field of view, this forms a frustum (not drawn to scale):
		//     W = width of plane in pixels
		// *---*---*
		//  \ Z|  /  Z = depth of plane
		//   \ | /
		//    \|/
		//     *
		// Where the distance to the screen where the half width is 0.5*W is also 0.5*W
		// i.e. Z = 0.5*W.
		// Z/X in screenspace = W / (x_in_pixels - 0.5*width_in_pixels)
		s32 xMid = s_screenXMid;
		for (s32 i = 0, x = 0; x < 320; x++, i++)
		{
			fixed16_16 xPos = intToFixed16(x - xMid);
			s_column_Z_Over_X[i] = (xMid != x) ? div16(s_halfWidth_Fixed, xPos) : s_halfWidth_Fixed;
		}

		for (s32 i = 0, x = 0; x < 320; i++, x++)
		{
			if (x != xMid)
			{
				fixed16_16 xPos = intToFixed16(x - xMid);
				s_column_X_Over_Z[i] = div16(xPos, s_halfWidth_Fixed);
			}
			else
			{
				s_column_X_Over_Z[i] = 0;
			}
		}

		s_rcpY = (fixed16_16*)realloc(s_rcpY, 4 * s_height * sizeof(fixed16_16));
		buildRcpYTable();
	}

	void computeSkyTable()
	{
		fixed16_16 parallax0, parallax1;
		TFE_Jedi::getSkyParallax(&parallax0, &parallax1);

		s32 xMid = s_screenXMid;
		s_skyTable_Fixed[0] = 0;
		for (s32 i = 0, x = 0; x < 320; i++, x++)
		{
			s32 xPos = x - xMid;
			// This is atanf(float(x) / 160.0f) / (2pi)
			f32 angleFractF = atanf(f32(xPos) / 160.0f) * 2607.595f;
			fixed16_16 angleFract = fixed16_16(angleFractF) << 2;

			// This intentionally overflows when x = 0 and becomes 0...
			s_skyTable_Fixed[1 + i] = mul16(angleFract, parallax0);
		}
	}

	void setIdentityMatrix(s32* mtx)
	{
		mtx[0] = ONE_16;
		mtx[3] = 0;
		mtx[6] = 0;

		mtx[1] = 0;
		mtx[4] = ONE_16;
		mtx[7] = 0;

		mtx[2] = 0;
		mtx[5] = 0;
		mtx[8] = ONE_16;
	}

	void createLight(CameraLight* light, fixed16_16 x, fixed16_16 y, fixed16_16 z, fixed16_16 brightness)
	{
		vec3_fixed* dirWS = &light->lightWS;
		dirWS->x = x;
		dirWS->y = y;
		dirWS->z = z;
		light->brightness = brightness;
		normalizeVec3(dirWS, dirWS);
	}

	void setupInitCameraAndLights()
	{
		s_width  = 320;
		s_height = 200;

		buildProjectionTables(160, 100, 320, 198);
		TFE_Jedi::setSkyParallax(FIXED(1024), FIXED(1024));
		computeSkyTable();

		setIdentityMatrix(s_cameraMtx_Fixed);
		computeCameraTransform(nullptr, 0, 0, 0, 0, 0);

		s_lightCount = 0;

		createLight(&s_cameraLight[0], 0, 0, ONE_16, ONE_16);
		s_lightCount++;

		createLight(&s_cameraLight[1], 0, ONE_16, 0, ONE_16);
		s_lightCount++;

		createLight(&s_cameraLight[2], ONE_16, 0, 0, ONE_16);
		s_lightCount++;

		s_colorMap = nullptr;
		s_lightSourceRamp = nullptr;
		s_enableFlatShading = JFALSE;
		s_cameraLightSource = JFALSE;
		s_pixelMask = 0xffffffff;
		s_visionEffect = 0;
	}

	void setResolution(s32 width, s32 height)
	{
		if (width == s_width && height == s_height) { return; }

		s_width = width;
		s_height = height;
		s_halfWidth_Fixed = intToFixed16(s_width >> 1);
		s_halfHeight_Fixed = intToFixed16(s_height >> 1);
		s_halfHeightBase_Fixed = s_halfHeight_Fixed;
		s_focalLength_Fixed = s_halfWidth_Fixed;
		s_focalLenAspect_Fixed = s_focalLength_Fixed;
		s_screenXMid = s_width >> 1;

		s_screenYMid = s_height;
		s_screenYMidBase = s_height;

		s_minScreenX = 0;
		s_maxScreenX = s_width - 1;
		s_minScreenY = 1;
		s_maxScreenY = s_height - 1;
		s_windowMinZ_Fixed = 0;

		s_windowX0 = s_minScreenX;
		s_windowX1 = s_maxScreenX;

		s_columnTop = (s32*)realloc(s_columnTop, s_width * sizeof(s32));
		s_columnBot = (s32*)realloc(s_columnBot, s_width * sizeof(s32));
		s_depth1d_all_Fixed = (fixed16_16*)realloc(s_depth1d_all_Fixed, s_width * sizeof(fixed16_16) * (MAX_ADJOIN_DEPTH + 1));
		s_windowTop_all = (s32*)realloc(s_windowTop, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));
		s_windowBot_all = (s32*)realloc(s_windowBot, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));

		// Build tables
		s_column_Z_Over_X = (fixed16_16*)realloc(s_column_Z_Over_X, s_width * sizeof(fixed16_16));
		s_column_X_Over_Z = (fixed16_16*)realloc(s_column_X_Over_Z, s_width * sizeof(fixed16_16));
		s_skyTable_Fixed = (fixed16_16*)realloc(s_skyTable_Fixed, s_width * sizeof(fixed16_16));
		s32 halfWidth = s_width >> 1;
		for (s32 x = 0; x < s_width; x++)
		{
			s_column_Z_Over_X[x] = (x != halfWidth) ? div16(s_halfWidth_Fixed, intToFixed16(x - halfWidth)) : s_halfWidth_Fixed;
			s_column_X_Over_Z[x] = div16(intToFixed16(x - halfWidth), s_halfWidth_Fixed);

			// This result isn't *exactly* the same as the original, but it is accurate to within 1 decimal point (example: -88.821585 vs -88.8125 @ x=63)
			// The original result is likely from a arc tangent table, which is more approximate. However the end difference is less
			// than a single pixel at 320x200. The more accurate result will look better at higher resolutions as well. :)
			// TODO: Extract the original table to use at 320x200?
			s_skyTable_Fixed[x] = floatToFixed16(512.0f * atanf(f32(x - halfWidth) / f32(halfWidth)) / PI);
		}

		s_rcpY = (fixed16_16*)realloc(s_rcpY, 4 * s_height * sizeof(fixed16_16));
		s32 yMid = s_height / 2;
		s32 startY = -2 * s_height;
		s32 endY = 2 * s_height;
		for (s32 y = startY; y < endY; y++)
		{
			s_rcpY[y + startY] = (y != yMid) ? div16(ONE_16, (y - yMid)) : ONE_16;
		}
	}

	// 2D
	void textureBlitColumn(const u8* image, u8* outBuffer, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width)
		{
			outBuffer[offset] = image[i];
		}
	}

	void blitTextureToScreen(TextureData* texture, s32 x0, s32 y0)
	{
		s32 x1 = x0 + texture->width - 1;
		s32 y1 = y0 + texture->height - 1;

		if (x0 < 0)
		{
			x0 = 0;
		}
		if (y0 < 0)
		{
			y0 = 0;
		}
		if (x1 > s_width - 1)
		{
			x1 = s_width - 1;
		}
		if (y1 > s_height - 1)
		{
			y1 = s_height - 1;
		}
		s32 yPixelCount = y1 - y0 + 1;

		const u8* buffer = texture->image;
		for (s32 x = x0; x <= x1; x++, buffer += texture->height)
		{
			textureBlitColumn(buffer, s_display + y0*s_width + x, yPixelCount);
		}
	}

	void clear3DView(u8* framebuffer)
	{
		memset(framebuffer, 0, s_width);
	}
}  // RClassic_Fixed

}  // TFE_Jedi
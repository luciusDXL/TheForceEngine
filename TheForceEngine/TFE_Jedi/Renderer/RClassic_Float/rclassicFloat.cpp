#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Game/igame.h>
#include "rclassicFloatSharedState.h"
#include "rlightingFloat.h"
#include "rflatFloat.h"
#include "../redgePair.h"
#include "rsectorFloat.h"
#include "../rcommon.h"

namespace TFE_Jedi
{
	
namespace RClassic_Float
{
	static JBool s_fullDetail = JTRUE;
	static f32 s_viewWidth;
	static f32 s_viewHeight;
	static f32 s_minScreenX;
	static f32 s_maxScreenX;
	static f32 s_screenWidthFract;
	static f32 s_oneOverWidthFract;
	static f32 s_worldX;
	static f32 s_worldY;
	static f32 s_worldZ;
	static f32 s_xOffset;
	static f32 s_zOffset;
	static f32 s_worldYaw;
	static f32 s_maxPitch = 8192.0f;
	static s32 s_screenHeight;
	static s32 s_pixelCount;
	static s32 s_visionEffect;
	static u32 s_pixelMask;
	static RSector* s_sector;
	
	void setVisionEffect(s32 effect)
	{
		s_visionEffect = effect;
	}

	void transformPointByCamera(vec3_float* worldPoint, vec3_float* viewPoint)
	{
		viewPoint->x = worldPoint->x * s_rcfltState.cosYaw + worldPoint->z * s_rcfltState.sinYaw + s_rcfltState.cameraTrans.x;
		viewPoint->y = worldPoint->y - s_rcfltState.eyeHeight;
		viewPoint->z = worldPoint->z * s_rcfltState.cosYaw + worldPoint->x * s_rcfltState.negSinYaw + s_rcfltState.cameraTrans.z;
	}

	void setCameraWorldPos(f32 x, f32 z, f32 eyeHeight)
	{
		s_worldX = x;
		s_worldZ = z;
		s_worldY = eyeHeight;
	}

	void computeCameraTransform(RSector* sector, f32 pitch, f32 yaw, f32 camX, f32 camY, f32 camZ)
	{
		s_rcfltState.cameraPos.x = camX;
		s_rcfltState.cameraPos.z = camZ;
		s_rcfltState.eyeHeight = camY;

		s_sector = sector;

		s_rcfltState.cameraYaw = yaw;
		s_rcfltState.cameraPitch = pitch;
		const f32 pitchOffsetScale = 226.0f * s_rcfltState.focalLenAspect/160.0f;	// half_width / sin(360*2047/16384)

		s_xOffset = -camX;
		s_zOffset = -camZ;
		sinCosFlt(-yaw, &s_rcfltState.sinYaw, &s_rcfltState.cosYaw);

		s_rcfltState.negSinYaw = -s_rcfltState.sinYaw;
		if (s_maxPitch != s_rcfltState.cameraPitch)
		{
			f32 sinPitch = sinFlt(s_rcfltState.cameraPitch);
			f32 pitchOffset = sinPitch * pitchOffsetScale;
			s_rcfltState.projOffsetY = s_rcfltState.projOffsetYBase + pitchOffset;
			s_screenYMid  = s_screenYMidBase + (s32)floorf(pitchOffset);

			// yMax*0.5 / halfWidth; ~pixel Aspect
			s_rcfltState.yPlaneBot =  (s_viewHeight*0.5f - pitchOffset) / s_rcfltState.focalLenAspect;
			s_rcfltState.yPlaneTop = -(s_viewHeight*0.5f + pitchOffset) / s_rcfltState.focalLenAspect;
		}

		s_rcfltState.cameraTrans.z = s_zOffset * s_rcfltState.cosYaw + s_xOffset * s_rcfltState.negSinYaw;
		s_rcfltState.cameraTrans.x = s_xOffset * s_rcfltState.cosYaw + s_zOffset * s_rcfltState.sinYaw;
		setCameraWorldPos(s_rcfltState.cameraPos.x, s_rcfltState.cameraPos.z, s_rcfltState.eyeHeight);
		s_worldYaw = s_rcfltState.cameraYaw;

		// Camera Transform:
		s_rcfltState.cameraMtx[0] = s_rcfltState.cosYaw;
		s_rcfltState.cameraMtx[2] = s_rcfltState.negSinYaw;
		s_rcfltState.cameraMtx[4] = 1.0f;
		s_rcfltState.cameraMtx[6] = s_rcfltState.sinYaw;
		s_rcfltState.cameraMtx[8] = s_rcfltState.cosYaw;
	}

	void computeSkyOffsets()
	{
		fixed16_16 parallax[2];
		TFE_Jedi::getSkyParallax(&parallax[0], &parallax[1]);

		// angles range from -16384 to 16383; multiply by 4 to convert to [-1, 1) range.
		s_rcfltState.skyYawOffset   = -s_rcfltState.cameraYaw   / 16384.0f * fixed16ToFloat(parallax[0]);
		s_rcfltState.skyPitchOffset = -s_rcfltState.cameraPitch / 16384.0f * fixed16ToFloat(parallax[1]);
	}

	void setupScreenParameters(s32 w, s32 h, s32 x0, s32 y0)
	{
		s_viewWidth  = f32(w);
		s_viewHeight = f32(h);
		s_screenWidth  = w;
		s_screenHeight = h;

		s_minScreenX_Pixels = x0;
		s_maxScreenX_Pixels = x0 + w - 1;
		s_minScreenX = f32(s_minScreenX_Pixels);
		s_maxScreenX = f32(s_maxScreenX_Pixels);

		s_minScreenY = y0;
		s_maxScreenY = y0 + h - 1;
		s_rcfltState.windowMinY = f32(y0);
		s_rcfltState.windowMaxY = f32(y0 + h - 1);

		s_fullDetail = JTRUE;
		s_pixelCount = w * h;
	}
		
	void setupProjectionParameters(f32 halfWidthFlt, s32 xc, s32 yc)
	{
		s_rcfltState.halfWidth = halfWidthFlt;
		s_screenXMid = xc;
		s_screenYMid = yc;
		s_screenYMidBase = yc;

		s_rcfltState.projOffsetX = f32(xc);
		s_rcfltState.projOffsetY = f32(yc);
		s_rcfltState.projOffsetYBase = s_rcfltState.projOffsetY;

		s_windowX0 = s_minScreenX_Pixels;
		s_windowX1 = s_maxScreenX_Pixels;

		s_rcfltState.oneOverHalfWidth = 1.0f / halfWidthFlt;

		// TFE
		s_rcfltState.focalLength = s_rcfltState.halfWidth;
		s_rcfltState.focalLenAspect = s_rcfltState.halfWidth;
		s_rcfltState.aspectScaleX = 1.0f;
		s_rcfltState.aspectScaleY = 1.0f;
		s_rcfltState.nearPlaneHalfLen = 1.0f;
	}

	void setWidthFraction(f32 widthFract)
	{
		s_screenWidthFract = widthFract;
		s_oneOverWidthFract = 1.0f / widthFract;
	}

	void buildProjectionTables(s32 xc, s32 yc, s32 w, s32 h)
	{
		s32 halfHeight = h >> 1;
		s32 halfWidth  = w >> 1;

		setupScreenParameters(w, h, xc - halfWidth, yc - halfHeight);
		setupProjectionParameters(f32(halfWidth), xc, yc);
		setWidthFraction(1.0f);

		EdgePairFloat* flatEdge = &s_rcfltState.flatEdgeList[s_flatCount];
		s_rcfltState.flatEdge = flatEdge;
		flat_addEdges(s_screenWidth, s_minScreenX_Pixels, 0, s_rcfltState.windowMaxY, 0, s_rcfltState.windowMinY);
		
		s_columnTop = (s32*)game_realloc(s_columnTop, s_width * sizeof(s32));
		s_columnBot = (s32*)game_realloc(s_columnBot, s_width * sizeof(s32));
		s_rcfltState.depth1d_all = (f32*)game_realloc(s_rcfltState.depth1d_all, s_width * sizeof(f32) * (MAX_ADJOIN_DEPTH + 1));
		s_windowTop_all = (s32*)game_realloc(s_windowTop, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));
		s_windowBot_all = (s32*)game_realloc(s_windowBot, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));

		memset(s_windowTop_all, s_minScreenY, s_width);
		memset(s_windowBot_all, s_maxScreenY, s_width);

		// Build tables
		s_rcfltState.column_Z_Over_X = (f32*)game_realloc(s_rcfltState.column_Z_Over_X, s_width * sizeof(f32));
		s_rcfltState.column_X_Over_Z = (f32*)game_realloc(s_rcfltState.column_X_Over_Z, s_width * sizeof(f32));
		s_rcfltState.skyTable = (f32*)game_realloc(s_rcfltState.skyTable, (s_width + 1) * sizeof(f32));

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
		for (s32 i = 0, x = 0; x < s_width; x++, i++)
		{
			f32 xPos = f32(x - xMid);
			s_rcfltState.column_Z_Over_X[i] = (xMid != x) ? (s_rcfltState.halfWidth / xPos) : s_rcfltState.halfWidth;
		}

		for (s32 i = 0, x = 0; x < s_width; i++, x++)
		{
			if (x != xMid)
			{
				f32 xPos = f32(x - xMid);
				s_rcfltState.column_X_Over_Z[i] = (xPos / s_rcfltState.halfWidth);
			}
			else
			{
				s_rcfltState.column_X_Over_Z[i] = 0.0f;
			}
		}
	}

	void computeSkyTable()
	{
		fixed16_16 parallax0, parallax1;
		TFE_Jedi::getSkyParallax(&parallax0, &parallax1);
		f32 parallaxFlt = fixed16ToFloat(parallax0);

		s32 xMid   = s_screenXMid;
		f32 xScale = 2.0f / f32(s_width);
		s_rcfltState.skyTable[0] = 0;
		for (s32 i = 0, x = 0; x < s_width; i++, x++)
		{
			f32 xOffset = f32(x - xMid);
			f32 angleFractF = atanf(xOffset * xScale) * 2607.595f;
			f32 angleFract =  angleFractF / 16384.0f;

			// This intentionally overflows when x = 0 and becomes 0...
			s_rcfltState.skyTable[1 + i] = angleFract * parallaxFlt;
		}
	}

	void setIdentityMatrix(f32* mtx)
	{
		mtx[0] = 1.0f;
		mtx[3] = 0;
		mtx[6] = 0;

		mtx[1] = 0;
		mtx[4] = 1.0f;
		mtx[7] = 0;

		mtx[2] = 0;
		mtx[5] = 0;
		mtx[8] = 1.0f;
	}

	void createLight(CameraLightFlt* light, f32 x, f32 y, f32 z, f32 brightness)
	{
		vec3_float* dirWS = &light->lightWS;
		dirWS->x = x;
		dirWS->y = y;
		dirWS->z = z;
		light->brightness = brightness;
		normalizeVec3(dirWS, dirWS);
	}

	void setupInitCameraAndLights(s32 width, s32 height)
	{
		s_width  = width;
		s_height = height;

		buildProjectionTables(width>>1, height>>1, s_width, s_height - 2);
		TFE_Jedi::setSkyParallax(FIXED(1024), FIXED(1024));
		computeSkyTable();

		setIdentityMatrix(s_rcfltState.cameraMtx);
		computeCameraTransform(nullptr, 0, 0, 0, 0, 0);

		s_lightCount = 0;

		createLight(&s_cameraLight[0], 0, 0, 1.0f, 1.0f);
		s_lightCount++;

		createLight(&s_cameraLight[1], 0, 1.0f, 0, 1.0f);
		s_lightCount++;

		createLight(&s_cameraLight[2], 1.0f, 0, 0, 1.0f);
		s_lightCount++;

		s_colorMap = nullptr;
		s_lightSourceRamp = nullptr;
		s_enableFlatShading = JFALSE;
		s_cameraLightSource = JFALSE;
		s_pixelMask = 0xffffffff;
		s_visionEffect = 0;
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
}  // RClassic_Float

}  // TFE_Jedi
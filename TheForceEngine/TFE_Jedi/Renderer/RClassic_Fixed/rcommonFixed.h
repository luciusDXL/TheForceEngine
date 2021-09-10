#pragma once
#include <TFE_System/types.h>
#include "../rwallRender.h"
#include "../rlimits.h"

namespace TFE_Jedi
{
	struct EdgePair;

	namespace RClassic_Fixed
	{
		// Resolution
		extern fixed16_16 s_halfWidth_Fixed;
		extern fixed16_16 s_halfHeight_Fixed;
		extern fixed16_16 s_projOffsetX;
		extern fixed16_16 s_projOffsetY;
		extern fixed16_16 s_projOffsetYBase;
		extern fixed16_16 s_halfHeightBase_Fixed;
		extern fixed16_16 s_oneOverHalfWidth;

		// Projection
		extern fixed16_16  s_focalLength_Fixed;
		extern fixed16_16  s_focalLenAspect_Fixed;
		extern fixed16_16  s_eyeHeight_Fixed;
		extern fixed16_16* s_depth1d_all_Fixed;
		extern fixed16_16* s_depth1d_Fixed;

		// Camera
		extern fixed16_16 s_cameraPosX_Fixed;
		extern fixed16_16 s_cameraPosY_Fixed;
		extern fixed16_16 s_cameraPosZ_Fixed;
		extern fixed16_16 s_xCameraTrans_Fixed;
		extern fixed16_16 s_zCameraTrans_Fixed;
		extern fixed16_16 s_cosYaw_Fixed;
		extern fixed16_16 s_sinYaw_Fixed;
		extern fixed16_16 s_negSinYaw_Fixed;
		extern fixed16_16 s_cameraYaw_Fixed;
		extern fixed16_16 s_cameraPitch_Fixed;
		extern fixed16_16 s_yPlaneTop_Fixed;
		extern fixed16_16 s_yPlaneBot_Fixed;
		extern fixed16_16 s_skyYawOffset_Fixed;
		extern fixed16_16 s_skyPitchOffset_Fixed;
		extern fixed16_16* s_skyTable_Fixed;
		extern fixed16_16 s_cameraMtx_Fixed[9];
	
		// Window
		extern fixed16_16 s_windowMinZ_Fixed;
		extern fixed16_16 s_windowMinYFixed;
		extern fixed16_16 s_windowMaxYFixed;
		extern s32 s_screenWidth;

		// Column Heights
		extern fixed16_16* s_column_Z_Over_X;
		extern fixed16_16* s_column_X_Over_Z;

		// Flats
		extern fixed16_16* s_rcpY;
	}  // RClassic_Fixed
}  // TFE_Jedi
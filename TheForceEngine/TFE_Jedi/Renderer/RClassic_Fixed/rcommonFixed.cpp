#include "rcommonFixed.h"
#include <TFE_Jedi/Math/fixedPoint.h>

namespace TFE_JediRenderer
{

namespace RClassic_Fixed
{
	// Resolution
	fixed16_16 s_halfWidth_Fixed;
	fixed16_16 s_halfHeight_Fixed;
	fixed16_16 s_projOffsetX;
	fixed16_16 s_projOffsetY;
	fixed16_16 s_projOffsetYBase;
	fixed16_16 s_halfHeightBase_Fixed;
	fixed16_16 s_oneOverHalfWidth;

	// Projection
	fixed16_16  s_focalLength_Fixed;
	fixed16_16  s_focalLenAspect_Fixed;
	fixed16_16  s_eyeHeight_Fixed;
	fixed16_16* s_depth1d_all_Fixed;
	fixed16_16* s_depth1d_Fixed;

	// Camera
	fixed16_16 s_cameraPosX_Fixed;
	fixed16_16 s_cameraPosY_Fixed;
	fixed16_16 s_cameraPosZ_Fixed;
	fixed16_16 s_xCameraTrans_Fixed;
	fixed16_16 s_zCameraTrans_Fixed;
	fixed16_16 s_cosYaw_Fixed;
	fixed16_16 s_sinYaw_Fixed;
	fixed16_16 s_negSinYaw_Fixed;
	fixed16_16 s_cameraYaw_Fixed;
	fixed16_16 s_cameraPitch_Fixed;
	fixed16_16 s_yPlaneTop_Fixed;
	fixed16_16 s_yPlaneBot_Fixed;
	fixed16_16 s_skyYawOffset_Fixed;
	fixed16_16 s_skyPitchOffset_Fixed;
	fixed16_16* s_skyTable_Fixed;
	fixed16_16 s_cameraMtx_Fixed[9] = { 0 };

	// Window
	fixed16_16 s_windowMinZ_Fixed;

	// Column Heights
	fixed16_16* s_column_Z_Over_X;
	fixed16_16* s_column_X_Over_Z;

	// Flats
	fixed16_16* s_rcpY;
}  // RClassic_Fixed

}  // TFE_JediRenderer
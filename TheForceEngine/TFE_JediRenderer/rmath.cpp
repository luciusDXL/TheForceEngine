#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "rmath.h"

namespace TFE_JediRenderer
{
	/////////////////////////////////////////////
	// Fixed point
	/////////////////////////////////////////////
	void computeTransformFromAngles_Fixed(s16 yaw, s16 pitch, s16 roll, fixed16_16* transform)
	{
		fixed16_16 sinYaw, cosYaw;
		fixed16_16 sinPch, cosPch;
		fixed16_16 sinRol, cosRol;
		sinCosFixed(yaw, sinYaw, cosYaw);
		sinCosFixed(pitch, sinPch, cosPch);
		sinCosFixed(roll, sinRol, cosRol);

		transform[0] = mul16(cosYaw, cosRol);
		transform[1] = mul16(cosPch, sinRol) + mul16(mul16(sinPch, sinYaw), cosPch);
		transform[2] = mul16(sinPch, sinRol) - mul16(mul16(cosPch, sinYaw), cosRol);

		transform[3] = -mul16(cosYaw, sinRol);
		transform[4] = mul16(cosPch, cosRol) - mul16(mul16(sinPch, sinYaw), sinRol);
		transform[5] = mul16(sinPch, cosRol) + mul16(mul16(cosPch, sinYaw), sinRol);

		transform[6] = sinYaw;
		transform[7] = -mul16(sinPch, cosYaw);
		transform[8] = mul16(cosPch, cosYaw);
	}

	/////////////////////////////////////////////
	// Floating point
	/////////////////////////////////////////////
	void computeTransformFromAngles_Float(s16 yaw, s16 pitch, s16 roll, f32* transform)
	{
		f32 sinYaw, cosYaw;
		f32 sinPch, cosPch;
		f32 sinRol, cosRol;
		sinCosFlt(yaw, sinYaw, cosYaw);
		sinCosFlt(pitch, sinPch, cosPch);
		sinCosFlt(roll, sinRol, cosRol);

		transform[0] = cosYaw * cosRol;
		transform[1] = cosPch * sinRol + sinPch * sinYaw*cosPch;
		transform[2] = sinPch * sinRol - cosPch * sinYaw*cosRol;

		transform[3] = -cosYaw * sinRol;
		transform[4] = cosPch * cosRol - sinPch * sinYaw*sinRol;
		transform[5] = sinPch * cosRol + cosPch * sinYaw*sinRol;

		transform[6] = sinYaw;
		transform[7] = -sinPch * cosYaw;
		transform[8] = cosPch * cosYaw;
	}
}

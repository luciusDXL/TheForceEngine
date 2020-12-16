#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "rmath.h"
#include "RClassic_Fixed/rcommonFixed.h"
#include "rcommon.h"

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

	void normalizeVec3(vec3_fixed* vIn, vec3_fixed* vOut)
	{
		s32 distSq = mul16(vIn->x, vIn->x) + mul16(vIn->y, vIn->y) + mul16(vIn->z, vIn->z);
		s32 dist = fixedSqrt(distSq);

		if (dist > 0)
		{
			vOut->x = div16(vIn->x, dist);
			vOut->y = div16(vIn->y, dist);
			vOut->z = div16(vIn->z, dist);
		}
		else
		{
			vOut->x = 0;
			vOut->y = 0;
			vOut->z = 0;
		}
	}

	void transformPointByCamera(vec3_fixed* worldPoint, vec3_fixed* viewPoint)
	{
		viewPoint->x = mul16(worldPoint->x, RClassic_Fixed::s_cosYaw_Fixed) + mul16(worldPoint->z, RClassic_Fixed::s_sinYaw_Fixed) + RClassic_Fixed::s_xCameraTrans_Fixed;
		viewPoint->y = worldPoint->y - RClassic_Fixed::s_eyeHeight_Fixed;
		viewPoint->z = mul16(worldPoint->z, RClassic_Fixed::s_cosYaw_Fixed) + mul16(worldPoint->x, RClassic_Fixed::s_negSinYaw_Fixed) + RClassic_Fixed::s_zCameraTrans_Fixed;
	}

	void transformPointByCameraFixed(vec3* worldPoint, vec3* viewPoint)
	{
		viewPoint->x.f16_16 = mul16(worldPoint->x.f16_16, RClassic_Fixed::s_cosYaw_Fixed) + mul16(worldPoint->z.f16_16, RClassic_Fixed::s_sinYaw_Fixed) + RClassic_Fixed::s_xCameraTrans_Fixed;
		viewPoint->y.f16_16 = worldPoint->y.f16_16 - RClassic_Fixed::s_eyeHeight_Fixed;
		viewPoint->z.f16_16 = mul16(worldPoint->z.f16_16, RClassic_Fixed::s_cosYaw_Fixed) + mul16(worldPoint->x.f16_16, RClassic_Fixed::s_negSinYaw_Fixed) + RClassic_Fixed::s_zCameraTrans_Fixed;
	}

	/////////////////////////////////////////////
	// Floating point
	/////////////////////////////////////////////
	void computeTransformFromAngles_Float(f32 yaw, f32 pitch, f32 roll, f32* transform)
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

	void normalizeVec3(vec3_float* vIn, vec3_float* vOut)
	{
		f32 distSq = vIn->x*vIn->x + vIn->y*vIn->y + vIn->z*vIn->z;
		f32 dist = sqrtf(distSq);

		if (dist > FLT_EPSILON)
		{
			f32 scale = 1.0f / dist;
			vOut->x = vIn->x * scale;
			vOut->y = vIn->y * scale;
			vOut->z = vIn->z * scale;
		}
		else
		{
			vOut->x = 0;
			vOut->y = 0;
			vOut->z = 0;
		}
	}

	void transformPointByCamera(vec3_float* worldPoint, vec3_float* viewPoint)
	{
		viewPoint->x = worldPoint->x*s_cosYaw + worldPoint->z*s_sinYaw + s_xCameraTrans;
		viewPoint->y = worldPoint->y - s_eyeHeight;
		viewPoint->z = worldPoint->z*s_cosYaw + worldPoint->x*s_negSinYaw + s_zCameraTrans;
	}

	void transformPointByCameraFloat(vec3* worldPoint, vec3* viewPoint)
	{
		viewPoint->x.f32 = worldPoint->x.f32*s_cosYaw + worldPoint->z.f32*s_sinYaw + s_xCameraTrans;
		viewPoint->y.f32 = worldPoint->y.f32 - s_eyeHeight;
		viewPoint->z.f32 = worldPoint->z.f32*s_cosYaw + worldPoint->x.f32*s_negSinYaw + s_zCameraTrans;
	}
}

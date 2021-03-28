#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "core_math.h"

namespace TFE_CoreMath
{
	/////////////////////////////////////////////
	// Fixed point
	/////////////////////////////////////////////
	void computeTransformFromAngles_Fixed(angle14_32 yaw, angle14_32 pitch, angle14_32 roll, fixed16_16* transform)
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

	angle14_32 vec2ToAngle(fixed16_16 dx, fixed16_16 dz)
	{
		if (dx == 0 && dz == 0)
		{
			return 0;
		}

		const s32 signsDiff = (signV2A(dx) != signV2A(dz)) ? 1 : 0;
		// Splits the view into 4 quadrants, 0 - 3:
		// 1 | 0
		// -----
		// 2 | 3
		const s32 quadrant = (dz < 0 ? 2 : 0) + signsDiff;

		// Further splits the quadrants into sub-quadrants:
		// \2|1/
		// 3\|/0
		//---*---
		// 4/|\7
		// /5|6\
			//
		dx = abs(dx);
		dz = abs(dz);
		const s32 subquadrant = quadrant * 2 + ((dx < dz) ? (1 - signsDiff) : signsDiff);

		// true in sub-quadrants: 0, 3, 4, 7; where dz tends towards 0.
		if ((subquadrant - 1) & 2)
		{
			// The original code did the "3 xor" trick to swap dx and dz.
			std::swap(dx, dz);
		}

		// next compute |dx| / |dz|, which will be a value from 0.0 to 1.0
		fixed16_16 dXdZ = div16(dx, dz);
		if (subquadrant & 1)
		{
			// invert the ratio in sub-quadrants 1, 3, 5, 7 to maintain the correct direction.
			dXdZ = ONE_16 - dXdZ;
		}

		// subquadrantF is based on the sub-quadrant, essentially fixed16(subquadrant)
		// which has a range of 0 to 7.0 (see above).
		const fixed16_16 subquadrantF = intToFixed16(subquadrant);
		// angle = (int(2.0 - (zF + dXdZ)) * 2048) & 16383
		// this flips the angle so that straight up (dx = 0, dz > 0) is 0, right is 90, down is 180.
		const angle14_32 angle = (2 * ONE_16 - (subquadrantF + dXdZ)) >> 5;
		// the final angle will be in the range of 0 - 16383
		return angle & 0x3fff;
	}

	fixed16_16 computeDirAndLength(fixed16_16 dx, fixed16_16 dz, fixed16_16* dirX, fixed16_16* dirZ)
	{
		fixed16_16 dist = vec2Length(dx, dz);

		if (dist)
		{
			*dirX = div16(dx, dist);
			*dirZ = div16(dz, dist);
		}
		else
		{
			*dirZ = 0;
			*dirX = 0;
		}

		return dist;
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
}

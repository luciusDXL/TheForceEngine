//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////

#include <utility>
#include <TFE_System/types.h>
#include "core_math.h"

namespace TFE_Jedi
{
	static s32 s_negArcCos;

	/////////////////////////////////////////////
	// Fixed point
	/////////////////////////////////////////////
	void computeTransformFromAngles_Fixed(angle14_32 yaw, angle14_32 pitch, angle14_32 roll, fixed16_16* transform)
	{
		fixed16_16 sinYaw, cosYaw;
		fixed16_16 sinPch, cosPch;
		fixed16_16 sinRol, cosRol;
		sinCosFixed(yaw, &sinYaw, &cosYaw);
		sinCosFixed(pitch, &sinPch, &cosPch);
		sinCosFixed(roll, &sinRol, &cosRol);

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

	void rotateVectorM3x3(vec3_fixed* inVec, vec3_fixed* outVec, s32* mtx)
	{
		outVec->x = mul16(inVec->x, mtx[0]) + mul16(inVec->y, mtx[3]) + mul16(inVec->z, mtx[6]);
		outVec->y = mul16(inVec->x, mtx[1]) + mul16(inVec->y, mtx[4]) + mul16(inVec->z, mtx[7]);
		outVec->z = mul16(inVec->x, mtx[2]) + mul16(inVec->y, mtx[5]) + mul16(inVec->z, mtx[8]);
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
		return angle & ANGLE_MASK;
	}

	fixed16_16 computeDirAndLength(fixed16_16 dx, fixed16_16 dz, fixed16_16* dirX, fixed16_16* dirZ)
	{
		fixed16_16 dist = vec2Length(dx, dz);

		if (dist)
		{
			*dirX = clamp(div16(dx, dist), -ONE_16, ONE_16);
			*dirZ = clamp(div16(dz, dist), -ONE_16, ONE_16);
		}
		else
		{
			*dirX = 0;
			*dirZ = 0;
		}

		return dist;
	}

	angle14_32 getAngleDifference(angle14_32 angle0, angle14_32 angle1)
	{
		angle0 &= ANGLE_MASK;
		angle1 &= ANGLE_MASK;

		angle14_32 dAngle = angle1 - angle0;
		if (abs(dAngle) <= 8191)	// less than 180 degrees
		{
			return dAngle;
		}
		// Otherwise we have to compute the "shortest arc".
		if (dAngle >= 0)
		{
			return dAngle - ANGLE_MAX - 1;
		}
		// dAngle < 0
		return dAngle + ANGLE_MAX;
	}

	void mulMatrix3x3(fixed16_16* mtx0, fixed16_16* mtx1, fixed16_16* mtxOut)
	{
		mtxOut[0] = mul16(mtx0[0], mtx1[0]) + mul16(mtx0[3], mtx1[3]) + mul16(mtx0[6], mtx1[6]);
		mtxOut[3] = mul16(mtx0[0], mtx1[1]) + mul16(mtx0[3], mtx1[4]) + mul16(mtx0[6], mtx1[7]);
		mtxOut[6] = mul16(mtx0[0], mtx1[2]) + mul16(mtx0[3], mtx1[5]) + mul16(mtx0[6], mtx1[8]);

		mtxOut[1] = mul16(mtx0[1], mtx1[0]) + mul16(mtx0[4], mtx1[3]) + mul16(mtx0[7], mtx1[6]);
		mtxOut[4] = mul16(mtx0[1], mtx1[1]) + mul16(mtx0[4], mtx1[4]) + mul16(mtx0[7], mtx1[7]);
		mtxOut[7] = mul16(mtx0[1], mtx1[2]) + mul16(mtx0[4], mtx1[5]) + mul16(mtx0[7], mtx1[8]);

		mtxOut[2] = mul16(mtx0[2], mtx1[0]) + mul16(mtx0[5], mtx1[3]) + mul16(mtx0[8], mtx1[6]);
		mtxOut[5] = mul16(mtx0[2], mtx1[1]) + mul16(mtx0[5], mtx1[4]) + mul16(mtx0[8], mtx1[7]);
		mtxOut[8] = mul16(mtx0[2], mtx1[2]) + mul16(mtx0[5], mtx1[5]) + mul16(mtx0[8], mtx1[8]);
	}

	angle14_32 arcCosFixed(fixed16_16 sinAngle, angle14_32 angle)
	{
		const fixed16_16* cosTable = s_cosTable;
		if (sinAngle >= 0)
		{
			s_negArcCos = 0;
		}
		else
		{
			s_negArcCos = 1;
			sinAngle = -angle;
		}
		s32 i = 0;
		for (; i < 4095; i++, cosTable++)
		{
			if (sinAngle >= *cosTable)
			{
				break;
			}
		}

		angle14_32 resAngle = 4095 - i;
		if (s_negArcCos)
		{
			resAngle += 8192;
		}
		return resAngle;
	}

	/////////////////////////////////////////////
	// Floating point
	/////////////////////////////////////////////
	void computeTransformFromAngles_Float(f32 yaw, f32 pitch, f32 roll, f32* transform)
	{
		f32 sinYaw, cosYaw;
		f32 sinPch, cosPch;
		f32 sinRol, cosRol;
		sinCosFlt(yaw, &sinYaw, &cosYaw);
		sinCosFlt(pitch, &sinPch, &cosPch);
		sinCosFlt(roll, &sinRol, &cosRol);

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

	void rotateVectorM3x3(vec3_float* inVec, vec3_float* outVec, f32* mtx)
	{
		outVec->x = inVec->x*mtx[0] + inVec->y*mtx[3] + inVec->z*mtx[6];
		outVec->y = inVec->x*mtx[1] + inVec->y*mtx[4] + inVec->z*mtx[7];
		outVec->z = inVec->x*mtx[2] + inVec->y*mtx[5] + inVec->z*mtx[8];
	}

	s32 vec2ToAngle(f32 dx, f32 dz)
	{
		if (dx == 0.0f && dz == 0.0f)
		{
			return 0;
		}

		const s32 signsDiff = (signV2A(dx) != signV2A(dz)) ? 1 : 0;
		// Splits the view into 4 quadrants, 0 - 3:
		// 1 | 0
		// -----
		// 2 | 3
		const s32 quadrant = (dz < 0.0f ? 2 : 0) + signsDiff;

		// Further splits the quadrants into sub-quadrants:
		// \2|1/
		// 3\|/0
		//---*---
		// 4/|\7
		// /5|6\
			//
		dx = fabsf(dx);
		dz = fabsf(dz);
		const s32 subquadrant = quadrant * 2 + ((dx < dz) ? (1 - signsDiff) : signsDiff);

		// true in sub-quadrants: 0, 3, 4, 7; where dz tends towards 0.
		if ((subquadrant - 1) & 2)
		{
			// The original code did the "3 xor" trick to swap dx and dz.
			std::swap(dx, dz);
		}

		// next compute |dx| / |dz|, which will be a value from 0.0 to 1.0
		f32 dXdZ = dx / dz;
		if (subquadrant & 1)
		{
			// invert the ratio in sub-quadrants 1, 3, 5, 7 to maintain the correct direction.
			dXdZ = 1.0f - dXdZ;
		}

		// subquadrantF is based on the sub-quadrant, essentially fixed16(subquadrant)
		// which has a range of 0 to 7.0 (see above).
		const f32 subquadrantF = f32(subquadrant);
		// this flips the angle so that straight up (dx = 0, dz > 0) is 0, right is 90, down is 180.
		const f32 angle = 2.0f - (subquadrantF + dXdZ);
		// the final angle will be in the range of 0 - 16383
		return s32(angle * 2048.0f) & ANGLE_MASK;
	}
}

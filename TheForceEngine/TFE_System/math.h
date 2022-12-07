#pragma once

#include "types.h"
#include <math.h>
#include <float.h>

namespace TFE_Math
{
	inline f32 sign(f32 x)
	{
		return x < 0.0f ? -1.0f : 1.0f;
	}

	inline bool isPow2(u32 x)
	{
		return (x > 0) && (x & (x - 1)) == 0;
	}

	inline bool isPow2(s32 x)
	{
		return (x > 0) && (x & (x - 1)) == 0;
	}

	inline u32 log2(u32 x)
	{
		if (x == 0 || x == 1) { return 0; }

		u32 l2 = 0;
		while (x > 1)
		{
			l2++;
			x >>= 1;
		}
		return l2;
	}

	inline u32 nextPow2(u32 x)
	{
		if (x == 0) { return 0; }

		x--;
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
		x++;
		return x;
	}

	inline f32 fract(f32 x)
	{
		return x - floorf(x);
	}

	// the effective range is (-4.8, 4.8]. Outside of that the value is clamped at [-1, 1].
	inline f32 tanhf_series(f32 beta)
	{
		if (beta > 4.8f) { return 1.0f; }
		else if (beta <= -4.8f) { return -1.0f; }

		const f32 x2 = beta * beta;
		const f32 a  = beta * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
		const f32 b  = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
		return a / b;
	}

	inline f32 dot(const Vec2f* p0, const Vec2f* p1)
	{
		return p0->x*p1->x + p0->z*p1->z;
	}

	inline f32 dot(const Vec3f* p0, const Vec3f* p1)
	{
		return p0->x*p1->x + p0->y*p1->y + p0->z*p1->z;
	}

	inline f32 dot(const Vec4f* p0, const Vec4f* p1)
	{
		return p0->x*p1->x + p0->y*p1->y + p0->z*p1->z + p0->w*p1->w;
	}

	inline f32 distance(const Vec3f* p0, const Vec3f* p1)
	{
		Vec3f diff = { p1->x - p0->x, p1->y - p0->y, p1->z - p0->z };
		return sqrtf(dot(&diff, &diff));
	}

	inline f32 distance(const Vec2f* p0, const Vec2f* p1)
	{
		Vec2f diff = { p1->x - p0->x, p1->z - p0->z };
		return sqrtf(dot(&diff, &diff));
	}

	inline f32 distanceSq(const Vec3f* p0, const Vec3f* p1)
	{
		Vec3f diff = { p1->x - p0->x, p1->y - p0->y, p1->z - p0->z };
		return dot(&diff, &diff);
	}

	inline f32 distanceSq(const Vec2f* p0, const Vec2f* p1)
	{
		Vec2f diff = { p1->x - p0->x, p1->z - p0->z };
		return dot(&diff, &diff);
	}
		
	inline Vec3f normalize(const Vec3f* vec)
	{
		const f32 lenSq = dot(vec, vec);
		Vec3f nrm;
		if (lenSq > FLT_EPSILON)
		{
			const f32 scale = 1.0f / sqrtf(lenSq);
			nrm = { vec->x * scale, vec->y * scale, vec->z * scale };
		}
		else
		{
			nrm = *vec;
		}
		return nrm;
	}

	inline Vec2f normalize(const Vec2f* vec)
	{
		const f32 lenSq = dot(vec, vec);
		Vec2f nrm;
		if (lenSq > FLT_EPSILON)
		{
			const f32 scale = 1.0f / sqrtf(lenSq);
			nrm = { vec->x * scale, vec->z * scale };
		}
		else
		{
			nrm = *vec;
		}
		return nrm;
	}

	inline Vec3f cross(const Vec3f* a, const Vec3f* b)
	{
		return { a->y*b->z - a->z*b->y, a->z*b->x - a->x*b->z, a->x*b->y - a->y*b->x };
	}

	Mat3 computeViewMatrix(const Vec3f* lookDir, const Vec3f* upDir);
	Mat3 transpose(const Mat3& mtx);
	Mat4 transpose4(Mat4 mtx);
	Mat4 computeProjMatrix(f32 fovInRadians, f32 aspectRatio, f32 zNear, f32 zFar);
	Mat4 computeProjMatrixExplicit(f32 xScale, f32 yScale, f32 zNear, f32 zFar);
	Mat4 computeInvProjMatrix(const Mat4& mtx);
	Mat4 mulMatrix4(const Mat4& mtx0, const Mat4& mtx1);

	void buildRotationMatrix(Vec3f angles, Vec3f* mat);
}

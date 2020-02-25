#pragma once

#include "types.h"
#include <math.h>
#include <float.h>

namespace DXL2_Math
{
	inline bool isPow2(u32 x)
	{
		return (x > 0) && (x & (x - 1)) == 0;
	}

	inline bool isPow2(s32 x)
	{
		return (x > 0) && (x & (x - 1)) == 0;
	}

	inline f32 fract(f32 x)
	{
		return x - floorf(x);
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
	Mat4 computeProjMatrix(f32 fovInRadians, f32 aspectRatio, f32 zNear, f32 zFar);
	Mat4 computeInvProjMatrix(const Mat4& mtx);

	void buildRotationMatrix(Vec3f angles, Vec3f* mat);
}

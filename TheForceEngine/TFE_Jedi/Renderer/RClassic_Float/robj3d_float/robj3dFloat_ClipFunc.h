//////////////////////////////////////////////////////////////////////
// Inline clip function to handle the various combinations.
//////////////////////////////////////////////////////////////////////
#include <cstring>

#if defined(CLIP_INTENSITY) && !defined(CLIP_UV)
s32 robj3d_swapClipBuffersI(s32 outVertexCount)
#elif !defined(CLIP_INTENSITY) && defined(CLIP_UV)
s32 robj3d_swapClipBuffersT(s32 outVertexCount)
#elif defined(CLIP_INTENSITY) && defined(CLIP_UV)
s32 robj3d_swapClipBuffersTI(s32 outVertexCount)
#else
s32 robj3d_swapClipBuffers(s32 outVertexCount)
#endif
{
	// Swap src position and output position.
	s_clipTempPos = s_clipPosSrc;
	s_clipPosSrc = s_clipPosOut;
	s_clipPosOut = s_clipTempPos;

	// Swap src intensity and output intensity.
	#if defined(CLIP_INTENSITY)
	s_clipTempIntensity = s_clipIntensitySrc;
	s_clipIntensitySrc = s_clipIntensityOut;
	s_clipIntensityOut = s_clipTempIntensity;
	#endif

	#if defined(CLIP_UV)
	s_clipTempUv = s_clipUvSrc;
	s_clipUvSrc = s_clipUvOut;
	s_clipUvOut = s_clipTempUv;
	#endif

	s32 srcVertexCount = outVertexCount;
	s32 end = srcVertexCount - 1;

	// Left clip plane.
	s_clipPos0 = &s_clipPosSrc[end];
	s_clipPos1 = s_clipPosSrc;

	#if defined(CLIP_INTENSITY)
	s_clipIntensity0 = &s_clipIntensitySrc[end];
	s_clipIntensity1 = s_clipIntensitySrc;
	#endif

	#if defined(CLIP_UV)
	s_clipUv0 = &s_clipUvSrc[end];
	s_clipUv1 = s_clipUvSrc;
	#endif

	return srcVertexCount;
}

#undef SWAP_CLIP_BUFFERS

#if defined(CLIP_INTENSITY) && !defined(CLIP_UV)
	#define SWAP_CLIP_BUFFERS robj3d_swapClipBuffersI
#elif !defined(CLIP_INTENSITY) && defined(CLIP_UV)
	#define SWAP_CLIP_BUFFERS robj3d_swapClipBuffersT
#elif defined(CLIP_INTENSITY) && defined(CLIP_UV)
	#define SWAP_CLIP_BUFFERS robj3d_swapClipBuffersTI
#else
	#define SWAP_CLIP_BUFFERS robj3d_swapClipBuffers
#endif

#if defined(CLIP_INTENSITY) && !defined(CLIP_UV)
void computeIntersectionI(s32 outVertexCount)
#elif !defined(CLIP_INTENSITY) && defined(CLIP_UV)
void computeIntersectionT(s32 outVertexCount)
#elif defined(CLIP_INTENSITY) && defined(CLIP_UV)
void computeIntersectionTI(s32 outVertexCount)
#else
void computeIntersection(s32 outVertexCount)
#endif
{
	#if defined(CLIP_INTENSITY)
		const f32 i0 = *s_clipIntensity0;
		const f32 i1 = *s_clipIntensity1;
		s_clipIntensityOut[outVertexCount] = i0 + s_clipParam*(i1 - i0);
	#endif
	#if defined(CLIP_UV)
		const vec2_float uv0 = *s_clipUv0;
		const vec2_float uv1 = *s_clipUv1;
		s_clipUvOut[outVertexCount].x = uv0.x + s_clipParam*(uv1.x - uv0.x);
		s_clipUvOut[outVertexCount].z = uv0.z + s_clipParam*(uv1.z - uv0.z);
	#endif
}

#undef COMPUTE_CLIP_INTERSECTION
#if defined(CLIP_INTENSITY) && !defined(CLIP_UV)
#define COMPUTE_CLIP_INTERSECTION computeIntersectionI
#elif !defined(CLIP_INTENSITY) && defined(CLIP_UV)
#define COMPUTE_CLIP_INTERSECTION computeIntersectionT
#elif defined(CLIP_INTENSITY) && defined(CLIP_UV)
#define COMPUTE_CLIP_INTERSECTION computeIntersectionTI
#else
#define COMPUTE_CLIP_INTERSECTION computeIntersection
#endif

#if defined(CLIP_INTENSITY) && !defined(CLIP_UV)
s32 robj3d_clipPolygonGouraud(vec3_float* pos, f32* intensity, s32 count)
#elif !defined(CLIP_INTENSITY) && defined(CLIP_UV)
s32 robj3d_clipPolygonUv(vec3_float* pos, vec2_float* uv, s32 count)
#elif defined(CLIP_INTENSITY) && defined(CLIP_UV)
s32 robj3d_clipPolygonUvGouraud(vec3_float* pos, vec2_float* uv, f32* intensity, s32 count)
#else
s32 robj3d_clipPolygon(vec3_float* pos, s32 count)
#endif
{
	s32 outVertexCount = 0;
	s32 end = count - 1;

	#if defined(CLIP_INTENSITY)
		s_clipIntensitySrc = intensity;
	#endif
	#if defined(CLIP_UV)
		s_clipUvSrc = uv;
	#endif

	s_clipPosSrc = pos;

	// Clip against the near plane.
	s_clipPosOut = s_clipPosBuffer;
	s_clipPos0 = &pos[count - 1];
	s_clipPos1 = pos;

	#if defined(CLIP_INTENSITY)
		s_clipIntensityOut = s_clipIntensityBuffer;
		s_clipIntensity0 = &intensity[count - 1];
		s_clipIntensity1 = intensity;
	#endif

	#if defined(CLIP_UV)
		s_clipUvOut = s_clipUvBuffer;
		s_clipUv0 = &uv[count - 1];
		s_clipUv1 = uv;
	#endif

	///////////////////////////////////////////////////
	// Near Clip Plane
	///////////////////////////////////////////////////
	for (s32 i = 0; i < count; i++)
	{
		if (s_clipPos0->z < 1.0f && s_clipPos1->z < 1.0f)
		{
			s_clipPos0 = s_clipPos1;
			s_clipPos1++;

			#if defined(CLIP_INTENSITY)
				s_clipIntensity0 = s_clipIntensity1;
				s_clipIntensity1++;
			#endif
			#if defined(CLIP_UV)
				s_clipUv0 = s_clipUv1;
				s_clipUv1++;
			#endif

			continue;
		}

		// Add vertex 0 if it is on or in front of the near plane.
		if (s_clipPos0->z >= 1.0f)
		{
			s_clipPosOut[outVertexCount] = *s_clipPos0;

			#if defined(CLIP_INTENSITY)
				s_clipIntensityOut[outVertexCount] = *s_clipIntensity0;
			#endif
			#if defined(CLIP_UV)
				s_clipUvOut[outVertexCount] = *s_clipUv0;
			#endif

			outVertexCount++;
		}

		// If either position is exactly on the near plane continue.
		if (s_clipPos0->z == 1.0f || s_clipPos1->z == 1.0f)
		{
			s_clipPos0 = s_clipPos1;
			s_clipPos1++;

			#if defined(CLIP_INTENSITY)
				s_clipIntensity0 = s_clipIntensity1;
				s_clipIntensity1++;
			#endif
			#if defined(CLIP_UV)
				s_clipUv0 = s_clipUv1;
				s_clipUv1++;
			#endif

			continue;
		}

		// Finally clip the edge against the near plane, generating a new vertex.
		if (s_clipPos0->z < 1.0f || s_clipPos1->z < 1.0f)
		{
			const f32 z0 = s_clipPos0->z;
			const f32 z1 = s_clipPos1->z;

			// Parametric clip coordinate.
			s_clipParam = (1.0f - z0) / (z1 - z0);
			// new vertex Z coordinate should be exactly 1.0
			s_clipPosOut[outVertexCount].z = 1.0f;
			s_clipPosOut[outVertexCount].y = s_clipPos0->y + s_clipParam*(s_clipPos1->y - s_clipPos0->y);
			s_clipPosOut[outVertexCount].x = s_clipPos0->x + s_clipParam*(s_clipPos1->x - s_clipPos0->x);

			COMPUTE_CLIP_INTERSECTION(outVertexCount);
			outVertexCount++;
		}

		s_clipPos0 = s_clipPos1;
		s_clipPos1++;

		#if defined(CLIP_INTENSITY)
			s_clipIntensity0 = s_clipIntensity1;
			s_clipIntensity1++;
		#endif
		#if defined(CLIP_UV)
			s_clipUv0 = s_clipUv1;
			s_clipUv1++;
		#endif
	}
	// Return if the polygon is clipping away.
	if (outVertexCount < 3)
	{
		return 0;
	}
	s32 srcVertexCount = SWAP_CLIP_BUFFERS(outVertexCount);
	outVertexCount = 0;

	///////////////////////////////////////////////////
	// Left Clip Plane
	///////////////////////////////////////////////////
	for (s32 i = 0; i < srcVertexCount; i++)
	{
		s_clipPlanePos0 = -s_clipPos0->z * s_rcfltState.nearPlaneHalfLen;
		s_clipPlanePos1 = -s_clipPos1->z * s_rcfltState.nearPlaneHalfLen;
		if (s_clipPos0->x < s_clipPlanePos0 && s_clipPos1->x < s_clipPlanePos1)
		{
			s_clipPos0 = s_clipPos1;
			s_clipPos1++;

			#if defined(CLIP_INTENSITY)
				s_clipIntensity0 = s_clipIntensity1;
				s_clipIntensity1++;
			#endif
			#if defined(CLIP_UV)
				s_clipUv0 = s_clipUv1;
				s_clipUv1++;
			#endif

			continue;
		}

		// Add vertex 0 if it is inside the plane.
		if (s_clipPos0->x >= s_clipPlanePos0)
		{
			s_clipPosOut[outVertexCount] = *s_clipPos0;
			
			#if defined(CLIP_INTENSITY)
				s_clipIntensityOut[outVertexCount] = *s_clipIntensity0;
			#endif
			#if defined(CLIP_UV)
				s_clipUvOut[outVertexCount] = *s_clipUv0;
			#endif

			outVertexCount++;
		}

		// Skip clipping if either vertex is touching the plane.
		if (s_clipPos0->x == s_clipPlanePos0 || s_clipPos1->x == s_clipPlanePos1)
		{
			s_clipPos0 = s_clipPos1;
			s_clipPos1++;

			#if defined(CLIP_INTENSITY)
				s_clipIntensity0 = s_clipIntensity1;
				s_clipIntensity1++;
			#endif
			#if defined(CLIP_UV)
				s_clipUv0 = s_clipUv1;
				s_clipUv1++;
			#endif

			continue;
		}

		// Clip the edge.
		if (s_clipPos0->x < s_clipPlanePos0 || s_clipPos1->x < s_clipPlanePos1)
		{
			const f32 x0 = s_clipPos0->x;
			const f32 x1 = s_clipPos1->x;
			const f32 z0 = s_clipPos0->z;
			const f32 z1 = s_clipPos1->z;

			const f32 dx = s_clipPos1->x - s_clipPos0->x;
			const f32 dz = s_clipPos1->z - s_clipPos0->z;

			s_clipParam0 = (x0*z1) - (x1*z0);
			s_clipParam1 = -dz*s_rcfltState.nearPlaneHalfLen - dx;

			s_clipIntersectZ = s_clipParam0;
			if (s_clipParam1 != 0)
			{
				s_clipIntersectZ = s_clipParam0 / s_clipParam1;
			}
			s_clipIntersectX = -s_clipIntersectZ * s_rcfltState.nearPlaneHalfLen;

			f32 p, p0, p1;
			if (TFE_Jedi::abs(dz) > TFE_Jedi::abs(dx))
			{
				p1 = s_clipPos1->z;
				p0 = s_clipPos0->z;
				p = s_clipIntersectZ;
			}
			else
			{
				p1 = s_clipPos1->x;
				p0 = s_clipPos0->x;
				p = s_clipIntersectX;
			}
			s_clipParam = (p - p0) / (p1 - p0);

			s_clipPosOut[outVertexCount].x = s_clipIntersectX;
			s_clipPosOut[outVertexCount].y = s_clipPos0->y + s_clipParam*(s_clipPos1->y - s_clipPos0->y);
			s_clipPosOut[outVertexCount].z = s_clipIntersectZ;

			COMPUTE_CLIP_INTERSECTION(outVertexCount);
			outVertexCount++;
		}
		s_clipPos0 = s_clipPos1;
		s_clipPos1++;

		#if defined(CLIP_INTENSITY)
			s_clipIntensity0 = s_clipIntensity1;
			s_clipIntensity1++;
		#endif
		#if defined(CLIP_UV)
			s_clipUv0 = s_clipUv1;
			s_clipUv1++;
		#endif
	}
	if (outVertexCount < 3)
	{
		return 0;
	}
	srcVertexCount = SWAP_CLIP_BUFFERS(outVertexCount);
	outVertexCount = 0;

	///////////////////////////////////////////////////
	// Right Clip Plane
	///////////////////////////////////////////////////
	for (s32 i = 0; i < srcVertexCount; i++)
	{
		s_clipPlanePos0 = s_clipPos0->z * s_rcfltState.nearPlaneHalfLen;
		s_clipPlanePos1 = s_clipPos1->z * s_rcfltState.nearPlaneHalfLen;
		if (s_clipPos0->x > s_clipPlanePos0 && s_clipPos1->x > s_clipPlanePos1)
		{
			s_clipPos0 = s_clipPos1;
			s_clipPos1++;

			#if defined(CLIP_INTENSITY)
				s_clipIntensity0 = s_clipIntensity1;
				s_clipIntensity1++;
			#endif
			#if defined(CLIP_UV)
				s_clipUv0 = s_clipUv1;
				s_clipUv1++;
			#endif

			continue;
		}

		// Add vertex 0 if it is inside the plane.
		if (s_clipPos0->x <= s_clipPlanePos0)
		{
			s_clipPosOut[outVertexCount] = *s_clipPos0;
			
			#if defined(CLIP_INTENSITY)
				s_clipIntensityOut[outVertexCount] = *s_clipIntensity0;
			#endif
			#if defined(CLIP_UV)
				s_clipUvOut[outVertexCount] = *s_clipUv0;
			#endif

			outVertexCount++;
		}

		// Skip clipping if either vertex is touching the plane.
		if (s_clipPos0->x == s_clipPlanePos0 || s_clipPos1->x == s_clipPlanePos1)
		{
			s_clipPos0 = s_clipPos1;
			s_clipPos1++;

			#if defined(CLIP_INTENSITY)
				s_clipIntensity0 = s_clipIntensity1;
				s_clipIntensity1++;
			#endif
			#if defined(CLIP_UV)
				s_clipUv0 = s_clipUv1;
				s_clipUv1++;
			#endif

			continue;
		}

		// Clip the edge.
		if (s_clipPos0->x > s_clipPlanePos0 || s_clipPos1->x > s_clipPlanePos1)
		{
			const f32 x0 = s_clipPos0->x;
			const f32 x1 = s_clipPos1->x;
			const f32 z0 = s_clipPos0->z;
			const f32 z1 = s_clipPos1->z;

			const f32 dx = s_clipPos1->x - s_clipPos0->x;
			const f32 dz = s_clipPos1->z - s_clipPos0->z;

			s_clipParam0 = (x0*z1) - (x1*z0);
			s_clipParam1 = s_rcfltState.nearPlaneHalfLen*dz - dx;

			s_clipIntersectZ = s_clipParam0;
			if (s_clipParam1 != 0)
			{
				s_clipIntersectZ = s_clipParam0 / s_clipParam1;
			}
			s_clipIntersectX = s_rcfltState.nearPlaneHalfLen * s_clipIntersectZ;

			f32 p, p0, p1;
			if (TFE_Jedi::abs(dz) > TFE_Jedi::abs(dx))
			{
				p1 = s_clipPos1->z;
				p0 = s_clipPos0->z;
				p = s_clipIntersectZ;
			}
			else
			{
				p1 = s_clipPos1->x;
				p0 = s_clipPos0->x;
				p = s_clipIntersectX;
			}
			s_clipParam = (p - p0) / (p1 - p0);

			s_clipPosOut[outVertexCount].x = s_clipIntersectX;
			s_clipPosOut[outVertexCount].y = s_clipPos0->y + s_clipParam*(s_clipPos1->y - s_clipPos0->y);
			s_clipPosOut[outVertexCount].z = s_clipIntersectZ;

			COMPUTE_CLIP_INTERSECTION(outVertexCount);
			outVertexCount++;
		}
		s_clipPos0 = s_clipPos1;
		s_clipPos1++;

		#if defined(CLIP_INTENSITY)
			s_clipIntensity0 = s_clipIntensity1;
			s_clipIntensity1++;
		#endif
		#if defined(CLIP_UV)
			s_clipUv0 = s_clipUv1;
			s_clipUv1++;
		#endif
	}
	if (outVertexCount < 3)
	{
		return 0;
	}

	srcVertexCount = SWAP_CLIP_BUFFERS(outVertexCount);
	outVertexCount = 0;

	///////////////////////////////////////////////////
	// Top Clip Plane
	///////////////////////////////////////////////////
	for (s32 i = 0; i < srcVertexCount; i++)
	{
		s_clipY0 = s_rcfltState.yPlaneTop * s_clipPos0->z;
		s_clipY1 = s_rcfltState.yPlaneTop * s_clipPos1->z;

		// If the edge is completely behind the plane, then continue.
		if (s_clipPos0->y < s_clipY0 && s_clipPos1->y < s_clipY1)
		{
			s_clipPos0 = s_clipPos1;
			s_clipPos1++;

			#if defined(CLIP_INTENSITY)
				s_clipIntensity0 = s_clipIntensity1;
				s_clipIntensity1++;
			#endif
			#if defined(CLIP_UV)
				s_clipUv0 = s_clipUv1;
				s_clipUv1++;
			#endif

			continue;
		}
		// Add vertex 0 if it is inside the plane.
		if (s_clipPos0->y > s_clipY0)
		{
			s_clipPosOut[outVertexCount] = *s_clipPos0;
			
			#if defined(CLIP_INTENSITY)
				s_clipIntensityOut[outVertexCount] = *s_clipIntensity0;
			#endif
			#if defined(CLIP_UV)
				s_clipUvOut[outVertexCount] = *s_clipUv0;
			#endif

			outVertexCount++;
		}

		// Skip clipping if either vertex is touching the plane.
		if (s_clipPos0->y == s_clipY0 || s_clipPos1->y == s_clipY1)
		{
			s_clipPos0 = s_clipPos1;
			s_clipPos1++;

			#if defined(CLIP_INTENSITY)
				s_clipIntensity0 = s_clipIntensity1;
				s_clipIntensity1++;
			#endif
			#if defined(CLIP_UV)
				s_clipUv0 = s_clipUv1;
				s_clipUv1++;
			#endif

			continue;
		}

		// Clip the edge.
		if (s_clipPos0->y < s_clipY0 || s_clipPos1->y < s_clipY1)
		{
			s_clipParam0 = (s_clipPos0->y*s_clipPos1->z) - (s_clipPos1->y*s_clipPos0->z);

			const f32 dy = s_clipPos1->y - s_clipPos0->y;
			const f32 dz = s_clipPos1->z - s_clipPos0->z;
			s_clipParam1 = s_rcfltState.yPlaneTop*dz - dy;

			s_clipIntersectZ = s_clipParam0;
			if (s_clipParam1 != 0)
			{
				s_clipIntersectZ = s_clipParam0 / s_clipParam1;
			}
			s_clipIntersectY = s_rcfltState.yPlaneTop * s_clipIntersectZ;
			const f32 aDz = TFE_Jedi::abs(s_clipPos1->z - s_clipPos0->z);
			const f32 aDy = TFE_Jedi::abs(s_clipPos1->y - s_clipPos0->y);

			f32 p, p0, p1;
			if (aDz > aDy)
			{
				p1 = s_clipPos1->z;
				p0 = s_clipPos0->z;
				p = s_clipIntersectZ;
			}
			else
			{
				p1 = s_clipPos1->y;
				p0 = s_clipPos0->y;
				p = s_clipIntersectY;
			}
			s_clipParam = (p - p0) / (p1 - p0);

			s_clipPosOut[outVertexCount].x = s_clipPos0->x + s_clipParam*(s_clipPos1->x - s_clipPos0->x);
			s_clipPosOut[outVertexCount].y = s_clipIntersectY;
			s_clipPosOut[outVertexCount].z = s_clipIntersectZ;

			COMPUTE_CLIP_INTERSECTION(outVertexCount);
			outVertexCount++;
		}
		s_clipPos0 = s_clipPos1;
		s_clipPos1++;

		#if defined(CLIP_INTENSITY)
			s_clipIntensity0 = s_clipIntensity1;
			s_clipIntensity1++;
		#endif
		#if defined(CLIP_UV)
			s_clipUv0 = s_clipUv1;
			s_clipUv1++;
		#endif
	}

	if (outVertexCount < 3)
	{
		return 0;
	}
	srcVertexCount = SWAP_CLIP_BUFFERS(outVertexCount);
	outVertexCount = 0;

	///////////////////////////////////////////////////
	// Bottom Clip Plane
	///////////////////////////////////////////////////
	for (s32 i = 0; i < srcVertexCount; i++)
	{
		s_clipY0 = s_rcfltState.yPlaneBot * s_clipPos0->z;
		s_clipY1 = s_rcfltState.yPlaneBot * s_clipPos1->z;

		// If the edge is completely behind the plane, then continue.
		if (s_clipPos0->y > s_clipY0 && s_clipPos1->y > s_clipY1)
		{
			s_clipPos0 = s_clipPos1;
			s_clipPos1++;

			#if defined(CLIP_INTENSITY)
				s_clipIntensity0 = s_clipIntensity1;
				s_clipIntensity1++;
			#endif
			#if defined(CLIP_UV)
				s_clipUv0 = s_clipUv1;
				s_clipUv1++;
			#endif

			continue;
		}

		// Add vertex 0 if it is inside the plane.
		if (s_clipPos0->y < s_clipY0)
		{
			s_clipPosOut[outVertexCount] = *s_clipPos0;
			
			#if defined(CLIP_INTENSITY)
				s_clipIntensityOut[outVertexCount] = *s_clipIntensity0;
			#endif
			#if defined(CLIP_UV)
				s_clipUvOut[outVertexCount] = *s_clipUv0;
			#endif

			outVertexCount++;
		}

		// Skip clipping if either vertex is touching the plane.
		if (s_clipPos0->y == s_clipY0 || s_clipPos1->y == s_clipY1)
		{
			s_clipPos0 = s_clipPos1;
			s_clipPos1++;

			#if defined(CLIP_INTENSITY)
				s_clipIntensity0 = s_clipIntensity1;
				s_clipIntensity1++;
			#endif
			#if defined(CLIP_UV)
				s_clipUv0 = s_clipUv1;
				s_clipUv1++;
			#endif

			continue;
		}

		// Clip the edge.
		if (s_clipPos0->y > s_clipY0 || s_clipPos1->y > s_clipY1)
		{
			s_clipParam0 = (s_clipPos0->y*s_clipPos1->z) - (s_clipPos1->y*s_clipPos0->z);

			const f32 dy = s_clipPos1->y - s_clipPos0->y;
			const f32 dz = s_clipPos1->z - s_clipPos0->z;
			s_clipParam1 = s_rcfltState.yPlaneBot*dz - dy;

			s_clipIntersectZ = s_clipParam0;
			if (s_clipParam1 != 0)
			{
				s_clipIntersectZ = s_clipParam0 / s_clipParam1;
			}
			s_clipIntersectY = s_rcfltState.yPlaneBot * s_clipIntersectZ;
			const f32 aDz = TFE_Jedi::abs(s_clipPos1->z - s_clipPos0->z);
			const f32 aDy = TFE_Jedi::abs(s_clipPos1->y - s_clipPos0->y);

			f32 p, p0, p1;
			if (aDz > aDy)
			{
				p1 = s_clipPos1->z;
				p0 = s_clipPos0->z;
				p = s_clipIntersectZ;
			}
			else
			{
				p1 = s_clipPos1->y;
				p0 = s_clipPos0->y;
				p = s_clipIntersectY;
			}
			s_clipParam = (p - p0) / (p1 - p0);

			s_clipPosOut[outVertexCount].x = s_clipPos0->x + s_clipParam*(s_clipPos1->x - s_clipPos0->x);
			s_clipPosOut[outVertexCount].y = s_clipIntersectY;
			s_clipPosOut[outVertexCount].z = s_clipIntersectZ;

			COMPUTE_CLIP_INTERSECTION(outVertexCount);
			outVertexCount++;
		}
		s_clipPos0 = s_clipPos1;
		s_clipPos1++;

		#if defined(CLIP_INTENSITY)
			s_clipIntensity0 = s_clipIntensity1;
			s_clipIntensity1++;
		#endif
		#if defined(CLIP_UV)
			s_clipUv0 = s_clipUv1;
			s_clipUv1++;
		#endif
	}
	if (outVertexCount < 3)
	{
		return 0;
	}

	if (pos != s_clipPosOut)
	{
		memcpy(pos, s_clipPosOut, outVertexCount * sizeof(vec3_float));
		#if defined(CLIP_INTENSITY)
			memcpy(intensity, s_clipIntensityOut, outVertexCount * sizeof(f32));
		#endif
		#if defined(CLIP_UV)
			memcpy(uv, s_clipUvOut, outVertexCount * sizeof(vec2_float));
		#endif
	}
	// There is a corner case where 0 z is produced which causes crashes on Windows.
	// (It is kind of weird that it doesn't affect DOS...)
	for (s32 i = 0; i < outVertexCount; i++)
	{
		pos[i].z = max(pos[i].z, 1.0f);
	}
	return outVertexCount;
}
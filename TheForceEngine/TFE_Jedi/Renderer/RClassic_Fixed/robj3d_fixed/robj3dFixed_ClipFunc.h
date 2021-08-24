//////////////////////////////////////////////////////////////////////
// Inline clip function to handle the various combinations.
//////////////////////////////////////////////////////////////////////

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
		const fixed16_16 i0 = *s_clipIntensity0;
		const fixed16_16 i1 = *s_clipIntensity1;
		s_clipIntensityOut[outVertexCount] = i0 + mul16(s_clipParam, i1 - i0);
	#endif
	#if defined(CLIP_UV)
		const vec2_fixed uv0 = *s_clipUv0;
		const vec2_fixed uv1 = *s_clipUv1;
		s_clipUvOut[outVertexCount].x = uv0.x + mul16(s_clipParam, uv1.x - uv0.x);
		s_clipUvOut[outVertexCount].z = uv0.z + mul16(s_clipParam, uv1.z - uv0.z);
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
s32 robj3d_clipPolygonGouraud(vec3_fixed* pos, s32* intensity, s32 count)
#elif !defined(CLIP_INTENSITY) && defined(CLIP_UV)
s32 robj3d_clipPolygonUv(vec3_fixed* pos, vec2_fixed* uv, s32 count)
#elif defined(CLIP_INTENSITY) && defined(CLIP_UV)
s32 robj3d_clipPolygonUvGouraud(vec3_fixed* pos, vec2_fixed* uv, s32* intensity, s32 count)
#else
s32 robj3d_clipPolygon(vec3_fixed* pos, s32 count)
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
		if (s_clipPos0->z < ONE_16 && s_clipPos1->z < ONE_16)
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
		if (s_clipPos0->z >= ONE_16)
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
		if (s_clipPos0->z == ONE_16 || s_clipPos1->z == ONE_16)
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
		if (s_clipPos0->z < ONE_16 || s_clipPos1->z < ONE_16)
		{
			const fixed16_16 z0 = s_clipPos0->z;
			const fixed16_16 z1 = s_clipPos1->z;

			// Parametric clip coordinate.
			s_clipParam = div16(ONE_16 - z0, z1 - z0);
			// new vertex Z coordinate should be exactly 1.0
			s_clipPosOut[outVertexCount].z = ONE_16;
			s_clipPosOut[outVertexCount].y = s_clipPos0->y + mul16(s_clipParam, s_clipPos1->y - s_clipPos0->y);
			s_clipPosOut[outVertexCount].x = s_clipPos0->x + mul16(s_clipParam, s_clipPos1->x - s_clipPos0->x);

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
		s_clipPlanePos0 = -s_clipPos0->z;
		s_clipPlanePos1 = -s_clipPos1->z;
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
			const fixed16_16 x0 = s_clipPos0->x;
			const fixed16_16 x1 = s_clipPos1->x;
			const fixed16_16 z0 = s_clipPos0->z;
			const fixed16_16 z1 = s_clipPos1->z;

			const fixed16_16 dx = s_clipPos1->x - s_clipPos0->x;
			const fixed16_16 dz = s_clipPos1->z - s_clipPos0->z;

			s_clipParam0 = mul16(x0, z1) - mul16(x1, z0);
			s_clipParam1 = -dz - dx;

			s_clipIntersectZ = s_clipParam0;
			if (s_clipParam1 != 0)
			{
				s_clipIntersectZ = div16(s_clipParam0, s_clipParam1);
			}
			s_clipIntersectX = -s_clipIntersectZ;

			fixed16_16 p, p0, p1;
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
			s_clipParam = div16(p - p0, p1 - p0);

			s_clipPosOut[outVertexCount].x = s_clipIntersectX;
			s_clipPosOut[outVertexCount].y = s_clipPos0->y + mul16(s_clipParam, s_clipPos1->y - s_clipPos0->y);
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
		s_clipPlanePos0 = s_clipPos0->z;
		s_clipPlanePos1 = s_clipPos1->z;
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
			const fixed16_16 x0 = s_clipPos0->x;
			const fixed16_16 x1 = s_clipPos1->x;
			const fixed16_16 z0 = s_clipPos0->z;
			const fixed16_16 z1 = s_clipPos1->z;

			const fixed16_16 dx = s_clipPos1->x - s_clipPos0->x;
			const fixed16_16 dz = s_clipPos1->z - s_clipPos0->z;

			s_clipParam0 = mul16(x0, z1) - mul16(x1, z0);
			s_clipParam1 = dz - dx;

			s_clipIntersectZ = s_clipParam0;
			if (s_clipParam1 != 0)
			{
				s_clipIntersectZ = div16(s_clipParam0, s_clipParam1);
			}
			s_clipIntersectX = s_clipIntersectZ;

			fixed16_16 p, p0, p1;
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
			s_clipParam = div16(p - p0, p1 - p0);

			s_clipPosOut[outVertexCount].x = s_clipIntersectX;
			s_clipPosOut[outVertexCount].y = s_clipPos0->y + mul16(s_clipParam, s_clipPos1->y - s_clipPos0->y);	// edx
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
		s_clipY0 = mul16(s_yPlaneTop_Fixed, s_clipPos0->z);
		s_clipY1 = mul16(s_yPlaneTop_Fixed, s_clipPos1->z);

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
			s_clipParam0 = mul16(s_clipPos0->y, s_clipPos1->z) - mul16(s_clipPos1->y, s_clipPos0->z);

			const fixed16_16 dy = s_clipPos1->y - s_clipPos0->y;
			const fixed16_16 dz = s_clipPos1->z - s_clipPos0->z;
			s_clipParam1 = mul16(s_yPlaneTop_Fixed, dz) - dy;

			s_clipIntersectZ = s_clipParam0;
			if (s_clipParam1 != 0)
			{
				s_clipIntersectZ = div16(s_clipParam0, s_clipParam1);
			}
			s_clipIntersectY = mul16(s_yPlaneTop_Fixed, s_clipIntersectZ);
			const fixed16_16 aDz = TFE_Jedi::abs(s_clipPos1->z - s_clipPos0->z);
			const fixed16_16 aDy = TFE_Jedi::abs(s_clipPos1->y - s_clipPos0->y);

			fixed16_16 p, p0, p1;
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
			s_clipParam = div16(p - p0, p1 - p0);

			s_clipPosOut[outVertexCount].x = s_clipPos0->x + mul16(s_clipParam, s_clipPos1->x - s_clipPos0->x);
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
		s_clipY0 = mul16(s_yPlaneBot_Fixed, s_clipPos0->z);
		s_clipY1 = mul16(s_yPlaneBot_Fixed, s_clipPos1->z);

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
			s_clipParam0 = mul16(s_clipPos0->y, s_clipPos1->z) - mul16(s_clipPos1->y, s_clipPos0->z);

			const fixed16_16 dy = s_clipPos1->y - s_clipPos0->y;
			const fixed16_16 dz = s_clipPos1->z - s_clipPos0->z;
			s_clipParam1 = mul16(s_yPlaneBot_Fixed, dz) - dy;

			s_clipIntersectZ = s_clipParam0;
			if (s_clipParam1 != 0)
			{
				s_clipIntersectZ = div16(s_clipParam0, s_clipParam1);
			}
			s_clipIntersectY = mul16(s_yPlaneBot_Fixed, s_clipIntersectZ);
			const fixed16_16 aDz = TFE_Jedi::abs(s_clipPos1->z - s_clipPos0->z);
			const fixed16_16 aDy = TFE_Jedi::abs(s_clipPos1->y - s_clipPos0->y);

			fixed16_16 p, p0, p1;
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
			s_clipParam = div16(p - p0, p1 - p0);

			s_clipPosOut[outVertexCount].x = s_clipPos0->x + mul16(s_clipParam, s_clipPos1->x - s_clipPos0->x);
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
		memcpy(pos, s_clipPosOut, outVertexCount * sizeof(vec3_fixed));
		#if defined(CLIP_INTENSITY)
			memcpy(intensity, s_clipIntensityOut, outVertexCount * sizeof(fixed16_16));
		#endif
		#if defined(CLIP_UV)
			memcpy(uv, s_clipUvOut, outVertexCount * sizeof(vec2_fixed));
		#endif
	}
	return outVertexCount;
}
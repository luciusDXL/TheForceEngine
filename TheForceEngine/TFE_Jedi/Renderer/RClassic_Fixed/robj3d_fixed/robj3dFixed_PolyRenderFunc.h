//////////////////////////////////////////////////////////////////////
// Inline polygon render functions
//////////////////////////////////////////////////////////////////////

#if defined(POLY_INTENSITY) && !defined(POLY_UV)
s32 robj3d_findNextEdgeI(s32 xMinIndex, s32 xMin)
#elif !defined(POLY_INTENSITY) && defined(POLY_UV)
s32 robj3d_findNextEdgeT(s32 xMinIndex, s32 xMin)
#elif defined(POLY_INTENSITY) && defined(POLY_UV)
s32 robj3d_findNextEdgeTI(s32 xMinIndex, s32 xMin)
#else
s32 robj3d_findNextEdge(s32 xMinIndex, s32 xMin)
#endif
{
	s32 prevScanlineLen = s_edgeTopLength;
	s32 curIndex = xMinIndex;

	// The min and max indices should not match, otherwise it is an error.
	if (xMinIndex == s_polyMaxIndex)
	{
		s_edgeTopLength = prevScanlineLen;
		return -1;
	}

	while (1)
	{
		s32 nextIndex = curIndex + 1;
		if (nextIndex >= s_polyVertexCount) { nextIndex = 0; }
		else if (nextIndex < 0) { nextIndex = s_polyVertexCount - 1; }

		vec3_fixed* cur = &s_polyProjVtx[curIndex];
		vec3_fixed* next = &s_polyProjVtx[nextIndex];
		s32 dx = next->x - cur->x;
		if (next->x == s_maxScreenX)
		{
			dx++;
		}

		if (dx > 0)
		{
			s_edgeTopLength = dx;

			const fixed16_16 step = div16(ONE_16, intToFixed16(dx));
			s_edgeTopY0_Pixel = cur->y;
			s_edgeTop_Y0 = intToFixed16(cur->y);

			const fixed16_16 dy = intToFixed16(next->y - cur->y);
			s_edgeTop_dYdX = mul16(dy, step);

			const fixed16_16 dz = next->z - cur->z;
			s_edgeTop_dZdX = mul16(dz, step);
			s_edgeTop_Z0 = cur->z;

			#if defined(POLY_INTENSITY)
				s_edgeTop_I0 = clamp(s_polyIntensity[curIndex], 0, VSHADE_MAX_INTENSITY);
				const fixed16_16 dI = s_polyIntensity[nextIndex] - s_edgeTop_I0;
				s_edgeTop_dIdX = mul16(dI, step);
			#endif
			#if defined(POLY_UV)
				s_edgeTop_Uv0 = s_polyUv[curIndex];
				const fixed16_16 dU = s_polyUv[nextIndex].x - s_edgeTop_Uv0.x;
				const fixed16_16 dV = s_polyUv[nextIndex].z - s_edgeTop_Uv0.z;
				s_edgeTop_dUVdX.x = mul16(dU, step);
				s_edgeTop_dUVdX.z = mul16(dV, step);
			#endif

			s_edgeTopIndex = nextIndex;
			return 0;
		}
		else if (nextIndex == s_polyMaxIndex)
		{
			s_edgeTopLength = prevScanlineLen;
			return -1;
		}
		curIndex = nextIndex;
	}

	// This shouldn't be reached, but just in case.
	s_edgeTopLength = prevScanlineLen;
	return -1;
}

#if defined(POLY_INTENSITY) && !defined(POLY_UV)
s32 robj3d_findPrevEdgeI(s32 minXIndex)
#elif !defined(POLY_INTENSITY) && defined(POLY_UV)
s32 robj3d_findPrevEdgeT(s32 minXIndex)
#elif defined(POLY_INTENSITY) && defined(POLY_UV)
s32 robj3d_findPrevEdgeTI(s32 minXIndex)
#else
s32 robj3d_findPrevEdge(s32 minXIndex)
#endif
{
	const s32 len = s_edgeBotLength;
	s32 curIndex = minXIndex;
	if (minXIndex == s_polyMaxIndex)
	{
		s_edgeBotLength = len;
		return -1;
	}

	while (1)
	{
		s32 prevIndex = curIndex - 1;
		if (prevIndex >= s_polyVertexCount) { prevIndex = 0; }
		else if (prevIndex < 0) { prevIndex = s_polyVertexCount - 1; }

		vec3_fixed* cur = &s_polyProjVtx[curIndex];
		vec3_fixed* prev = &s_polyProjVtx[prevIndex];
		s32 dx = prev->x - cur->x;
		if (s_maxScreenX == prev->x)
		{
			dx++;
		}

		if (dx > 0)
		{
			s_edgeBotLength = dx;

			const fixed16_16 step = div16(ONE_16, intToFixed16(dx));
			s_edgeBotY0_Pixel = cur->y;
			s_edgeBot_Y0 = intToFixed16(cur->y);

			const s32 dy = prev->y - cur->y;
			s_edgeBot_dYdX = mul16(intToFixed16(dy), step);

			const fixed16_16 dz = prev->z - cur->z;
			s_edgeBot_dZdX = div16(dz, intToFixed16(dx));
			s_edgeBot_Z0 = cur->z;
						
			#if defined(POLY_INTENSITY)
			s_edgeBot_I0 = clamp(s_polyIntensity[curIndex], 0, VSHADE_MAX_INTENSITY);
			const fixed16_16 dI = s_polyIntensity[prevIndex] - s_edgeBot_I0;
			s_edgeBot_dIdX = mul16(dI, step);
			#endif
			#if defined(POLY_UV)
			s_edgeBot_Uv0 = s_polyUv[curIndex];
			const fixed16_16 dU = s_polyUv[prevIndex].x - s_edgeBot_Uv0.x;
			const fixed16_16 dV = s_polyUv[prevIndex].z - s_edgeBot_Uv0.z;
			s_edgeBot_dUVdX.x = mul16(dU, step);
			s_edgeBot_dUVdX.z = mul16(dV, step);
			#endif

			s_edgeBotIndex = prevIndex;

			return 0;
		}
		else
		{
			curIndex = prevIndex;
			if (prevIndex == s_polyMaxIndex)
			{
				s_edgeBotLength = len;
				return -1;
			}
		}
	}
	s_edgeBotLength = len;
	return -1;
}

#if !defined(POLY_INTENSITY) && !defined(POLY_UV)
void robj3d_drawColumnFlatColor()
{
	s32 end = s_columnHeight - 1;
	s32 offset = end * s_width;
	for (s32 i = end; i >= 0; i--, offset -= s_width)
	{
		s_pcolumnOut[offset] = s_polyColorIndex;
	}
}
#endif

#if defined(POLY_INTENSITY) && !defined(POLY_UV)
void robj3d_drawColumnShadedColor()
{
	const u8* colorMap = s_polyColorMap;

	fixed16_16 intensity = s_col_I0;
	u8  colorIndex = s_polyColorIndex;
	s32 dither = s_dither;

	s32 end = s_columnHeight - 1;
	s32 offset = end * s_width;
	for (s32 i = end; i >= 0; i--, offset -= s_width)
	{
		s32 pixelIntensity = floor16(intensity);
		if (dither)
		{
			const fixed16_16 iOffset = intensity - s_ditherOffset;
			if (iOffset >= 0)
			{
				pixelIntensity = floor16(iOffset);
			}
		}
		s_pcolumnOut[offset] = colorMap[(pixelIntensity&31)*256 + colorIndex];

		intensity += s_col_dIdY;
		dither = !dither;
	}
}
#endif

#if !defined(POLY_INTENSITY) && defined(POLY_UV)
void robj3d_drawColumnFlatTexture()
{
	const u8* colorMap = &s_polyColorMap[s_polyColorIndex * 256];
	const u8* textureData = s_polyTexture->image;
	const s32 texHeight = s_polyTexture->height;
	const s32 texWidthMask = s_polyTexture->width - 1;
	const s32 texHeightMask = texHeight - 1;

	fixed16_16 U = s_col_Uv0.x;
	fixed16_16 V = s_col_Uv0.z;
	
	s32 end = s_columnHeight - 1;
	s32 offset = end * s_width;
	for (s32 i = end; i >= 0; i--, offset -= s_width)
	{
		const u8 colorIndex = textureData[(floor16(U)&texWidthMask)*texHeight + (floor16(V)&texHeightMask)];
		s_pcolumnOut[offset] = colorMap[colorIndex];

		U += s_col_dUVdY.x;
		V += s_col_dUVdY.z;
	}
}
#endif

#if defined(POLY_INTENSITY) && defined(POLY_UV)
void robj3d_drawColumnShadedTexture()
{
	const u8* colorMap = s_polyColorMap;
	const u8* textureData = s_polyTexture->image;
	const s32 texHeight = s_polyTexture->height;
	const s32 texWidthMask = s_polyTexture->width - 1;
	const s32 texHeightMask = texHeight - 1;

	fixed16_16 U = s_col_Uv0.x;
	fixed16_16 V = s_col_Uv0.z;
	fixed16_16 I = s_col_I0;

	s32 end = s_columnHeight - 1;
	s32 offset = end * s_width;
	for (s32 i = end; i >= 0; i--, offset -= s_width)
	{
		const u8 colorIndex = textureData[(floor16(U)&texWidthMask)*texHeight + (floor16(V)&texHeightMask)];
		const s32 pixelIntensity = floor16(I)&31;
		s_pcolumnOut[offset] = colorMap[pixelIntensity*256 + colorIndex];

		I += s_col_dIdY;
		U += s_col_dUVdY.x;
		V += s_col_dUVdY.z;
	}
}
#endif

#undef FIND_NEXT_EDGE
#undef FIND_PREV_EDGE
#undef DRAW_COLUMN
#if defined(POLY_INTENSITY) && !defined(POLY_UV)
#define FIND_NEXT_EDGE robj3d_findNextEdgeI
#define FIND_PREV_EDGE robj3d_findPrevEdgeI
#define DRAW_COLUMN robj3d_drawColumnShadedColor
#elif !defined(POLY_INTENSITY) && defined(POLY_UV)
#define FIND_NEXT_EDGE robj3d_findNextEdgeT
#define FIND_PREV_EDGE robj3d_findPrevEdgeT
#define DRAW_COLUMN robj3d_drawColumnFlatTexture
#elif defined(POLY_INTENSITY) && defined(POLY_UV)
#define FIND_NEXT_EDGE robj3d_findNextEdgeTI
#define FIND_PREV_EDGE robj3d_findPrevEdgeTI
#define DRAW_COLUMN robj3d_drawColumnShadedTexture
#else
#define FIND_NEXT_EDGE robj3d_findNextEdge
#define FIND_PREV_EDGE robj3d_findPrevEdge
#define DRAW_COLUMN robj3d_drawColumnFlatColor
#endif

#if defined(POLY_INTENSITY) && !defined(POLY_UV)
void robj3d_drawShadedColorPolygon(vec3_fixed* projVertices, fixed16_16* intensity, s32 vertexCount, u8 color)
#elif !defined(POLY_INTENSITY) && defined(POLY_UV)
void robj3d_drawFlatTexturePolygon(vec3_fixed* projVertices, vec2_fixed* uv, s32 vertexCount, TextureData* texture, u8 color)
#elif defined(POLY_INTENSITY) && defined(POLY_UV)
void robj3d_drawShadedTexturePolygon(vec3_fixed* projVertices, vec2_fixed* uv, fixed16_16* intensity, s32 vertexCount, TextureData* texture)
#else
void robj3d_drawFlatColorPolygon(vec3_fixed* projVertices, s32 vertexCount, u8 color)
#endif
{
	s32 xMax = INT_MIN;
	s32 xMin = INT_MAX;
	s_polyProjVtx = projVertices;
	s_polyVertexCount = vertexCount;

	#if defined(POLY_INTENSITY)
		s_polyIntensity = intensity;
	#endif
	#if defined(POLY_UV)
		s_polyUv = uv;
		s_polyTexture = texture;
	#endif

	s32 yMax = xMax;
	s32 yMin = xMin;
	if (vertexCount <= 0) { return; }

	// Compute the 2D bounding box of the polygon.
	// Track the extreme vertex indices in X.
	s32 minXIndex;
	vec3_fixed* projVertex = projVertices;
	for (s32 i = 0; i < s_polyVertexCount; i++, projVertex++)
	{
		const s32 x = projVertex->x;
		if (x < xMin)
		{
			xMin = x;
			minXIndex = i;
		}
		if (x > xMax)
		{
			xMax = x;
			s_polyMaxIndex = i;
		}

		const s32 y = projVertex->y;
		if (y < yMin) { yMin = y; }
		if (y > yMax) { yMax = y; }
	}

	// If the polygon is too small or off screen, skip it.
	if (xMin >= xMax || yMin > s_windowMaxY_Pixels || yMax < s_windowMinY_Pixels) { return; }

	assert(s_colorMap);
	s_polyColorMap = s_colorMap;
	#if !(defined(POLY_INTENSITY) && defined(POLY_UV))
		s_polyColorIndex = color;
	#endif
	#if defined(POLY_INTENSITY) && !defined(POLY_UV)
		s_ditherOffset = HALF_16;
	#endif
	s_columnX = xMin;

	if (FIND_NEXT_EDGE(minXIndex, xMin) != 0 || FIND_PREV_EDGE(minXIndex) != 0) { return; }

	for (s32 foundEdge = 0; !foundEdge && s_columnX >= s_minScreenX && s_columnX <= s_maxScreenX; s_columnX++)
	{
		const fixed16_16 edgeMinZ = min(s_edgeBot_Z0, s_edgeTop_Z0);
		const fixed16_16 z = s_rcfState.depth1d[s_columnX];

		// Is ave edge Z occluded by walls? Is column outside of the vertical area?
		if (edgeMinZ < z && s_edgeTopY0_Pixel <= s_windowMaxY_Pixels && s_edgeBotY0_Pixel >= s_windowMinY_Pixels)
		{
			const s32 winTop = s_objWindowTop[s_columnX];
			const s32 winBot = s_objWindowBot[s_columnX];
			s32 y0_Top = s_edgeTopY0_Pixel;
			s32 y0_Bot = s_edgeBotY0_Pixel;
			#if defined(POLY_INTENSITY) || defined(POLY_UV)
				fixed16_16 yOffset = 0;
			#endif

			if (y0_Top < winTop)
			{
				y0_Top = winTop;
			}
			if (y0_Bot > winBot)
			{
				#if defined(POLY_INTENSITY) || defined(POLY_UV)
					yOffset = intToFixed16(y0_Bot - winBot);
				#endif
				y0_Bot = winBot;
			}

			s_columnHeight = y0_Bot - y0_Top + 1;
			// TODO: Figure out why I have to add: s_columnX >= s_windowMinX && s_columnX <= s_windowMaxX
			// I must have missed a step elsewhere.
			if (s_columnHeight > 0 && s_columnX >= s_windowMinX && s_columnX <= s_windowMaxX)
			{
				const fixed16_16 height = intToFixed16(s_edgeBotY0_Pixel - s_edgeTopY0_Pixel + 1);
				s_pcolumnOut = &s_display[y0_Top*s_width + s_columnX];

				#if defined(POLY_INTENSITY)
					s_col_dIdY = div16(s_edgeTop_I0 - s_edgeBot_I0, height);
					s_col_I0 = s_edgeBot_I0;
					if (yOffset)
					{
						s_col_I0 += mul16(yOffset, s_col_dIdY);
					}
					s_dither = ((s_columnX & 1) ^ (y0_Bot & 1)) - 1;
				#endif

				#if defined(POLY_UV)
					s_col_dUVdY.x = div16(s_edgeTop_Uv0.x - s_edgeBot_Uv0.x, height);
					s_col_dUVdY.z = div16(s_edgeTop_Uv0.z - s_edgeBot_Uv0.z, height);
					s_col_Uv0 = s_edgeBot_Uv0;
					if (yOffset)
					{
						s_col_Uv0.x += mul16(yOffset, s_col_dUVdY.x);
						s_col_Uv0.z += mul16(yOffset, s_col_dUVdY.z);
					}
				#endif

				DRAW_COLUMN();
			}
		}

		s_edgeTopLength--;
		if (s_edgeTopLength <= 0)
		{
			foundEdge = FIND_NEXT_EDGE(s_edgeTopIndex, s_columnX);
		}
		else
		{
			#if defined(POLY_INTENSITY)
				s_edgeTop_I0 = clamp(s_edgeTop_I0 + s_edgeTop_dIdX, 0, VSHADE_MAX_INTENSITY);
			#endif
			#if defined(POLY_UV)
				s_edgeTop_Uv0.x += s_edgeTop_dUVdX.x;
				s_edgeTop_Uv0.z += s_edgeTop_dUVdX.z;
			#endif

			s_edgeTop_Y0 += s_edgeTop_dYdX;
			s_edgeTop_Z0 += s_edgeTop_dZdX;
			s_edgeTopY0_Pixel = round16(s_edgeTop_Y0);
		}
		if (foundEdge == 0)
		{
			s_edgeBotLength--;
			if (s_edgeBotLength <= 0)
			{
				foundEdge = FIND_PREV_EDGE(s_edgeBotIndex);
			}
			else
			{
				#if defined(POLY_INTENSITY)
				s_edgeBot_I0 = clamp(s_edgeBot_I0 + s_edgeBot_dIdX, 0, VSHADE_MAX_INTENSITY);
				#endif
				#if defined(POLY_UV)
					s_edgeBot_Uv0.x += s_edgeBot_dUVdX.x;
					s_edgeBot_Uv0.z += s_edgeBot_dUVdX.z;
				#endif

				s_edgeBot_Y0 += s_edgeBot_dYdX;
				s_edgeBot_Z0 += s_edgeBot_dZdX;
				s_edgeBotY0_Pixel = round16(s_edgeBot_Y0);
			}
		}
	}
}

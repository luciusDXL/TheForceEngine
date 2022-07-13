uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;

uniform samplerBuffer  Sectors;
uniform samplerBuffer  Walls;
uniform samplerBuffer  DrawListPos;
uniform usamplerBuffer DrawListData;
uniform samplerBuffer  DrawListPlanes;	// Top and Bottom planes for each portal.

// in int gl_VertexID;
out float gl_ClipDistance[6];
flat out vec4 Frag_Uv;
out vec3 Frag_Pos;
out vec4 Texture_Data;
flat out vec4 Frag_Color;
flat out int Frag_TextureId;

void unpackPortalInfo(uint portalInfo, out uint portalOffset, out uint portalCount)
{
	portalCount  = (portalInfo >> 13u) & 7u;
	portalOffset = portalInfo & 8191u;
}

void main()
{
	// We do our own vertex fetching and geometry expansion, so calculate the relevent values from the vertex ID.
	int partIndex = gl_VertexID / 4;
	int vertexId  = gl_VertexID & 3;
	
	// Read part position and data.
	vec4 positions = texelFetch(DrawListPos, partIndex);
	uvec4 data = texelFetch(DrawListData, partIndex);

	// Unpack part data.
	bool sky = (data.x & 32768u) != 0u;
	int partId = int(data.x & 32767u);

	int nextId   = int(data.x >> 16u);
	int sectorId = int(data.y);
	int lightOffset = int(data.z & 63u) - 32;
	bool flip    = (data.z & 64u) != 0u;

	uint portalOffset, portalCount;
	unpackPortalInfo(data.z >> 7u, portalOffset, portalCount);

	int wallId = int(data.w >> 16u);
	Frag_TextureId = int(data.w & 65535u);

	// Get the current sector heights.
	vec4 sectorData   = texelFetch(Sectors, sectorId*2);
	float floorHeight = sectorData.x;
	float ceilHeight  = sectorData.y;
	float sectorAmbient = sectorData.z;
	
	// Generate the output position and uv for the vertex.
	vec3 vtx_pos;
	vec4 vtx_uv = vec4(0.0);
	vec4 vtx_color = vec4(0.0, 0.0, sectorAmbient, flip ? 1.0 : 0.0);
	vec4 texture_data = vec4(0.0);
	float zbias = 0.0;
	#ifndef SECTOR_TRANSPARENT_PASS
	if (partId < 3)	// Wall
	#endif
	{
		vec2 vtx = (vertexId & 1)==0 ? positions.xy : positions.zw;
		vtx_pos = vec3(vtx.x, (vertexId < 2) ? ceilHeight : floorHeight, vtx.y);

		float texBase = floorHeight;
	#ifdef SECTOR_TRANSPARENT_PASS
		vtx_uv.y = 0.0;
		if (partId == 7)  // Mid Sign
		{
			vtx_uv.zw = texelFetch(Walls, wallId*3 + 1).zw;
			// Add a small z bias to avoid issues with the wall when clamped.
			zbias = -0.00005;
		}
		else if (partId == 8) // Top Sign
		{
			vtx_uv.zw = texelFetch(Walls, wallId*3 + 1).zw;

			float nextTop = texelFetch(Sectors, nextId*2).y;
			float curTop = min(floorHeight, max(nextTop, ceilHeight));
			vtx_pos.y = (vertexId < 2) ? ceilHeight : curTop;
			texBase = nextTop;
			// Add a small z bias to avoid issues with the wall when clamped.
			zbias = -0.00005;
		}
		else if (partId == 9) // Bottom Sign
		{
			vtx_uv.zw = texelFetch(Walls, wallId*3 + 1).zw;

			float nextBot = texelFetch(Sectors, nextId*2).x;
			float curBot = max(ceilHeight, min(nextBot, floorHeight));
			vtx_pos.y = (vertexId < 2) ? curBot : floorHeight;
			// Add a small z bias to avoid issues with the wall when clamped.
			zbias = -0.00005;
		}
		else  // Transparent Mid-texture
		{
			vtx_uv.zw = texelFetch(Walls, wallId*3 + 1).xy;
			vtx_uv.y = 2.0;

			if (nextId < 32768)
			{
				vec2 nextHeights = texelFetch(Sectors, nextId*2).xy;
				float y0 = min(floorHeight, max(nextHeights.y, ceilHeight));
				float y1 = max(ceilHeight, min(nextHeights.x, floorHeight));
				texBase = y1;

				// Compute final height value for this vertex.
				vtx_pos.y = (vertexId < 2) ? y0 : y1;
			}
		}
	#else  // !SECTOR_TRANSPARENT_PASS
		if (partId == 1) // Top
		{
			float nextTop = texelFetch(Sectors, nextId*2).y;
			float curTop = min(floorHeight, max(nextTop, ceilHeight));
			vtx_pos.y = (vertexId < 2) ? ceilHeight : curTop;
			vtx_uv.zw = texelFetch(Walls, wallId*3 + 2).zw;
			texBase = nextTop;
		}
		else if (partId == 2) // Bottom
		{
			float nextBot = texelFetch(Sectors, nextId*2).x;
			float curBot = max(ceilHeight, min(nextBot, floorHeight));
			vtx_pos.y = (vertexId < 2) ? curBot : floorHeight;
			vtx_uv.zw = texelFetch(Walls, wallId*3 + 2).xy;
		}
		else
		{
			vtx_uv.zw = texelFetch(Walls, wallId*3 + 1).xy;
		}

		vtx_uv.y = sky ? 3.0 : 2.0;
	#endif  // !SECTOR_TRANSPARENT_PASS

		texture_data = texelFetch(Walls, wallId*3);
		vtx_uv.x = texBase;
		
		vtx_color.r = float(lightOffset);
		vtx_color.g = 32.0;
	}
	#ifndef SECTOR_TRANSPARENT_PASS
	else if (partId < 5)	// flat
	{
		float extrusion = 200.0;
		int flatIndex = partId - 3;	// 0 = floor, 1 = ceiling.
		float y0 = (flatIndex==0) ? floorHeight : ceilHeight - extrusion;
		float y1 = (flatIndex==0) ? floorHeight + extrusion : ceilHeight;
		vec2 vtx = (vertexId & 1)==0 ? positions.xy : positions.zw;

		if (sky && nextId < 32768)
		{
			// TODO: This isn't *quite* right, since the sky should be extended to the top/bot of the frustum.
			vec2 nextHeights = texelFetch(Sectors, nextId*2).xy;
			if (flatIndex == 0) { y0 = max(floorHeight, nextHeights.x); }
			else { y1 = min(ceilHeight, nextHeights.y); }
		}

		vtx_pos  = vec3(vtx.x, (vertexId < 2) ? y0 : y1, vtx.y);
		vtx_color.r = 0.0;
		vtx_color.g = float(48 + 16*(1-flatIndex));

		// Store the relative plane height for the floor/ceiling projection in the fragment shader.
		float planeHeight = (flatIndex==0) ? floorHeight : ceilHeight;
		vtx_uv.x = planeHeight - CameraPos.y;
		vtx_uv.y = sky ? 3.0 : 1.0;

		vec4 sectorTexOffsets = texelFetch(Sectors, sectorId*2+1);
		texture_data.xy = (flatIndex == 0) ? sectorTexOffsets.xy : sectorTexOffsets.zw;

		// Add a small z bias to flats to avoid seams.
		zbias = -0.00005;
	}
	else // Cap
	{
		float extrusion = 200.0;
		int flatIndex = partId - 5;	// 0 = floor, 1 = ceiling.
		if (flatIndex == 0)
		{
			// This flips the polygon orientation so backface culling works.
			vertexId = (vertexId + 2) & 3;
		}

		vtx_pos.x = positions[2*(vertexId&1)];
		vtx_pos.z = positions[1+2*(vertexId/2)];
		vtx_pos.y = (flatIndex==0) ? floorHeight + extrusion : ceilHeight - extrusion;
		vtx_color.r = 0.0;
		vtx_color.g = float(48 + 16*(1-flatIndex));

		// Given the vertex position, compute the XZ position as the intersection between (camera->pos) and the plane at floor/ceiling height.
		float planeHeight = (flatIndex==0) ? floorHeight : ceilHeight;
		vtx_uv.x = planeHeight - CameraPos.y;
		vtx_uv.y = sky ? 3.0 : 1.0;

		vec4 sectorTexOffsets = texelFetch(Sectors, sectorId*2+1);
		texture_data.xy = (flatIndex == 0) ? sectorTexOffsets.xy : sectorTexOffsets.zw;
	}
	#endif  // !SECTOR_TRANSPARENT_PASS

	// Clipping.
	for (int i = 0; i < int(portalCount) && i < 6; i++)
	{
		vec4 plane = texelFetch(DrawListPlanes, int(portalOffset) + i);
		gl_ClipDistance[i] = dot(vec4(vtx_pos.xyz, 1.0), plane);
	}
	for (int i = int(portalCount); i < 6; i++)
	{
		gl_ClipDistance[i] = 1.0;
	}
	
	Frag_Pos = vtx_pos - CameraPos;
	
	// Transform from world to view space.
    vec3 vpos = (vtx_pos - CameraPos) * CameraView;
	gl_Position = vec4(vpos, 1.0) * CameraProj;
	gl_Position.z += zbias;

	// Write out the per-vertex uv and color.
	Frag_Uv = vtx_uv;
	Frag_Color = vtx_color;
	Texture_Data = texture_data;
}

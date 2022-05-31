uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;

// Sector data - TODO move to buffer.
uniform ivec4 SectorData;
uniform vec4  SectorData2;
uniform vec4  SectorBounds;

uniform usamplerBuffer SectorWalls;
uniform samplerBuffer  SectorVertices;
uniform samplerBuffer  Sectors;

// in int gl_VertexID;
out vec2 Frag_Uv;
out vec4 Frag_Color;
void main()
{
	// We do our own vertex fetching and geometry expansion, so calculate the relevent values from the vertex ID.
	int quadId    = gl_VertexID / 4;
	int vertexId  = gl_VertexID & 3;
	int partIndex = quadId + SectorData.x;

	// Fetch wall part data - this includes indices into the vertex data, partID (mid, top, bottom, etc.),
	// the adjoined sector ID (to compute height values), and textureID.
	uvec4 wall = texelFetch(SectorWalls, partIndex);

	// GLSL is really stubborn about uint vs int values, so pre-cast upfront
	// to avoid hassles in the rest of the shader.
	ivec2 vtxIdx = ivec2(wall.xy);
	int partId = int(wall.z & 65535u);
	int nextId = int(wall.z >> 16u);
	
	// Generate the output position and uv for the vertex.
	vec3 vtx_pos;
	vec2 vtx_uv = vec2(0.0);
	vec4 vtx_color = vec4(0.5, 0.5, 0.5, 1.0);
	if (partId < 3)	// Wall
	{
		vec2 vtx0 = texelFetch(SectorVertices, vtxIdx[0]).rg;
		vec2 vtx  = texelFetch(SectorVertices, vtxIdx[vertexId & 1]).rg;

		vtx_pos = vec3(vtx.x, (vertexId < 2) ? SectorData2.y : SectorData2.x, vtx.y);
		if (partId == 1) // Top
		{
			float nextTop = texelFetch(Sectors, nextId).y;
			float curTop = min(SectorData2.x, max(nextTop, SectorData2.y));
			vtx_pos.y = (vertexId < 2) ? SectorData2.y : curTop;
		}
		else if (partId == 2) // Bottom
		{
			float nextBot = texelFetch(Sectors, nextId).x;
			float curBot = max(SectorData2.y, min(nextBot, SectorData2.x));
			vtx_pos.y = (vertexId < 2) ? curBot : SectorData2.x;
		}

		vtx_uv.x = length(vtx - vtx0) / 8.0;
		vtx_uv.y = (vtx_pos.y - SectorData2.x) / 8.0;
		vtx_color.rgb = vec3(float(wall.w) / 31.0);
	}
	else if (partId < 5)	// flat
	{
		int flatIndex = partId - 3;	// 0 = floor, 1 = ceiling.
		float y0 = (flatIndex==0) ? SectorData2.x : SectorData2.y - 200.0;
		float y1 = (flatIndex==0) ? SectorData2.x + 200.0 : SectorData2.y;

		vec2 vtx = texelFetch(SectorVertices, vtxIdx[vertexId & 1]).rg;
		vtx_pos  = vec3(vtx.x, (vertexId < 2) ? y0 : y1, vtx.y);
		vtx_color.rgb = vec3(float(wall.w) / 31.0);
		vtx_color.rg *= vec2(0.5 * float(flatIndex) + 0.4);
	}
	else // Cap
	{
		int flatIndex = partId - 5;	// 0 = floor, 1 = ceiling.
		if (flatIndex == 0)
		{
			vertexId = (vertexId + 2) & 3;
		}

		vtx_pos.x = SectorBounds[2*(vertexId&1)];
		vtx_pos.z = SectorBounds[1+2*(vertexId/2)];
		vtx_pos.y = (flatIndex==0) ? SectorData2.x + 200.0 : SectorData2.y - 200.0;
		vtx_color.rgb = vec3(float(wall.w) / 31.0);
		vtx_color.rg *= vec2(0.5 * float(flatIndex) + 0.4);
	}
	
	// Transform from world to view space.
    vec3 vpos = (vtx_pos - CameraPos) * CameraView;
	gl_Position = vec4(vpos, 1.0) * CameraProj;

	// Write out the per-vertex uv and color.
	Frag_Uv = vtx_uv;
	Frag_Color = vtx_color;
}

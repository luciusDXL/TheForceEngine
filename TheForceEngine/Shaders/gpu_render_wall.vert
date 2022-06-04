uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;

uniform samplerBuffer  Sectors;
uniform samplerBuffer  DrawListPos;
uniform usamplerBuffer DrawListData;

// in int gl_VertexID;
out vec2 Frag_Uv;
out vec4 Frag_Color;
void main()
{
	// We do our own vertex fetching and geometry expansion, so calculate the relevent values from the vertex ID.
	int partIndex = gl_VertexID / 4;
	int vertexId  = gl_VertexID & 3;
	
	// Read part position and data.
	vec4 positions = texelFetch(DrawListPos, partIndex);
	uvec4 data = texelFetch(DrawListData, partIndex);

	// Unpack part data.
	int partId   = int(data.x & 0xffffu);
	int nextId   = int(data.x >> 16u);
	int sectorId = int(data.y);
	int ambient  = int(data.z);

	// Get the current sector heights.
	vec4 sectorData = texelFetch(Sectors, sectorId);
	float floorHeight = sectorData.x;
	float ceilHeight = sectorData.y;
	
	// Generate the output position and uv for the vertex.
	vec3 vtx_pos;
	vec2 vtx_uv = vec2(0.0);
	vec4 vtx_color = vec4(0.5, 0.5, 0.5, 1.0);
	if (partId < 3)	// Wall
	{
		vec2 vtx = (vertexId & 1)==0 ? positions.xy : positions.zw;
		vtx_pos = vec3(vtx.x, (vertexId < 2) ? ceilHeight : floorHeight, vtx.y);

		if (partId == 1) // Top
		{
			float nextTop = texelFetch(Sectors, nextId).y;
			float curTop = min(floorHeight, max(nextTop, ceilHeight));
			vtx_pos.y = (vertexId < 2) ? ceilHeight : curTop;
		}
		else if (partId == 2) // Bottom
		{
			float nextBot = texelFetch(Sectors, nextId).x;
			float curBot = max(ceilHeight, min(nextBot, floorHeight));
			vtx_pos.y = (vertexId < 2) ? curBot : floorHeight;
		}

		vtx_uv.xy = vtx.xy / vec2(8.0);
		vtx_color.rgb = vec3(float(ambient) / 31.0);
	}
	else if (partId < 5)	// flat
	{
		int flatIndex = partId - 3;	// 0 = floor, 1 = ceiling.
		float y0 = (flatIndex==0) ? floorHeight : ceilHeight - 200.0;
		float y1 = (flatIndex==0) ? floorHeight + 200.0 : ceilHeight;

		vec2 vtx = (vertexId & 1)==0 ? positions.xy : positions.zw;
		vtx_pos  = vec3(vtx.x, (vertexId < 2) ? y0 : y1, vtx.y);
		vtx_color.rgb = vec3(float(ambient) / 31.0);
		vtx_color.rg *= vec2(0.5 * float(flatIndex) + 0.4);
	}
	else // Cap
	{
		int flatIndex = partId - 5;	// 0 = floor, 1 = ceiling.
		if (flatIndex == 0)
		{
			vertexId = (vertexId + 2) & 3;
		}

		vtx_pos.x = positions[2*(vertexId&1)];
		vtx_pos.z = positions[1+2*(vertexId/2)];
		vtx_pos.y = (flatIndex==0) ? floorHeight + 200.0 : ceilHeight - 200.0;
		vtx_color.rgb = vec3(float(ambient) / 31.0);
		vtx_color.rg *= vec2(0.5 * float(flatIndex) + 0.4);
	}
	
	// Transform from world to view space.
    vec3 vpos = (vtx_pos - CameraPos) * CameraView;
	gl_Position = vec4(vpos, 1.0) * CameraProj;

	// Write out the per-vertex uv and color.
	Frag_Uv = vtx_uv;
	Frag_Color = vtx_color;
}

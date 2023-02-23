// #ifdef DYNAMIC_LIGHTING
#include "Shaders/lighting.h"
// #endif

uniform vec4 ScaleOffset;
uniform vec3 CameraPos;

in vec4 vtx_pos;
in vec4 vtx_color;

out vec2 Frag_Uv;		// base uv coordinates (0 - 1)
flat out vec4 Frag_TextureId_Color;
flat out vec3 Frag_Lighting;

void main()
{
	Frag_Uv = vtx_pos.zw;
	Frag_TextureId_Color = vtx_color;
	Frag_Lighting = vec3(0.0);

	// Lighting - since color is normally ignored, we use it as a flag to enable lighting.
	int textureId = int(floor(Frag_TextureId_Color.x * 255.0 + 0.5) + floor(Frag_TextureId_Color.y * 255.0 + 0.5)*256.0 + 0.5);
	int color     = int(Frag_TextureId_Color.z * 255.0 + 0.5);
	if (textureId < 65535 && color > 0)
	{
		vec3 posWS = CameraPos;
		Frag_Lighting = handleLightingSprite(posWS, CameraPos, /*applyDesaturate*/false);
	}

	vec2 pos = vtx_pos.xy*ScaleOffset.xy + ScaleOffset.zw;
	gl_Position = vec4(pos.xy, 0, 1);
}

in vec4 Frag_Color;
in vec2 Frag_Uv;
in vec3 Frag_Pos;
out vec4 Out_Color;

uniform sampler2D image;
uniform vec3 CameraPos;
uniform ivec2 isTexturedProj;
uniform vec4 texProj;
uniform vec2 texOffset;
uniform vec4 tint;

void main()
{
	vec4 outColor = Frag_Color;
	if (isTexturedProj.x == int(1))
	{
		vec2 uv = Frag_Uv;
		if (isTexturedProj.y == int(1))
		{
			// Simple intersection for the editor.
			uv.xy = (Frag_Pos.xz - texOffset) / vec2(-8.0, 8.0);
		}
		outColor = texture(image, uv);
		if (outColor.a < 0.1)
		{
			discard;
		}
	}
    Out_Color = outColor * tint;
}

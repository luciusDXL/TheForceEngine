uniform sampler2D Image;
uniform vec4 Tint;

in vec2 Frag_UV;
out vec4 Out_Color;

void main()
{
	vec4 color = texture(Image, Frag_UV) * Tint;
	Out_Color.rgb = color.rgb * vec3(color.a);
	Out_Color.a = color.a;
}

uniform sampler2D VirtualDisplay;

in vec2 Frag_UV;
out vec4 Out_Color;

void main()
{
	vec2 offset = vec2(0.5) / vec2(textureSize(VirtualDisplay, 0));

	vec4 color0 = texture(VirtualDisplay, Frag_UV + vec2(-offset.x,  offset.y));
	vec4 color1 = texture(VirtualDisplay, Frag_UV + vec2( offset.x,  offset.y));
	vec4 color2 = texture(VirtualDisplay, Frag_UV + vec2(-offset.x, -offset.y));
	vec4 color3 = texture(VirtualDisplay, Frag_UV + vec2( offset.x, -offset.y));
	vec4 color = 0.25 * (color0 + color1 + color2 + color3);

	Out_Color.rgb = color.rgb * max(1.0/256.0, clamp(2.0 * (color.a - 0.5), 0.0, 1.0));
	Out_Color.a = 1.0;
}

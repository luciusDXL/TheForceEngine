/////////////////////////////////////////////////////////////
// A basic script to test Vector Math.
/////////////////////////////////////////////////////////////

void main()
{
	// Create float2 variables.
	float2 a(1.0, 2.0);
	float2 b(3.0, 4.0);
	system.print("a = {}", a);
	system.print("b = {}", b);
	// Basic math.
	float2 c = a * b + a;
	system.print("c = {}", c);
	system.print("c.yx = {}", c.yx);
	// Scalar/Per-component math.
	float2 d = c.yx * 0.5 + a.x + 0.25;
	system.print("{}: {}*0.5 + {} + 0.25 = {}", "c.yx*0.5 + a.x + 0.25", c.yx, a.x, d);
	// HLSL-style swizzles.
	float2 e = d.xy;
	float2 f = d.yx;
	float2 g = c.xx;
	float2 h = a.yy;
	// Test compound with accessors.
	float2 r(-1);
	float2 s(1, 2);
	float2 t(2, 3);
	
	t += s.xy;
	r += t.yx;
	// Expected: r = (4, 2), s = (1, 2), t = (3, 5)
	system.print("r = {}, s = {}, t = {}", r, s, t);
	// Array access
	float2 w(1, 2);
	w[0] += 1.0;
	w[1] -= 1.0;
	// Expected (2, 1)
	system.print("w[0] = {}, w[1] = {}", w[0], w[1]);
}

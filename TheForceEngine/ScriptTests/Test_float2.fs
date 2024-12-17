/////////////////////////////////////////////////////////////
// Vector Math Tests
/////////////////////////////////////////////////////////////

// Swizzle
void test_float2_swizzle()
{
	test.beginTest("float2_swizzle");
	float2 a = float2(1, 2);
	// float2 -> float2 swizzles.
	test.expectEq(a.xy, float2(1,2));
	test.expectEq(a.yx, float2(2,1));
	test.expectEq(a.xx, float2(1,1));
	test.expectEq(a.yy, float2(2,2));
	// float2 -> scalar swizzles.
	test.expectEq(a.x, 1.0f);
	test.expectEq(a.y, 2.0f);
	// float2 array indexing.
	test.expectEq(a[0], 1.0f);
	test.expectEq(a[1], 2.0f);
}

// float2 x float2 math
void test_float2_float2_math()
{
	test.beginTest("float2_float2_math");
	float2 a = float2(1, 2);
	float2 b = float2(3, 4);
	float2 c = float2(0, 1);
	test.expectEq(a + b, float2(4, 6));
	test.expectEq(a - b, float2(-2, -2));
	test.expectEq(a * b, float2(3, 8));
	test.expectEq(a / b, float2(1.0/3.0, 2.0/4.0));

	const float neg45 = math.radians(-45.0); // -45 degrees in radians.
	test.expectEq(math.distance(a, b), 2.828427125f);
	test.expectEq(math.dot(a, b), 11.0f);
	test.expectEq(math.length(a), 2.2360679775f);
	test.expectEq(math.normalize(a), float2(0.4472135955, 0.894427191));
	test.expectEq(math.perp(a, b), -2.0f);
	test.expectEq(math.sincos(neg45), float2(-0.7071067812, 0.7071067812));
	
	// Test normalize with zero-vector (invalid).
	test.expectEq(math.normalize(float2(0)), float2(0));
	
	test.expectEq(math.clamp(a, float2(0.5,2.5), float2(2,3)), float2(1.0, 2.5));
	test.expectEq(math.clamp(a, 1.5, 3.0), float2(1.5, 2.0));
	test.expectEq(math.max(a, b), float2(3, 4));
	test.expectEq(math.max(a, 1.5), float2(1.5, 2));
	test.expectEq(math.min(a, b), float2(1, 2));
	test.expectEq(math.min(a, 1.5), float2(1, 1.5));
	test.expectEq(math.mix(a, b, 0.5), float2(2, 3));
	test.expectEq(math.mix(a, b, float2(0.5, 1.0)), float2(2, 4));

	test.expectTrue(math.all(a));
	test.expectTrue(math.any(a));
	test.expectFalse(math.all(c));
	test.expectTrue(math.any(c));
}

// float2 x scalar math
void test_float2_scalar_math()
{
	test.beginTest("float2_scalar_math");
	float2 a = float2(3, 4);
	test.expectEq(a * 0.0, float2(0));
	test.expectEq(a * 1.0, float2(3, 4));
	test.expectEq(a.yx * 0.5, float2(2.0, 1.5));
	
	test.expectEq(a / 2.0, float2(1.5, 2));
	test.expectEq(a + 2.0, float2(5, 6));
	test.expectEq(a - 2.0, float2(1, 2));
	test.expectEq(a * 2.0, float2(6, 8));
}

void main()
{
	test.beginSystem("Float2");
	{
		test_float2_swizzle();
		test_float2_float2_math();
		test_float2_scalar_math();
	}
	test.report();
}

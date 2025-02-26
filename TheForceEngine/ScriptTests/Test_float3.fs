/////////////////////////////////////////////////////////////
// Vector Math Tests
/////////////////////////////////////////////////////////////

// Swizzle
void test_float3_swizzle()
{
	test.beginTest("float3_swizzle");
	float3 a = float3(1, 2, 3);
	// float3 -> float3 swizzles.
	test.expectEq(a.xxx, float3(1,1,1));
	test.expectEq(a.xxy, float3(1,1,2));
	test.expectEq(a.xxz, float3(1,1,3));
	test.expectEq(a.xyx, float3(1,2,1));
	test.expectEq(a.xyy, float3(1,2,2));
	test.expectEq(a.xyz, float3(1,2,3));
	test.expectEq(a.xzx, float3(1,3,1));
	test.expectEq(a.xzy, float3(1,3,2));
	test.expectEq(a.xzz, float3(1,3,3));
	test.expectEq(a.yxx, float3(2,1,1));
	test.expectEq(a.yxy, float3(2,1,2));
	test.expectEq(a.yxz, float3(2,1,3));
	test.expectEq(a.yyx, float3(2,2,1));
	test.expectEq(a.yyy, float3(2,2,2));
	test.expectEq(a.yyz, float3(2,2,3));
	test.expectEq(a.yzx, float3(2,3,1));
	test.expectEq(a.yzy, float3(2,3,2));
	test.expectEq(a.yzz, float3(2,3,3));
	test.expectEq(a.zxx, float3(3,1,1));
	test.expectEq(a.zxy, float3(3,1,2));
	test.expectEq(a.zxz, float3(3,1,3));
	test.expectEq(a.zyx, float3(3,2,1));
	test.expectEq(a.zyy, float3(3,2,2));
	test.expectEq(a.zyz, float3(3,2,3));
	test.expectEq(a.zzx, float3(3,3,1));
	test.expectEq(a.zzy, float3(3,3,2));
    test.expectEq(a.zzz, float3(3,3,3));
	// float3 -> float2 swizzles.
	test.expectEq(a.xx, float2(1,1));
	test.expectEq(a.xy, float2(1,2));
	test.expectEq(a.xz, float2(1,3));
	test.expectEq(a.yx, float2(2,1));
	test.expectEq(a.yy, float2(2,2));
	test.expectEq(a.yz, float2(2,3));
	test.expectEq(a.zx, float2(3,1));
	test.expectEq(a.zy, float2(3,2));
	test.expectEq(a.zz, float2(3,3));
	// float3 -> scalar swizzles.
	test.expectEq(a.x, 1.0f);
	test.expectEq(a.y, 2.0f);
	test.expectEq(a.z, 3.0f);
	// float3 array indexing.
	test.expectEq(a[0], 1.0f);
	test.expectEq(a[1], 2.0f);
	test.expectEq(a[2], 3.0f);
}

// float3 x float3 math
void test_float3_float3_math()
{
	test.beginTest("float3_float3_math");
	float3 a = float3(1, 2, 3);
	float3 b = float3(3, 4, 5);
	float3 c = float3(0, 1, 2);
	test.expectEq(a + b, float3(4, 6, 8));
	test.expectEq(a - b, float3(-2, -2, -2));
	test.expectEq(a * b, float3(3, 8, 15));
	test.expectEq(a / b, float3(1.0/3.0, 2.0/4.0, 3.0/5.0));

	test.expectEq(math.distance(a, b), 3.4641016f);
	test.expectEq(math.dot(a, b), 26.0f);
	test.expectEq(math.length(a), 3.741657387f);
	test.expectEq(math.normalize(a), float3(0.267261237, 0.534522474, 0.801783681));
	test.expectEq(math.cross(a, b), float3(-2.0, 4.0, -2.0));
	
	// Test normalize with zero-vector (invalid).
	test.expectEq(math.normalize(float3(0)), float3(0));
	
	test.expectEq(math.clamp(a, float3(0.5,2.5,1.5), float3(2,3,2.75)), float3(1.0, 2.5, 2.75));
	test.expectEq(math.clamp(a, 1.5, 3.0), float3(1.5, 2.0, 3.0));
	test.expectEq(math.max(a, b), float3(3, 4, 5));
	test.expectEq(math.max(a, 1.5), float3(1.5, 2, 3));
	test.expectEq(math.min(a, b), float3(1, 2, 3));
	test.expectEq(math.min(a, 1.5), float3(1, 1.5, 1.5));
	test.expectEq(math.mix(a, b, 0.5), float3(2, 3, 4));
	test.expectEq(math.mix(a, b, float3(0.5, 1.0, 2.0)), float3(2, 4, 7));

	test.expectTrue(math.all(a));
	test.expectTrue(math.any(a));
	test.expectFalse(math.all(c));
	test.expectTrue(math.any(c));
}

// float3 x scalar math
void test_float3_scalar_math()
{
	test.beginTest("float3_scalar_math");
	float3 a = float3(3, 4, 5);
	test.expectEq(a * 0.0, float3(0));
	test.expectEq(a * 1.0, float3(3, 4, 5));
	test.expectEq(a.zyx * 0.5, float3(2.5, 2.0, 1.5));
	
	test.expectEq(a / 2.0, float3(1.5, 2, 2.5));
	test.expectEq(a + 2.0, float3(5, 6, 7));
	test.expectEq(a - 2.0, float3(1, 2, 3));
	test.expectEq(a * 2.0, float3(6, 8, 10));
}

void main()
{
	test.beginSystem("Float3");
	{
		test_float3_swizzle();
		test_float3_float3_math();
		test_float3_scalar_math();
	}
	test.report();
}

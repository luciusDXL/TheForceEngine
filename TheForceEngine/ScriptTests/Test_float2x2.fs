/////////////////////////////////////////////////////////////
// Vector Math Tests
/////////////////////////////////////////////////////////////

// Swizzle
void test_float2x2_arrayAccess()
{
	test.beginTest("float2x2_arrayAccess");
	float2x2 m = float2x2(1, 2, 3, 4);
	test.expectEq(m[0], float2(1, 2));
	test.expectEq(m[1], float2(3, 4));
}

// float2x2_math
void test_float2x2_math()
{
	test.beginTest("float2x2_math");
	float2x2 a = float2x2(1, 2, 3, 4);
	float2x2 b = float2x2(5, 6, 7, 8);
	
	float2x2 ta = math.transpose(a);
	float2x2 tb = math.transpose(b);
	test.expectEq(ta[0], float2(1, 3));
	test.expectEq(ta[1], float2(2, 4));
	test.expectEq(tb[0], float2(5, 7));
	test.expectEq(tb[1], float2(6, 8));
	
	// matrix multiply: float2x2 * float2x2 -> float2x2
	float2x2 ab = a * b;
	test.expectEq(ab[0], float2(19, 22));
	test.expectEq(ab[1], float2(43, 50));
			
	// scale: float2x2 * float -> float2x2
	float2x2 as = a * 2;
	test.expectEq(as[0], float2(2, 4));
	test.expectEq(as[1], float2(6, 8));
	
	// vector multiply: float2x2 * float2 -> float2 (float2 is column vector).
	float2 v = a * float2(1, 2);
	test.expectEq(v, float2(5, 11));
	
	float2x2 rm = math.rotationMatrix2x2(math.radians(-45.0));
	test.expectEq(rm[0], float2(0.7071067812, 0.7071067812));
	test.expectEq(rm[1], float2(-0.7071067812, 0.7071067812));
}

void main()
{
	test.beginSystem("Float2x2");
	{
		test_float2x2_arrayAccess();
		test_float2x2_math();
	}
	test.report();
}

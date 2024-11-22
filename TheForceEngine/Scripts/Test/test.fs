// Small test script.

void main()
{
	system_print("Test Script Start");
	for (int i=0; i<10; i++)
	{
		system_print("   Loop " + (i + 1));
		yield(3);
	}
	system_print("Test Script End");
}

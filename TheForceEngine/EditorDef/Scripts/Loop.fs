/////////////////////////////////////////////////////////////
// A basic script that shows loops & yield.
//
// This script counts up from 1 to 20 once per second and
// displays the number.
//
// Once the loop is complete, after 20 seconds, the script
// ends.
//
// To test in the editor, go to the Output window -> Script
// and type  runScript("Loop")  and then press Enter.
/////////////////////////////////////////////////////////////

void main()
{
	int[] id = { 1, 3, 4, 5, 6, 780, 103 };
	// Loop 20 times, once per second.
	for (int i = 0; i < 20; i++)
	{
		system.print("Loop# {}, id {}", (i + 1), id[i % id.length]);
		yield(1.0);
	}
}

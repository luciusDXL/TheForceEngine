/////////////////////////////////////////////////////////////
// Shortcuts for the Script Console in the TFE Level Editor.
//
// The full script API is available, but a bit cumbersome to
// use in a console.
/////////////////////////////////////////////////////////////

void runScript(string name) { system.runScript(name); }
void showScript(string name) { system.showScript(name); }
void print(string msg) { system.print(msg); }
void warning(string msg) { system.warning(msg); }
void error(string msg) { system.error(msg); }
void clear() { system.clearOutput(); }
void findSector(string name) { level.findSector(name); }

void help()
{
	system.print("=== Script Console Short-cuts ===");
	system.print("clear()");
	system.print("print(msg) - print(\"Test\")");
	system.print("runScript(name) - runScript(\"LevelInfo\")");
	system.print("showScript(name) - showScript(\"VectorMath\")");
	system.print("findSector(name) - findSector(\"RedDoor\")");
}
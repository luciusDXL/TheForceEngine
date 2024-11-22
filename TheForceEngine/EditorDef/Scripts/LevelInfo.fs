/////////////////////////////////////////////////////////////
// Print out basic level info to the Output.
/////////////////////////////////////////////////////////////

void main()
{
	string name = level.name;
	string slot = level.slot;
	int sectorCount = level.sectorCount;
	int entityCount = level.entityCount;
	int levelNoteCount = level.levelNoteCount;
	int guidelineCount = level.guidelineCount;
	int minLayer = level.minLayer;
	int maxLayer = level.maxLayer;
	float2 parallax = level.parallax;
	
	system.print("==== Level Info ====\n"
	             "Name: {}\n"
				 "Slot: {}\n"
				 "Sector Count: {}\n"
				 "Entity Count: {}\n"
				 "Note Count: {}\n"
				 "Guideline Count: {}\n"
				 "Parallax: {}\n"
				 "Layer Range: [{}, {}]",
				 name, slot, sectorCount, entityCount, levelNoteCount,
				 guidelineCount, parallax, minLayer, maxLayer);
}

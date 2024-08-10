/////////////////////////////////////////////////////////////
// Print out basic level info to the Output.
/////////////////////////////////////////////////////////////

void main()
{
	string name = level.getName();
	string slot = level.getSlot();
	int sectorCount = level.getSectorCount();
	int entityCount = level.getEntityCount();
	int levelNoteCount = level.getLevelNoteCount();
	int guidelineCount = level.getGuidelineCount();
	int minLayer = level.getMinLayer();
	int maxLayer = level.getMaxLayer();
	float2 parallax = level.getParallax();
	
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

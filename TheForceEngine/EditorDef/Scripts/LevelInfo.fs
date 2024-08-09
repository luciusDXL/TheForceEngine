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
	
	system.print("==== Level Info ====\n" +
	             "Name: " + name + "\n" +
				 "Slot: " + slot + "\n" +
				 "Sector Count: " + sectorCount + "\n" +
				 "Entity Count: " + entityCount + "\n" +
				 "Note Count: " + levelNoteCount + "\n" +
				 "Guideline Count: " + guidelineCount + "\n" +
				 "Parallax: " + toString(parallax) + "\n" +
				 "Layer Range: [" + minLayer + ", " + maxLayer + "]\n");
}

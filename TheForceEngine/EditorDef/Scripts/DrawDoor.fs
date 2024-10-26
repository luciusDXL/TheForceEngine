/////////////////////////////////////////////////////////////
// A basic script that shows how to draw a simple door.
//
// Note: It doesn't set textures or flags yet, but that is
// coming in the future.
/////////////////////////////////////////////////////////////

void main()
{
	// Get the selected entity position.
	float2 pos = selection.getPositionXZ();
	
	// Draw the door as 3 rectangles.
	draw.begin(pos);
	// Door frame.
	draw.rect(8,1);
	draw.moveForward(1);
	// Main door part.
	draw.rect(8,2);
	draw.moveForward(2);
	// Door frame.
	draw.rect(8,1);
}

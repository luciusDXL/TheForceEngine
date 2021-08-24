#pragma once
//////////////////////////////////////////////////////////////////////
// Elevator update function forward declarations.
//////////////////////////////////////////////////////////////////////
namespace TFE_Jedi
{
	typedef fixed16_16(*InfUpdateFunc)(InfElevator* elev, fixed16_16 delta);
	fixed16_16 infUpdate_moveHeights(InfElevator* elev, fixed16_16 delta);
	fixed16_16 infUpdate_moveWall(InfElevator* elev, fixed16_16 delta);
	fixed16_16 infUpdate_rotateWall(InfElevator* elev, fixed16_16 delta);
	fixed16_16 infUpdate_scrollWall(InfElevator* elev, fixed16_16 delta);
	fixed16_16 infUpdate_scrollFlat(InfElevator* elev, fixed16_16 delta);
	fixed16_16 infUpdate_changeAmbient(InfElevator* elev, fixed16_16 delta);
	fixed16_16 infUpdate_changeWallLight(InfElevator* elev, fixed16_16 delta);
}

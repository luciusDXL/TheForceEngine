# Simple Interactive Elements
There are two ways of creating interactive elements in Dark Forces levels - sector flags to cover a few simple cases and INF scripting. Sector flags can be used to create simple Doom-like doors and walls that can be blown up using explosives, such as Thermal Detonators. Other interactive elements, such as key doors, elevators, sliding doors, rotating doors, flashing lights, switches and more require INF scripting.

## Creating a Simple Door
To create a simple door go to Sector Mode and select the sector you wish to act like a basic door. Make sure the floor and ceiling heights are set to the open state. Then enable the **Door** flag using the sector panel on the right side of the view, above the texture/entity browser. The sector outline should turn Cyan on the map.

## Creating an Explosive Wall
To create a wall that blows up when throwing or firing explosives at it, create a sector that represents the passageway. Place a proper mid-texture on the inner and outer walls where frame 0 is the blown up state and frame 1 is the regular state. Make sure to enable the **Mask Wall** flag on those walls so that the texture is drawn as transparent. Finally select the sector and enable the **Exploding Wall** flag.

**Next:** [Inf Script Editor](local://Inf_Editor)  **Prev:** [Contents](local://InfScript)
## Contents
  * [Contents](local://InfScript)
  * [Simple Doors & Explosive Walls](local://Inf_SimpleInteractiveElements)
  * [INF Script Editor](local://Inf_Editor)
  * [Interactive Sectors and Walls](local://Inf_InteractiveElements)
  * [Triggers](local://Inf_Triggers)
  * [Elevators](local://Inf_Elevators)
  * [Teleporters](local://Inf_Teleporters)
  * [Variables](local://Inf_Variables)
  * [Functions](local://Inf_Functions)
  * [Tutorials](local://Inf_Tutorials)
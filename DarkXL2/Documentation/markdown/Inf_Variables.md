# Variables
Variables change the behavior of classes, controlling various aspects like the speed of movement, how it can be triggered, what key is needed to trigger it and more. Each Sub-Class has its own set of default values which are implied if the Variable is not displayed in the Edit Window. For example all Sub-Classes have master = on, so the master variable is only displayed if it has been turned off or manually added to the list using Add Variable. {Note: sound based variables are excluded until they are properly implemented}
### master
Determines if an Elevator or Trigger is active. If master is **off** then the script will ignore all functions except **master on**. For example if you want a continously scrolling conveyor belt that starts off and can be turned on using a switch, you can set the master variable to    **off** - which means it won't move until the switch calls **master on** with the conveyor as the target.
**Possible Values:** on / off
**Default:** on
### event
A custom event identifier that is sent to the wall or sector being activated from the trigger.
**Possible Values:** Power of 2 bit flag 1024 or higher.
**Default:** 0
### start
Determines which Stop an Elevator starts at when the level loads.
**Possible Values:** stop index.
**Default:** 0
### event_mask
Determines which events will operate on an Elevator or Trigger. Note multiple event_mask values can be combined.
**Possible Values:**
  * **cross front:** cross the wall line from the front.
  * **cross back:** cross the wall line from the back.
  * **enter:** enter the sector.
  * **leave:** leave the sector.
  * **nudge front/inside:** nudge (use) a wall from the front or sector from inside.
  * **nudge back/outside:** nudge (use) a wall from the back or sector from the outside.
  * **explosion:** an explosion activates the wall or sector.
  * **shoot:** shoot or punch a wall to activate it.
  * **land:** land on the floor of a sector to activate.
  * **custom:** a custom event, which is a power of 2 number 1024 or higher that identifies the event. This wall/sector will be only be triggered by a matching **event**.
**Default:** specific to the Trigger or Elevator.
### entity_mask
Determines which type of entity can activate a Trigger or Elevator.
**Possible Values:**
  * **enemy:** an enemy can activate.
  * **weapon:** a weapon can activate.
  * **player:** the player can activate.
**Default:** player
### key
Determines which key is required to manually activate the Elevator.
**Possible Values:** none, red, yellow, blue.
**Default:** none
### flags
Flags determine how things move horizontally with the sector and/or rotate - useful for scrolling floors and moving or rotating sectors. Note that regardless of this flag, objects will move with the floor height.
**Possible Values:**
  * **none:** nothing moves with the sector.
  * **move_floor:** objects move with the floor (rotation and translation).
  * **move_secalt:** objects move with the floor if on the second altitude.
**Default:** none unless specified by the Elevator sub-class.
### center
The center of rotation for the sector. This is a 2d vector, where the x and z values are specified. You can see the position of any point on the level by moving the mouse pointer and looking at the position readout near the top middle of the display.
**Possible Values:** (x, z) value in world units.
**Default:** (0.0, 0.0)
### angle
The angle in degrees of the movement direction for scrolling floors or ceilings, scrolling wall textures or moving sectors. For scrolling walls, 0 = down, 180 = up; for everything else 0 = north, 180 = south.
**Possible Values:** 0.0 - 360.0
**Default:** 0.0
### speed
Speed of movement in world units, degrees, light levels or texels per second, depending on the Elevator type.
**Possible Values:** float/decimal value representing units per second.
**Default:** 30.0
### target
The teleport target sector for teleporters.
**Possible Values:** sector name in level.
**Default:** ""

**Next:**  [Functions](local://Inf_Functions)  **Prev:** [Teleporters](local://Inf_Teleporters)
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
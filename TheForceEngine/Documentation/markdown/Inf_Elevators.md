# Class: Elevator
Elevators can only be assigned to sectors and include doors, moving floors, moving ceilings, sliding sectors, rotating sectors, changing light level, scrolling floor or ceiling textures and scrolling wall textures.

### Sub-Classes
### change_light
Changes the ambient light of a sector. Stop values are the ambient which is clamped betwee 0 (full darkness) to 31 (full bright).
**Event_Mask Default:** 0
### basic
Changes the Floor Height of the sector. This elevator honors the **Smart Object Reaction** sector flag - which will allow enemies to activate the elevator. Stop value is the height of the floor.
**Event_Mask Default:** enter, nudge front/insde, nudge back/outside
### inv
Changes the Ceiling Height of the sector. Often used for making doors since it honors the **Smart Object Reaction** flag (see basic above). Stop value is the height of the ceiling.
**Event_Mask Default:** enter, nudge front/insde, nudge back/outside
### move_floor
The same as basic except that the **Smart Object Reaction** has no effect - meaning enemies cannot activate this elevator even if that Sector flag is set.
**Event_Mask Default:** 0
### move_ceiling
The same as inv except that the **Smart Object Reaction** has no effect - meaning enemies cannot activate this elevator even if that sector flag is set.
**Event_Mask Default:** 0
### move_fc
Changes both the floor and ceiling height of the sector together. Stop values are the height of the floor. Note the same difference between floor and ceiling height will be maintained.
**Event_Mask Default:** 0
### scroll_floor
Scrolls the floor texture of a sector. The player moves with the texture by default unless overriden using the **Flags** variable. Stop values are in distances in pixels. The **Angle** variable determines which direction the texture scrolls in, where 0 = North and 180 = South.
**Flags Default:** move_floor
**Event_Mask Default:** 0
### scroll_ceiling
Scrolls the ceiling texture of a sector. Stop values are in distances in pixels. The **Angle** variable determines which direction the texture scrolls in, where 0 = North and 180 = South.
**Event_Mask Default:** 0
### move_offset
Changes the Second Altitude of a sector. Stop values are the second altitude, where 0 = floor altitude.
**Event_Mask Default:** 0
### basic_auto
Changes the Floor Altitude of a sector but returns to 0 after cyling through all of its stops. From there its event needs to be triggered twice to move it to its first stop again. Stop values are floor altitude.
**Event_Mask Default:** enter, nudge front/insde, nudge back/outside
### change_wall_light
Changes the **Light** level of any walls in the sector that have the **Change WallLight** flag set. **Light** is the wall light level relative to the sector ambient. Note that if the Sector ambient is 31 (fullbright) then this has no effect. Stop values are the value of the wall light.
**Event_Mask Default:** 0
### morph_move1
Translates the vertices of the walls in the sector that have the **Wall Morphs** flag set. The direction of movement is determined by the  **Angle** variable, where 0 = North and 180 = South. The stop value is the distance along the move direction. By default, the **Player** will **not** move relative to the wall movement, unless the **Flags** variable is changed.
**Flags Default:** none
**Event_Mask Default:** enter, leave, nudge front/insde, nudge back/outside
### morph_move2
The same as morph_move1 but the **Player** will move relative to the wall movement, unless the **Flags** variable is changed. Use this when creating moving walls or platforms that the player can stand on top of.
**Flags Default:** move_floor
**Event_Mask Default:** enter, leave, nudge front/insde, nudge back/outside
### morph_spin1
Rotates the vertex positions of any walls in the sector that have the **Wall Morphs** flag set. If the wall has adjoins, then those vertices will also rotate if their walls have the Wall Morphs flag set (which they should). Use the **Center** variable to set the center of rotation. Stops are the relative rotation angle in degrees. By default the **Player** will **not** rotate or move relative to the sector, though this can be overriden using the **Flags** variable.
**Flags Default:** none
**Event_Mask Default:** enter, leave, nudge front/insde, nudge back/outside
### morph_spin2
The same as morph_spin2 except that the **Player** will rotate relative to the sector - causing both the view point to rotate and the player's position to rotate relative to the **Center**. This behavior can be overriden using the **Flags** variable.
**Flags Default:** move_floor
**Event_Mask Default:** enter, leave, nudge front/insde, nudge back/outside
### move_wall
This is the same as morph_move1 except the event_mask defaults to 0 - meaning it cannot be directly activated.
**Flags Default:** none
**Event_Mask Default:** 0
### rotate_wall
This is the same as morph_spin1 except the event_mask defaults to 0 - meaning it cannot be directly activated.
**Flags Default:** none
**Event_Mask Default:** 0
### scroll_wall
Scrolls any textures of any walls in the sector that have the appropriate flags set - there is one flag for each wall part: **Scroll Top Tex, Scroll Mid Tex, Scroll Bottom Tex** and **Scroll Sign**. Stop values are in pixels along the scroll direction. The scroll direction is set by the **Angle** variable where 0 = down and 180 = up.
**Event_Mask Default:** 0
### door
This is a basic door, where stops and event_mask are setup automatically. By default this is exactly the same as using the **Door** sector flag. However this is still useful for requiring keys using the **Key** variable (such as the red key). You can also change other variables such as **Speed**.
**Event_Mask Default:** nudge back/outside
### door_mid
This is a door that has a part that opens upwards and a part that opens downwards from the center point between the floor and ceiling. Stops and event_mask are setup automatically. Make sure the sector is in the **open** state, it will be closed at level startup just like regular doors.
**Event_Mask Default:** nudge back/outside
### door_inv
This acts the same as a regular door but opens downwards instead of upwards.
**Event_Mask Default:** nudge back/outside

### Stops
There are basically two types of use cases for elevators - continuously scrolling textures or rotating sectors and elevators with discrete stops which allow you to control the timing, call functions at each stop and hold in place until activated - such as an elevator or door that the player activates by nudging it (i.e. using it). In the second case you will need to add one or more stops to your elevator.

Each stop is numbered and this is the way they are referenced by some Variables and Functions.

**Next:** [Teleporters](local://Inf_Teleporters)  **Prev:** [Triggers](local://Inf_Triggers)
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
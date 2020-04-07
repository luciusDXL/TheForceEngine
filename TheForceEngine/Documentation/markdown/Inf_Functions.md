# Functions
Functions can be called when a Trigger is activated or when an Elevator arrives at a Stop. More than one function can be executed in both of these cases. However there is a difference between Triggers and Elevators in regards to Functions: the **Target** of any function is the entire list of **Clients** whereas for Elevators functions can only have one **Target** which is specified as part of the Function setup.

Note also that not all Functions have targets - some cause a sound effect to be played (Page), a text message to be displayed or directly change the level in some way. Functions may have zero or more arguments, which are listed in paranthesis, seperated by commas. [opt] are optional parameters and can be left blank.
### m_trigger([opt]event)
**event** only elevators or triggers that have the event in their **event_mask** are triggered.
Trigger an elevator, causing it to move to its next stop or another trigger.
### goto_stop(stopIndex)
**stopIndex** target stop.
Causes the target elevator to move towards the target stop specified by stopIndex.
### next_stop([opt]event)
**event** only elevators that have the event in their **event_mask** are triggered.
Causes the target elevator to move towards its next stop.
### prev_stop([opt]event)
**event** only elevators that have the event in their **event_mask** are triggered.
Causes the target elevator to move towards its previous stop.
### master_on([opt]event)
**event** only elevators that have the event in their **event_mask** are triggered.
Turns on the target trigger or elevator if its master is currently off. Master_on also turns on enemy generators in the target sector.
### master_off([opt]event)
**event** only elevators that have the event in their **event_mask** are triggered.
Turns off the target trigger or elevator if its matster is currently on. Master_off also turns off enemy generators in the target sector.
### clear_bits(flags)
**flags** flags to clear.
Clears all of the specified flag bits from the Target. For example, flickering force fields may use this with setBits() to turn collision on and off on the wall.
### set_bits(flags)
**flags** flags to set.
Sets all of the specified flag bits from the Target. For example, flickering force fields may use this with setBits() to turn collision on and off on the wall.
### complete(goalNum)
**goalNum** goal number.
When setting up a level, the author can setup multiple level goals which are listed in the player's PDA. This function allows one of those goals to be marked as complete.
### done
Resets a switch so that it can be used again. Note this only works on **Switch1** triggers.
### wakeup
Any 3d object with a Vue that is currently paused in the target sector will wake up and continue animating.
### lights
Sets the sector ambient for **all sectors in the level** to the value of their "flags3" variable.
### adjoin(sector1, wall1, sector2, wall2)
**sector1** name of the first sector.
**wall1** index of the wall in the first sector.
**sector2** name of the second sector.
**wall2** index of the wall in the second sector.
Connects sector1, wall1 to sector2, wall2. This is useful for elevators that connect multiple floors, such as the main elevators in Detention Center.
### page(sound)
**sound** sound effect name
Plays the specified sound effect, if it exists. This is used in Dark Forces to have Kyle to make comments at certain events ("too easy").
### text(textNum)
**textNum** the number found in TEXT.MSG
Displays a text string in the main game window for a short time. The string is specified by **textNum** which refers to the strings in TEXT.MSG. Note that the UI allows you to pick from the list of strings directly so you don't have to remember the numbers.
### texture(flag, donor)
**flag** specifies whether to copy from the floor or ceiling.
**donor** the name of the sector to copy from.
Copies the floor or ceiling texture from the **donor** to the current sector's floor or ceiling.

**Next:**  [Tutorials](local://Inf_Tutorials)  **Prev:** [Variables](local://Inf_Variables)
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
# Class: Trigger
Triggers can be assigned to both sectors and walls. Each trigger has one or more functions and one or more clients. Note that when adding a new Trigger Class, the default function is **m_trigger**. You can select this function to change it. There are several sub-classes available.

### Sub-Classes
### standard
This is a basic trigger that can be applied to either a sector (usually enter, leave, nudge) or wall (usually cross or nudge). However this type of Trigger **cannot** be used as a Switch.
### default
This is the same as standard.
### switch1
This must be assigned to a wall and is designed to be a switch. As discussed in the **Creating Wall Triggers and Switches**, setup a **Sign texture** on the Wall where the first frame is the **Off** state and second frame is the **On** state. When the switch is triggered, the texture will change to the **On** state and cannot be pressed again until it is reset. To reset a switch, an INF script must call the **Done** function targeting this wall.
### single
This must be assigned to a wall and is designed to be a switch. As discussed in the **Creating Wall Triggers and Switches**, setup a **Sign texture** on the Wall where the first frame is the **Off** state and second frame is the **On** state. When the switch is triggered, the texture will change to the **On** state and cannot be pressed again. Unlike switch1, this type of switch cannot be reset.
### toggle
Similar to switch1 except that the switch can be toggled on and off.

### Setup
When setting up a trigger you will usually want to add an **Event_Mask** variable to control what events will activate the trigger - such as **Enter**, **Leave** or **Nudge**. If this is a trigger that can be activated by shooting at it, you will probably want to add an **Entity_Mask** variable and set it to **Player** and **Weapon**, so that the player can shoot it but not enemies. See the **Variables** section for a list of available variables and what they do.

The next thing to do, once setting up the Trigger's sub-class, function(s) and variables is to assign it **Clients** - that is what walls and sectors that will be targeted by the Trigger. To do this select Clients in the editor window and then use the Add Client UI. You can add them by sector name and wall number or selecting them on the map.

**Next:** [Elevators](local://Inf_Elevators)  **Prev:** [Interactive Sectors and Walls](local://Inf_InteractiveElements)
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
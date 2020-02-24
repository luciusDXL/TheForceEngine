# INF Script Editor
Editing is context sensitive, based on which item you have selected in the text window. For example, to add a new **Class** click on the **Seq** line in the text window. Then you will have controls to select a class and sub-class. To add things - such as clients, stops or variables to a class, select it in the text window and those options will become available.

In the **Edit Window** you should see two symbols: **Seq** and **Seqend** - this is the beginning and end of the script sequence for the selected sector or wall. To begin select **Seq** in the edit window. Now you have the option to add a **Template** or **Class** to the sequence. If you are editing a wall then the only class that is available is **Trigger**, otherwise **Elevator**, **Trigger** and **Teleporter** are available. Once you pick a **Class** from the list you will then need to select a **Sub-Class**. The Sub-Classes are detailed with each Class type below. Once a Sub-Class has been chosen, click on the **+** button to add it. You may add multiple classes to each wall or sector, such as setting a moving floor, ceiling and animated lighting all in the same sector.

Once the class has been added, it can be selected in the edit window. For **Triggers** you can add **Functions**, **Clients** and **Variables**. For **Elevators**, you can add **Stops**, **Slaves** and **Variables**. **Functions** are actions or messages applied to the game state or to specific targets. **Variables** change the behavior of Classes, such as setting the speed an elevator moves or which actions or entities can activate a trigger. **Slaves** are named sectors that are changed in the same way as the parent (i.e. move the same amount as a parent elevator, change light level the same way, etc.). **Clients** are the targets for trigger functions.

Items can be added to each category by selecting it in the editor and then using the controls. Similarly, clicking on a variable, function, client or slave allows you to edit it - changing its type, parameters or targets.

**Next:**  [Interactive Sectors and Walls](local://Inf_InteractiveElements)  **Prev:**  [Simple Doors & Explosive Walls](local://Inf_SimpleInteractiveElements)
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
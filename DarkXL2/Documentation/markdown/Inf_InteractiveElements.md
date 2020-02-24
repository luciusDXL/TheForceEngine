# Interactive Sectors and Walls
Most level interactivity is implemented by creating interactive sectors and walls.

## Creating Interactive Sectors
To create an interactive sector - such as a special door, elevator, conveyor belt, water or trigger - go to the Sector Mode and select the desired sector. Once selected make sure the sector has a name unique to the current level. It is a good idea to give it a distinct name so you can easily remember its purpose. If you do not give it a name, you will get an error message when trying to setup an INF script. Note that names are not required when using the **Door** or **Exploding Wall** flags.

Once you have named the sector, click on Edit INF Script - to the right of the name edit box - which will bring up the INF editor. From here you can use a pre-existing **Template** for common scripts such as keyed doors and simple elevators. Or you can setup one or more classes which include **Elevator**, **Trigger** and **Teleporter**. Each of these classes and its sub-classes are detailed in their own sections below.

## Creating Wall Triggers and Switches
To create a wall trigger or switch, first go to Sector Mode and select the sector that contains the wall. If the sector is not named, give it a name unique to this level. If you attempt to setup an INF script in a sector without a name an error message will pop up. Then go to Wall Mode and select the wall you wish to setup. If the wall is an **adjoin** - that is it is linking two sectors togther - then the side you choose is important.

If you want this wall to act like a switch then give it a **Sign texture**. The first frame of the sign texture represents its **off** state and the second frame its **on** state. Switches with more states are possible but require more work to setup and will be discussed later. If the wall is not a switch then you should skip this step. Note that Sign textures can be assigned to non-interactive walls as well and can be used like decals - similar to the Rebel symbol in Talay. Once your textures are all setup, click on the **Edit INF Script** button to the right of **Light Adjustment** which will open up the **INF editor**.

Next select **Seq** in the text box and add a **Trigger** class. Note that only Trigger classes are valid for walls. To see more detailed Trigger setup instructions see the Triggers below. Once you have setup your trigger, the wall should turn red on the map. Finally, like walls, you can use a **Template** for common Trigger types, such as switches, to make the setup easier.

**Next:**  [Triggers](local://Inf_Triggers)  **Prev:** [INF Script Editor](local://Inf_Editor)
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
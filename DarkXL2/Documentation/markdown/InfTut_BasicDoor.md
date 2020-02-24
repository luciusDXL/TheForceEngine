# Tutorial: Basic Door
In this tutorial we will be creating a basic door. There are two methods to create a door and we will be going through both.
## Setup
For this tutorial, we will be creating two rooms connected by a basic door. The door can be activated by "nudging" it, which is another way of saying you use it. Once it is activated, the door will open upwards, stay open for a few seconds and then close again.

{Show image of the setup in the editor}

First create three sectors as shown in the image above. The two large sectors will be our rooms and the small connecting sector is the door. Next set the floor and ceiling heights of the door sector to the desired heights **when the door is open.** Select the outer walls of the door sector and assign the door mid-texture.

{Image of the door texture assigned}

You can lower the door to its closed state to see what it looks like or to easily add the textures. However make sure to move it back to its open state before moving on.

{Image of the door sides}

Next assign the door side textures as shown in the image.
## Method 1: Sector Flag
Method 1 is very easy and what you should normally use when making simple doors. Select the door sector in the editor, which will open the Sector Panel. There you can see a list of sector flags. To make this sector a door, simply enable the **Door** flag. That's it.

{Image of the sector panel with door flag set and circled}

Note the the door sector outline should now be cyan on the map, indicating that it is now an interactive element.

{Image of door sector on map with cyan outline.}
## Method 2: Inf Script
Method 2 can be used when you want something a little different than the default door. Maybe its a key door or opens slower. For this tutorial we will stick to a simple door.

Build another set of rooms like the first. This way when you can see the result of both methods. You can connect the rooms with a hallway if you want to be able to try everything out in the same run.

This time, instead of setting a sector flag give the sector a name that is unique to this level. Make sure to name it something that makes sense and is recognizable to you.

{Show sector panel with door name}

Next open the INF Editor by clicking on the **Edit INF Script** to the right of the name field. You can return to the normal properties at any time by clicking on the **Return to Properties** button in the upper right. If you did not give the sector a name an error message will pop up instead of the INF Editor.

{Show INF editor, name edit window}

In the Edit Window, select **Seq** to add things to the script sequence. You should now have the option to **Add a Class** or **Create from Template**. For class select **Elevator** - which encompasses doors, elevators, scrolling textures, moving sectors and more. Now you will have the option to pick a **Sub-Class**, from there pick **Door**. And that's it, you should now have a fully functioning door. The sector outline should now appear cyan on the map like in Method 1.

{Image of the final state of the editor}

So why go through all this extra effort? Go to the next tutorial to find out! But first use the **Play** command to try out the level for yourself and make sure it works correctly in-game.

**Next:** [Red Key Door](local://InfTut_RedKeyDoor)  **Prev:** [Return to Tutorials](local://Inf_Tutorials)
## Contents
  * [Interactive Level Elements: Tutorials](local://Inf_Tutorials)
  * [Basic Door](local://InfTut_BasicDoor)
  * [Red Key Door](local://InfTut_RedKeyDoor)
  * [Door Opened by a Switch](local://InfTut_SwitchDoor)
  * [Basic Elevator](local://InfTut_BasicElevator)

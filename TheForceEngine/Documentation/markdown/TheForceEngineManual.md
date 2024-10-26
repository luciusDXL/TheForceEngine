## Getting Started
The current version of The Force Engine does not yet support Outlaws, that it is planned for version 2.0. You will find references to Outlaws in the TFE UI - such as the game data source. However the game is not yet playable.

The first step to using The Force Engine is to purchase and/or install Dark Forces. If you do not own the game, it is currently available at the [GOG](https://www.gog.com/) and [Steam](https://store.steampowered.com/) digital storefronts. The Force Engine will attempt to auto-detect your installations, in which case you can get started immediately. However, if this fails for whatever reason, you will need to setup you game data directories manually. To do this, return to the main menu and then select Settings > Game. You will see your current Game Source Data directories, which may be blank. You will need to select the directory that points to the original game's executable or source data - use the Browse button to bring up the file menu. The dialog will tell you which file(s) to select at the top.

Once your game source data is setup, select your Game to play - Dark Forces is the default, and currently only, option. While in the Settings menus, you can adjust Graphics settings such as game resolution and fullscreen/windowed mode, change key bindings and other input settings, adjust sound volume, and various other tweaks. If you have previously saved while playing using TFE, you can also load your saved game. When finished, select Return to go back to the main menu.

For more information and links or to download the latest build, visit [The Force Engine website](https://theforceengine.github.io/).

## Settings During Gameplay
Once you Start a game, you will be interfacing with the original game's UI. If you want to change settings, such as Graphics, Sound or Control options, or Save or Load a saved game, select the **Config** option in the Escape Menu or use the **Alt + F1** shortcut. This will also allow you to return to the main menu (quitting the game) so you can quickly change mods or in the future, change games. Most Graphics options will change the way the game is displayed immediately, which is a great way of tweaking the settings to your liking while seeing the results.

## Controlling Dark Forces
By default **Alt + F1** brings up the System UI and settings, **Alt + F5** will quick save if possible, and **Alt + F9** will quick load. Note that these bindings can be changed in the Input settings. There is also a console available, the default key is **`** or **~** depending on your region.

In Dark Forces while playing in a mission, by default **Escape** will bring up the menu and **F1** will bring up the PDA. By bringing up the menu and selecting **Config**, you can save or load your game, change settings, or exit the game and return to the main menu. By bringing up the PDA, you get access to the mission briefing, objectives screen, your **inventory** (needed for looking at **keycards** in certain levels, such as the **Dention Center**), your weapon loadout, and the map.

Dark Forces has many controls and options - such as a **headlamp** to see in dark areas, a **gas mask** (once you find the item) to survive in certain areas and more. Check the **Input** menu, under **Settings**, to see all of the possible actions. You can also rebind the actions to different keys and buttons, adjust mouse and controller sensitivity, change the mouse mode - which controls mouselook options, and more.

## Menu Options
**Start**
Starts the currently selected game.

**Manual**
Opens up this manual.

**Credits**
Displays the credits screen, which shows both individual contributors and libraries used with appropriate links.

**Settings**
Change settings such as source game data, graphics options, change sound volume, etc.

**Mods**
Select a mod to play, uses the currently selected game as the base. To get mods to show up in this menu, copy the zip files to the Mods/ directory found in your TFE directory. It is recommended that you leave each mod in the zip file, but if you wisth to extract the files, the mod will need to be in its own directory, under Mods/. For example, you might have a directory called `TFE_Install/Mods/Dark Tide 2/` which contains the Dark Tide 2 GOB, LFD and other files.

**Editor**
Open the built-in editors. Note that the editors are currently disabled due to large changes code structure changes that were required during development. The editors will be returning soon, however, so the option remains in the menu.

**Exit**
Exit the application.

## Accessibility Settings
The Accessibility Settings menu includes customization settings for closed captions/subtitles. You can separately enable or disable voice subtitles (e.g. "You there, stop where you are!") and captions (e.g. "[Mechanical death rattle]") for gameplay and cutscenes.
### Custom captions and fonts
It is possible to add custom caption files and fonts, for example to support subtitles in other languages.

**Custom caption files**
Custom caption files can be added to the `Captions` directory in your TFE install directory (e.g. **`C:\TheForceEngine\Captions`**) or in the TFE documents directory (e.g. **`C:\Users\UserName\Documents\TheForceEngine\Captions`** on Windows). The **`Captions`** directory in the TFE install directory includes a **`README.txt`** file with notes about the caption format. UTF-8 Unicode is supported, but make sure that you select a font which supports the necessary character set.

**Custom fonts**
Custom fonts in TrueType (.ttf) format can be added to the **`Fonts`** directory in your TFE install directory (e.g. **`C:\TheForceEngine\Fonts`**) or in the TFE documents directory (e.g. **`C:\Users\UserName\Documents\TheForceEngine\Fonts`** on Windows). Unicode character sets are supported.
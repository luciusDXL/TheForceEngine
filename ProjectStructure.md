This document covers the code and data structure for **The Force Engine** (TFE). Note that this is still in flux and parts of the code will be refactored in the future. But it should give you a decent overview of the code structure of the project.

# Main
Program entry point, manages initialization and OS main loop.

# Support Data
Support data includes:
* Documentation
* Fonts
* Libraries
* SDL 2
* Shaders
* SoundFonts (later)
* UI Images (for images displayed in the UI)
* UI Text (for text displayed in the UI, currently only Key/button names)

# TFE Framework
Each folder in the TFE framework is appended with `TFE_`. These folders represent the "meat" of project. There are two types of modules here: core framework with custom code that may use third party libraries in sub-folders and code derived from the original programs using reverse-engineering.

## Reverse-Engineered Code
Code in these folders was derived from reverse-engineering the original executables and libraries. This means that this code cannot be used in commercial projects without the express permission of the rights holders, Disney in the case.

### TFE_DarkForces
Dark Forces game code. The split between game code and engine code is a little fuzzy in the original, but this is my best guess as to the items that belong in the game code category. Future Outlaws work will reveal where I got it wrong and things may be shuffled around accordingly.

This code interfaces with the TFE framework, specifically memory management, switching "game" concept, file I/O, and similar items. Note that some of the asset loading is done here rather than `TFE_Asset/` but that will be cleaned up in the future.

The main entry point to the Dark Forces game is `darkForcesMain` which contains the main program loop and setup. Notable systems include `Actor` which handles AI, `GameUI` which handles the in-game UI, and `Landru` which is the cutscene and low-level 2D rendering system used by Dark Forces.

### TFE_Jedi
The JEDI engine code. Like above, the split between game code and engine code isn't always clear, so some systems may be shuffled around in the future.

The JEDI engine consists of several systems:
* **Collision** - the object/sector collision system.
* **iMuse** - the iMuse library reverse-engineered and embedded in the code. This handles low level digital audio and dynamic music.
* **InfSystem** - this handles dynamic level elements based on information found in level INF files. This handles switches, elevators, dynamic light changes, mission objective completion, etc.
* **Level** - level loading and level structure definitions used by the game and rendering code such as sectors, fonts, objects, textures, and walls.
* **Math** - core math including fixed-point, matrix math, cosine, and tangent tables used by the original game.
* **Memory** - JEDI memory structures - basically different list types.
* **Renderer** - the main rendering interface which includes the sector renderer, 3D object renderer, screen rendering (lines, blitting textures, etc.). This includes the original fixed-point renderer, a floating-point renderer derived from the fixed-point renderer for higher resolutions, and a GPU renderer.
* **Task** - the task system - a system used by Dark Forces to create small task functions that can be put to sleep and delayed. This is used extensively by the game code and INF system.

## TFE Framework
The framework code was not reverse-engineered, with the exception of some assets which needs to be cleaned up and/or moved, but was influenced by reverse-engineered code in some ways (i.e. making the framework support the game and engine).

The framework code includes handling File I/O, low level audio, system UI, input, memory management, post processing, settings, etc.

### TFE_Archive
This code handles archive formats such as ZIP, GOB, LFD, and LAB. The rest of the code interacts with archives using a common interface so there is little difference between source archive formats in the rest of the code.

### TFE_Asset
Handles asset loading and parsing. Note that some assets are located here and some in TFE_Jedi which needs to be cleaned up in the future.

### TFE_Audio
This system handles low-level digital audio mixing and midi playback. TFE creates and audio thread to process audio samples as needed and a midi thread that outputs midi commands to the midi hardware. Both of these threads provide callbacks so that the audio data can be filled or midi commands sent which are used by iMuse for Dark Forces.

### TFE_Editor
Currently disabled, but this is the TFE editor code - which includes asset viewers and the level editor. Most of the current code is in a branch but this will be cleaned up and properly enabled after version 1.0 is released.

### TFE_FileSystem
This system handles File I/O, and file path management.

### TFE_FrontEndUI
This should be renamed System UI - but it is the TFE System UI for handling input remapping, game source data locations, TFE rendering options, TFE sound options, and various other settings. Dark Forces "config" option now opens up the System UI so game settings can be handled in one place.

### TFE_Game
This system contains the reticle/crosshair code, as well as the "game" interface. The Game interface is used to handle starting, running, and stopping games so that TFE can support more than one. Current plans are to support Dark Forces and Outlaws.

### TFE_Input
The input system manages keyboard, mouse, and controller input and presents it to the game systems using a unified interface. The system also includes an input mapping system, so bindings can be re-assigned and adjusted.

### TFE_Memory
TFE uses *memory regions* in order to handle cleanly using and clearing blocks of memory without the overhead of many tiny memory allocations. Using this system, level memory is cleared between levels, simplifying memory management. Similarly game memory is cleared and reset, allowing for the user to switch games and mods at runtime.

### TFE_PostProcess
The post process system which is responsible for blitting the software renderer framebuffer or GPU renderer render target to the screen, color correction, system overlays such as the crosshair, and later postfx such as bloom.

### TFE_RenderBackend
The render backend provides an interface to the GPU hardware so the backend API can be changed without modifying the rest of the code (with the current exception of shader code). It handles vertex and index buffers, render targets, dynamic textures, shaders, render states, and similar systems. Currently the backend only supports OpenGL but there are plans for Vulkan and Metal support in the future.

### TFE_RenderShared
A place for GPU-based rendering code that is shared between the runtime and editors, such as 2D line drawing and blitting images.

### TFE_Settings
This system manages user settings such as game source locations, window and graphics settings, game settings, input mapping, and more. It also has support for registry access on Windows.

This system provides a single interface that any other place in the code can access in order to lookup settings.

### TFE_System
The core system module, this covers the logging system, thread management, non-game specific math, text file parser used by the settings INI parser and asset/game text file parsing, profiler, OS-specific timing, and TFE types.

### TFE_UI
The low-level UI system used by TFE for the System UI. This is built on top of imGUI, which is also included. This adds extra support such as markdown and file dialogs.

### Mods
The directory where mods go. Each mod must be placed in a separate sub‑directory.

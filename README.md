# The Force Engine (TFE)
[Website](https://theforceengine.github.io/)

## About
The Force Engine is a project to reverse engineer and rebuild the Jedi Engine for modern systems and the games that used that engine - **Dark Forces** and **Outlaws**. The project includes modern, built-in tools, such as a level editor and makes it easy to play **Dark Forces** and **Outlaws** on modern systems as well as the many community **mods** designed to work with the original games.

Playing Dark Forces or Outlaws using the Force Engine adds ease of use and modern features such as higher resolutions and modern control schemes such as mouse-look. Using the built-in tools allows for easier modding with more modern UI, greater flexibility and the ability to use enhancements made to the Jedi Engine for Outlaws in custom Dark Forces levels - such as slopes, stacked sectors, per-sector color maps and more.

## Copyright
Some parts of the source code are derived by reverse-engineering the original (DOS) Dark Forces executable (and soon Outlaws - Windows). As a result, this code has been derived and translated from code copyrighted by LucasArts. Areas of the code where this is the case will include a notice in their main header file, such as /TFE_JediRenderer/jediRenderer.h

I consider the reverse-engineering to be "Fair Use" - a means of supporting the games on other platforms and to improve support on existing platforms without claiming ownership of the games themselves or their IPs. You are still required to own a copy of the original games in order to play them using The Force Engine.

That said using code from sections so marked in a commercial project is risky without permission of the original copyright holders (LucasArts).

## Current State
The project is in a pre-release state, version 0.9. While it shares a legacy with DarkXL, it is a complete rewrite - rebuilt from the ground up with a much greater focus on accuracy. It is much more focused than the XL Engine, focused on being a Jedi Engine replacement/port only - thus full support for Dark Forces and Outlaws. Please check the [Roadmap](Roadmap.md) for more information on release timetable and planned feature-set. It is somewhat conservative - there is a decent chance that the release will be earlier.

A lot of the framework for the first release has been implemented but the majority of the focus is currently on the reverse engineering effort. What that means is that this version is incomplete, some elements are still inaccurate and others missing (such as enemy AI). 
The reason is simple - these elements will be filled in and/or corrected based on the reverse engineering work that is currently underway (and has been for some time). That means that no non-Windows build system is in place (and it is unlikely buildable on anything but Windows at the moment anyway). It also means that high resolution software rendering is slow but that will be corrected within the next few months.

The project will be properly released once Dark Forces support is complete (full INF, iMuse, all cutscenes, full AI, etc.) and Outlaws support is at "tech demo" status - mainly so that the new Jedi Engine features are in-place even if Outlaws gameplay code is not yet ready. Note that pull requests are very unlikely to be accepted until release because __*the code and even structure will change drastically*__ as the reverse-engineered code is ported from Dark Forces and Outlaws and refactored.

This section will be updated near release and contain a better description of the how & whys of this project.

A note on building - the project does build on Windows, however the setup is pretty fragile. Build it using Visual Studio 2017, only x64 builds (Debug/Release). The first time you run it will likely crash because it can't find the game assets (it is not ready for release yet, keep that in mind). Once that happens a settings.ini file will be created in Documents/TheForceEngine folder. Open that file in a text editor, set the game to "Dark Forces" and the sourcePath to the game data, like "C:/Program Files (x86)/Steam/steamapps/common/dark forces/Game/". Only a Steam installation has been tested but GOG and CD installations should be supported before release.

## Minimum Requirements
Note that older OS versions might work (such as Vista) but at least Windows 7 is highly recommended. Software rendering still relies on OpenGL to accelerate blitting. Generally a 2010 or later PC is recommended, though machines as old as 2009 or even 2006, depending on OS, may work but will likely not perform well unless running at a modest resolution. Note that some older GPUs may perform poorly with hardware rendering due to driver issues or poor support for required features (such as older integrated Intel chipsets) - in these cases software rendering should be used.
* Recommended 2010 era PC or newer
* Recommeded 2+ GB RAM
* Windows 7 (2009) / Linux (Version Info TBD)
* 32 bit or 64 bit
* CPU with SSE2 support (any desktop/laptop CPU released after 2004)
* OpenGL 2.1 for Software Rendering (2006 era GPU)
* OpenGL 3.3 for Hardware Rendering (2010 era GPU)

## Features
The project is focused on accuracy - by using reverse engineering techniques to reconstruct the original code and algorithms - the Force Engine is designed to be extremely accurate, a complete replacement for the original executables. The engine supports three feature templates to make it easier to tune the experience:
* **Classic** - `a recreation of the original software renderer, controls and gameplay - providing the original experience as close as possible while still properly supporting modern systems.`
* **Retro** - `close to the original experience while being enhanced with modern controls and high resolution rendering.`
* **Modern** - `modern enhancements such as proper perspective rendering, enhanced texture filtering, mipmapping, widescreen and more.`

From these general templates, settings can be fine tuned. One example is "Classic" settings with higher resolutions or mouse look. Or using the perspective renderer but sticking to 320x200. Both the "Classic" and Perspective renderers support pure software rendering and gpu based rendering, though some features - such as enhanced texture filtering - are only available using gpu rendering.

Additional control methods will be supported, such as gamepads with the ability to freely rebind keys and buttons at any time.

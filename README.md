# The Force Engine (TFE)
[Website](https://theforceengine.github.io/)

## About
The Force Engine is a project with the goal to reverse engineer and rebuild the Jedi Engine for modern systems and the games that used that engine - **Dark Forces** and **Outlaws**. The project will include modern, built-in tools, such as a level editor and will make it easy to play **Dark Forces** and **Outlaws** on modern systems as well as the many community **mods** designed to work with the original games.

Playing Dark Forces or Outlaws using the Force Engine will add ease of use and modern features such as higher resolutions and modern control schemes such as mouse-look. Using the built-in tools will allow for easier modding with more modern UI, greater flexibility and the ability to use enhancements made to the Jedi Engine for Outlaws in custom Dark Forces levels - such as slopes, stacked sectors, per-sector color maps and more.

Note that while Dark Forces support is almost complete (version 0.8), Outlaws is **not** playable yet - the focus so far has been on the framework, Dark Forces support, and JEDI reverse-engineering. However, Outlaws support is planned and will be complete in TFE version 2.0. See **Current State** below.

## Copyright
Some parts of the source code are derived by reverse-engineering the original (DOS) Dark Forces executable (and soon Outlaws - Windows). As a result, this code has been derived and translated from code copyrighted by LucasArts. Areas of the code where this is the case will include a notice in their main header file, such as /TFE_JediRenderer/jediRenderer.h

I consider the reverse-engineering to be "Fair Use" - a means of supporting the games on other platforms and to improve support on existing platforms without claiming ownership of the games themselves or their IPs. You are still required to own a copy of the original games in order to play them using The Force Engine.

That said using code from sections so marked in a commercial project is risky without permission of the original copyright holders (LucasArts/Disnay).

## Current State
The project is in a pre-release state, version 0.8. While it shares a legacy with DarkXL, it is a complete rewrite - rebuilt from the ground up with a much greater focus on accuracy. It is much more focused than the XL Engine, focused on being a Jedi Engine replacement/port only - thus full support for Dark Forces and later Outlaws. Please check the [Roadmap](Roadmap.md) for more information on release timetable and planned feature-set.

### Current Release
The current release is **version 0.8** and only supports Dark Forces. All weapons, AI, items, and other systems function, with the exception of IMuse. You can play through Dark Forces from beginning to end and play some Dark Forces mods. There are still bugs, the music isn't correct, some ambient level sounds are missing, and sound falloff is not accurate.

### Next Release
**Version 0.9** is expected to be released soon. This version will have full IMuse support, including music cues and dynamic changes in-game. It will also include missing ambient level sounds, and accurate sound falloff and playback.

### Progress Towards Next Release
IMuse music has recently been completed and work on the sound system is underway.

### Building
The project builds on Windows, though there is no proper CMake build system yet. Build it using Visual Studio 2017, only x64 builds (Debug/Release).

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

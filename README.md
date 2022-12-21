# The Force Engine (TFE)
* [Website](https://theforceengine.github.io/)
* [Release Downloads](https://theforceengine.github.io/downloads.html)
* [Forums](https://the-force-engine.freeforums.net/)
* [Discord](https://discord.gg/hpsJnY9)
* [Project Structure](ProjectStructure.md)

**A purchased copy of the original game is required and is not provided by The Force Engine.** The documentation - https://theforceengine.github.io/Documentation.html - has information on how to legally purchase Dark Forces.

## Current Features
* Full Dark Forces support, including mods. Outlaws support is coming in version 2.0.
* Mod Loader - simply place your mods in the Mods/ directory as zip files or directories.
* High Resolution and Widescreen support - when using 320x200 you get the original software renderer. TFE also includes a floating-point software renderer which supports widescreen, including ultrawide, and much higher resolutions.
* GPU Renderer with perspective correct pitch - play at much higher resolutions with improved performance.
* Extended Limits - TFE, by default, will support much higher limits than the original game which removes most of the HOM (Hall of Mirrors) issues in advanced mods.
* Full input binding, mouse sensitivity adjustment, and controller support. Note, however, that menus currently require the mouse.
* Optional Quality of Life improvements, such as full mouselook, aiming reticle, improved Boba Fett AI, autorun, and more.
* A new save system that works seamlessly with the exiting checkpoint and lives system. You can ignore it entirely, use it just as an exit save so you don't have to play long user levels in one sitting, or full save and load with quicksaves like Doom or Duke Nukem 3D.
* Optional and quality of life features, even mouselook, can be disabled if you want the original experience. Play in 320x200, turn the mouse mode (Input menu) to Menus only or horizontal, and enable the Classic (software) renderer - and it will look and play just like DOS, but with a higher framerate and without needing to adjust cycles in DosBox.

## About
The Force Engine is a project with the goal to reverse engineer and rebuild the Jedi Engine for modern systems and the games that used that engine - Dark Forces and Outlaws. The project will include modern, built-in tools, such as a level editor and will make it easy to play Dark Forces and Outlaws on modern systems as well as the many community mods designed to work with the original games.

Playing Dark Forces or, in the future, Outlaws using the Force Engine will add ease of use and modern features such as higher resolutions and modern control schemes such as mouse-look. Using the built-in tools, once they are available, will allow for easier modding with more modern UI, greater flexibility and the ability to use enhancements made to the Jedi Engine for Outlaws in custom Dark Forces levels - such as slopes, stacked sectors, per-sector color maps and more.

Dark Forces support is complete, but Outlaws is not playable yet - the focus so far has been on the framework, Dark Forces support, and JEDI reverse-engineering. However, Outlaws support is planned and will be complete in TFE version 2.0. See Current State below.

## Current State
Full support for Dark Forces has been completed. You can play through the entire game, with all AI, weapons, items, and functionality working as expected. While the project shares a legacy with DarkXL, it is a complete rewrite - rebuilt from the ground up with a much greater focus on accuracy. It is much more focused than the XL Engine, focused on being a Jedi Engine replacement/port only - thus full support for Dark Forces and later Outlaws. Please check the Roadmap for more information.

## Current Release
The current release only supports Dark Forces. All weapons, AI, items, and all other systems function, including IMuse. You can play through Dark Forces from beginning to end and even play Dark Forces mods. As with any project of this nature, there may be bugs and system specific issues. If you run into any bugs that cannot be reproduced in the DOS version, please post them on the forums or GitHub.

## Cross-platform Support
After version 1.0, one of the next big things to tackle is official Linux and Mac support. I expect proper support to become available early in 2023.

## Minimum Requirements
* Windows 7, 64 bit.
* OpenGL 3.3

Note that there are plans to lower the requirements for using the classic software renderer in the future. However, the minimum requirements for GPU Renderer support are here to stay. For now only OpenGL is supported, which might limit the use of some older Intel integrated GPUs that would otherwise be capable. There are near-term plans to add DirectX 10/11, Vulkan, and maybe Metal render backends which should enable more GPUs to run the engine efficiently.

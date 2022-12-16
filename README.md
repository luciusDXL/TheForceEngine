# The Force Engine (TFE)
* [Website](https://theforceengine.github.io/)
* [Release Downloads](https://theforceengine.github.io/downloads.html)
* [Forums](https://the-force-engine.freeforums.net/)
* [Discord](https://discord.gg/hpsJnY9)
* [Project Structure](ProjectStructure.md)

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

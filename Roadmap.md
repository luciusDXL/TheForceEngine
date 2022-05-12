# Roadmap

This roadmap has been completely rewritten based on current progress and changes that naturally occur during a long term project. A lot of this is from a recent blog post, but I will be updating it over time and want to have a single place for the information. A few of the entries have estimated release dates, which are subject to change. In-between releases are, obviously, expected to fall in-between the dates but are not estimated. Substantial work towards version 2.0 (Outlaws support) is expected to begin in early 2022 but it is too early to make any meaningful time estimates, though as described in that section, I do expect version 2.0 to be completed much faster than version 1.0.

- [x] [Core Game Loop Release](#core-game-loop-release)
- [ ] [Bug Fixes](#bug-fixes)
- [x] [UI and Mission Briefings](#ui-and-mission-briefings)
- [ ] [Sound](#sound) - _In Progress_
- [ ] [Version 1.0 Release](#version-10-release)
- [ ] [Voxels](#voxels)
- [ ] [Mac and Linux Support](#mac-and-linux-support)
- [ ] [Tools and Level Editor](#tools)

_These releases are too broad and will be clarified as the project gets closer_
* [Towards Version 2.0](#towards-version-20)

You can view the short-term progress here: [Progress Tracker](https://theforceengine.github.io/ProgressTracker.html)

# Core Game Loop Release
### Released November 18, 2021
The Core Game Loop release is still the next planned release. While it has taken longer than I originally planned for, as you can see above it is getting close to the finish line. This release will include saving and loading Dark Forces save game data (meaning your current saves will work), creating and deleting agents, proper saved progression through the game, proper visuals using the reverse-engineered classic renderer, pickups, items, all weapons working correctly, accurate player control, physics and collision detection, accurate level interactivity (INF), full enemy AI, palette effects from picking up objects and getting hurt, all items - such as the gasmask - working correctly, Vue animations, basic controller support, key and button rebinding, mouse and controller sensitivity adjustment.

The editors will be temporarily off-line, due to the complete refactor of the code base to properly use the reverse-engineered code. However, they will come back in a future release (see below).

Note, however, with all of this being made available at once there will inevitably be bugs and issues. I consider this to be an Alpha release - since it is not yet feature complete. Missing elements will include: cutscenes, mission briefings, some in-game UI (PDA), iMuse, and there will be some sound inaccuracies.

# Bug Fixes
There will be plenty of bugs to fix, so this is a placeholder here. This probably won’t be a big release by itself - instead it will be a bunch of smaller “point” releases. This will likely continue as the next releases are being worked on.

# UI and Mission Briefings
### Released December 5, 2021
The next major system will be the in-game UI, especially useful when looking at mission objectives and key codes using the PDA. Once the UI and mission briefings are in place, the game is technically fully playable.

# Sound
### Estimated Release: Mid May, 2022
This release will implement the correct sound falloff and 3D effects and finally fully implement the iMuse system. Note that the iMuse system has been completed and work on other sound elements is in progress.

# Version 1.0 Release
### Estimated Release: Late July 2022
With this release, TFE will be a complete replacement for DosBox. This release will also include the perspective-correct GPU renderer.

# Voxels
### Estimated Release: August 2022
Quite some time ago now, I implemented an experimental voxel renderer that integrated seamlessly with the Jedi classic renderer. However, there were some loose ends to deal with, such as not supporting the full VOX format and dealing with some palette issues. This release will integrate that code with the main branch and add support for replacing objects with their voxel counterpart.

# Tools
### Estimated Release: September 2022
The focus of this release is to get the tools working again with the reverse-engineered code. This will include basic functionality like importing and exporting assets, viewing them, etc. and the first release of the full level editor. It might still be missing some planned features, but as of this release it should be fully usable for building real levels and mods.

# Mac and Linux Support
### Estimated Release: November 2022
With this release, TFE will gain initial support on Mac and Linux. While the code more or less compiles on Linux (or is pretty close), there is more work to make sure it works well. And the Mac requires its own treatment and testing.

# Towards Version 2.0
Once version 1.0 is released, I plan on beginning the process of adding Outlaws support to TFE. I don’t yet have any real way of making any time estimates beyond this point, but I expect the reverse-engineering process to proceed at a much quicker rate for Outlaws for several reasons:

* I have become a lot faster at the process by going through the process of reverse-engineering Dark Forces.
* Outlaws was built from the Dark Forces code base, and shares the same engine. This means a lot of the structures will be the same or substaintally similar.
* I have a better sense of how to organize the process and reduce re-work and refactoring time.

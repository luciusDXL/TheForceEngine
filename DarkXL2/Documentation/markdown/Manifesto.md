# DarkXL 2: Design Manifesto
[Back](local://About)

DarkXL was never completed and was eventually replaced by the XL Engine with working going towards other games and projects. Unfortunately, real-life intervened, as it is apt to do, and progress fell off on the XL Engine. Fast forward to today (or the time of this writing anyway) - Daggerfall is in the good hands of the Daggerfall Unity project, Blood has been getting attention elsewhere and many other games I was interested in are getting their own remakes or ports. But still, to this day, Dark Forces is left to squander.

While I consider DarkXL a "partial success," it is flawed in some ways. It was originally written against DirectX 9 and focused on Windows and GPU based rendering. In hindsight, I spent too much time on additional features and effects while the core was not quite accurate or robust enough. What I originally wanted and still do is a source port quality engine for Dark Forces and this requires a slightly different focus than DarkXL.

To meet these ends I am starting a new project - DarkXL 2 (A New Hope) - with a renewed focus on Dark Forces with the goal to create a source port from which to work. To this end, this work will follow six core pillars.

## Accuracy
The overriding pillar is to accurately reproduce the game experience. This does not mean sticking to 320x200 (or 640x480 on Mac) or limiting controls - though those should be options - but accurate rendering, gameplay, music, and sound. DarkXL 2 will come pre-packaged with at least a few mods - so that I can ensure full accurate mod support.

## Open Source
The next pillar is to move to an Open Source model. Real-life can often get in the way and run a project off course. While the plan is to get to a first release before announcing the project - so by the time you are reading this it has already happened - this first release will include all of the source code available on Git Hub. This way people can extend it right away, for example adding support for their platform of choice. Also if I am no longer able or willing to work on the project for a while, it is there for others to work with. And finally, the first release will be a fully functional, software-rendered port of the game allowing for Doom style source ports to be developed without my help.

## Software Rendering
The third pillar is rendering - specifically, software rendering using the same algorithms as the original. This should reproduce the original rendering, even including artifacts and side effects. In this way, true side by side, apples to apple comparisons can be made and ensure accuracy. I do plan on supporting hardware rendering similar to the original DarkXL but that will come later, once the accurate release is done. In this way I can focus on accuracy and completeness rather than enhancements - though some basic improvements will likely come right away such as higher resolution support, modern OS support, etc.. Starting with pure software rendering feeds into the third pillar - cross-platform support from day 1. Later GPU rendering support will be added again, after the first release.

## Cross-Platform
DarkXL2 should be built with cross-platform support in mind from the beginning. There are multiple elements to this but the core is to stick with cross-platform libraries when possible. For example, the windowing system - rather than being raw Win32 as in the original DarkXL - will use a modern cross-platform windowing/input library (SDL or similar). The software rendering pillar will make this easier to some degree. That said complications will arise once the GPU renderer comes online, but that won’t happen until after the first release.

## INF and Scripting
Accurate playback of INF scripts will be a high priority and a major focus for the first release. Every level should function correctly in addition to various mods. Having Software Rendering as a core principle and doing apples to apple comparisons will help in this regard.

## iMuse
The iMuse implementation in DarkXL was always lacking. This is an important pillar to focus on once the other game elements are in place. Accurately reproducing the original experience is vital to the success of DarkXL 2.

[Back](local://About)
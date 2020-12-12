## Introduction
I believe that I have captured all of the individual contributors and dependencies. That said, if you think I am using your work and you are not listed below, or would rather be listed differently, please let me know so that it can be corrected. Any mistakes in this regard are not intentional and will be fixed as soon as possible.

## Individual Contributors
luciusDXL (me) - Principle Developer / Project Lead (really only developer for now)

Craig Yates - TFE Logo design. Minimalist title screen design.
Pawel "Dzierzan" Dzierzanowski - Adjustable Hud mod (built-in).
Bela Lugosi is cold (and dead) - Thorough testing and very cool test levels that are now used internally to provide regression testing.

## Libraries / Source Code
Most of the source code listed below is embedded within the project so that it can be modified to better fit the project and fix issues. However all such source code is available in the respository. Obviously some libraries, such as SDL2 and DevIL only include DLLs and includes; but original source code is available through the provided links.

Note that some libraries will be replaced by custom code or simpler libraries in the future (such as DevIL).

**Windowing/OpenGL**
SDL 2.0.x
[https://www.libsdl.org/](https://www.libsdl.org/)
License: ZLIB

GLEW
[https://github.com/nigels-com/glew](https://github.com/nigels-com/glew)
License: Modified BSD license

**UI**
imGUI
[https://github.com/ocornut/imgui](https://github.com/ocornut/imgui)
License: MIT

imGUI File Dialog, modified to fit The Force Engine use case.
[https://github.com/gallickgunner/ImGui-Addons](https://github.com/gallickgunner/ImGui-Addons)
License: MIT

imGUI Markdown
[https://github.com/juliettef/imgui_markdown](https://github.com/juliettef/imgui_markdown)
License: ZLIB

Portal File Dialogs
[https://github.com/samhocevar/portable-file-dialogs](https://github.com/samhocevar/portable-file-dialogs)
License: [WTFPL](https://github.com/samhocevar/portable-file-dialogs/blob/master/COPYING)

**Geometry**
fast-poly2tri - v1.0
Rewrite of the poly2tri library [https://github.com/jhasse/poly2tri](https://github.com/jhasse/poly2tri) by Unspongeful (@unspongeful).
License: BSD-3-Clause

Clipper
[http://www.angusj.com/delphi/clipper.php](http://www.angusj.com/delphi/clipper.php)
License: Boost Software License

**Image Loading**
DevIL
[http://openil.sourceforge.net/](http://openil.sourceforge.net/)
License: LGPL 2.1

**Audio**
RtAudio (Core Audio I/O)
[http://www.music.mcgill.ca/~gary/rtaudio/](http://www.music.mcgill.ca/~gary/rtaudio/)
License: Custom

RtMidi (Core Midi Hardware I/O)
[https://www.music.mcgill.ca/~gary/rtmidi/](https://www.music.mcgill.ca/~gary/rtmidi/)
License: Custom

**Scripting**
[https://www.angelcode.com/angelscript/](https://www.angelcode.com/angelscript/)
License: ZLIB


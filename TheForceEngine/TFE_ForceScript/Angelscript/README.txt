============================
Force Script
============================
Force Script relies on the Angelscript library to provide the core language.
This is embedded within the source tree, even for Linux, rather than relying
on the system library for several reasons:

* Scripts for *all* builds of TFE *need* to be using the same core library,
otherwise you may get different mod behaviors.
* It is possible that I make tweaks to Angelscript itself, which is permitted
by the license since the changes are part of the source code.
* To make it easier to support on Linux/Mac without adding yet another library.

Initially the scripting system will be optional on Linux, but eventually it
will be required.

NOTE: Not all "add ons" are included, this is intentional. I am making
opinionated choices as to what should be included as part of Force Script.

===================================================================
Angelscript can be found here, along with the original source code:
https://www.angelcode.com/angelscript/

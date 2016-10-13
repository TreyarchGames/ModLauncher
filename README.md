# ModLauncher
ModTools Launcher App

This is the Launcher App used by the Call of Duty: Black Ops III Mod Tools.

It's licensed under the Apache 2.0 License, a copy of which is included in the LICENSE file.

# Compiling

You'll need the Qt and Steam SDK installed. We use VS 2012 to compile but other versions may work, even though we don't provide a Qt Creator project it should still be possible to use it if you create a project.

Openning the project with a different version of VS may display a prompt to upgrade, this is ok and shouldn't cause any problems.

If you have the Qt VS Plugin installed you may get a message saying "There's no Qt Version assigned to this project for platform x64. Please use the 'change Qt version' feature and choose a valid Qt version for this platform", just select your Qt version from the Qt Project Settings Dialog. 

You'll probably need to change the compiler and linker search paths to where you have the Qt and Steam SDK installed. Note that the Steam SDK version you use must match the DLL in game\bin or you'll get errors when trying to run.

# Vortex Emulator
A test framework for the Vortex Engine

<img src="/screenshot.png" />

To pull the VortexEngine branch inside do:
```
git submodule init
git submodule update
```

Then open the Test Framework solution file in visual studio 2019+

It should build without any issues or dependencies

The test framework works by wrapping around the core VortexLib library
and utilizing the Vortex Engine apis to run a desktop based Vortex Engine
that rendered the output and accepts inputs through the button press.

There is a feature to simulate IR communications between two devices,
it will open a pipe from the first program to the second program allowing
for an IR transfer from one test framework to another.

This software exists mainly as a testing and debugging ground for the
Vortex Engine library, but this software can also be used to test the
Vortex Engine and see how the Vortex Firmware works before owning a device.

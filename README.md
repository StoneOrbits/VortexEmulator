# VortexTestingFramework
A test framework for the Vortex engine

![Screenshot](Screenshot.png?raw=true "Screenshot")

To pull the Vortex-Gloves branch inside do:
```
git submodule init
git submodule update
```

Then open the Test Framework solution file in visual studio 2019+

It should build without any issues or dependencies

The test framework works by replacing the core arduino headers and 
library functions with it's own functions while running inside a basic 
win32 application.

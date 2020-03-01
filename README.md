## AutoArt

This is an OpenGL implementation of AutoArt.

## Controls

Press r to generate a new piece of art, and f to toggle fullscreen.  
Press 3 to switch to 3D and 4 to switch to 4D.  
Press c to switch to Cartesian coordinates and p to switch to polar coordinates.  
Press m to switch to mod and s to switch to sigmoid.  
Press g to switch to RGB and h to switch to HSV.

### Building AutoArt

On Unix-y operating systems, just run `build.sh` (you will need `libsdl2-dev`)

On Windows, run `build.bat`, after extracting the [SDL development libraries](https://www.libsdl.org/download-2.0.php), and moving `include` to this directory, renaming it to `SDL2`. Also move `lib/SDL2.lib`, `lib/SDL2main.lib`, and `lib/SDL2.dll` to this directory.


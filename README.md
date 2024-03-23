### HexGPU

HexGPU is a game project with all game logic calculated entirely on the GPU side.

The world in this game is a 3D grid of hexagonal prisms of size roughly equal to 1 m.
This grid is updated with fixed frequency - water flows, grass grows, light propagates, sand falls, etc.

All world data (blocks, additional data, lighting data, etc.) is stored in GPU memory.
All game logic is implemented via set of compute shaders which operate with game data on the GPU side.
World generation performs by a shader too.

Player logic (movement) is also GPU-driven, C++ code provides only user inputs.
Thus player state is stored in GPU memory too.

World meshing is (of course) performed on the GPU.
World data is used directly by special compute shader which produces result triangles.
Finally these triangles are rendered as usually.


### How to build

You need a C++ compiler with C++17 standard support.
CMake 3.15 or newer required.

SDL2 library is used for platform-independent window creation and input.
You can specify path to SDL2 libraries via _SDL2_DIR_ cmake variable.

The project uses Vulkan as API for GPU computing and rendering.
So, Vulkan headers are necessary.
I recommend to use Vulkan SDK for that.
Vulkan 1.1 or newer is required.

glslangValidator is required in order to compile GLSL shaders.
You can specify path to it via _GLSLANGVALIDATOR_ cmake variable.

This project uses some thirdparty dependencies as git submodules.
Do not forget to init/update submodules before building!


### System requirements

Vulkan 1.1 support int the system is required.
Some Vulkan extension are also needed, like 8bit/16bit integers.
Generally the game should work on any modern GPU (AMD, nVidia, Intel).

Video memory requiremens are for now in orders of hundreds of megabytes.
More memory for bigger active world size is required.


### Basic usage

For now it's possible to run the game, fly, build blocks, destroy them.
Game world is pseudo-randomly generated and is somewhat unlimited.
The number of block types is for now very poor - around 10.

It's possible to specify time of day via debug menu.
But for now this affects only the sky and world lighting, but not the game logic itself.

World state is automatically saved on disk, default directory is named _world_ (make sure it exists).


### Controls

For now all controls are fixed:

* W, A, S, D - fly
* Space - fly up
* C - fly down
* Shift - sprint
* ↑, ↓, ←, → - rotate camera
* Mouse motion - rotate camera
* Mouse left button - destroy
* Mouse right button - build
* E - toggle block selection menu
* ~ - toggle debug menus
* ESC - exit


### Configuration

HexGPU uses file _HexGPU.cfg_ for settings.

Some configuration params:

* "r_window_width", "r_window_height" - game window size
* "r_fullscreeen" - 0 to run windowed or 1 to run in fullscreen mode
* "r_vsync" - 0 to disable vsync, 1 to enable
* "r_device_id" - you may change Vulkan device via this setting. This may be helpful for systems with more than 1 GPU.
* "g_world_size_x", "g_world_size_y" - world size (in chunks). Increase this to have bigger view distance, but this may affect performance.
* "g_world_seed" - set to some number to change world generator seed
* "g_world_dir" - change it to directory where world data should be saved
* "in_mouse_speed" - mouse sensitivity
* "in_invert_mouse_y" - 0 to normal mouse mode, 1 to invert mouse y axis

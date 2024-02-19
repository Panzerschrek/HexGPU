Here is the list of futures that needs to be implemented.
The order represents importance and/or dependency.
* Sun lighting
* GPU-side allocator for chunk vertices - in order to allow to some chunks have huge meshes but use not so huge overall vertex buffer
* Variable number of chunks in the world
* World movement
* Chunks save/load
* Textures with scale based on block type/side
* Textures without cross-block repetition (with zeroing at each block)
* Flowing water
* Growing grass
* Improved textures loading - use smaller staging buffer and free it at the end of the loading process
* Biome-based world generation
* Trees generation
* Fire (that burns some blocks)
* Player collision against the world
* More block types
* More light source blocks
* Settings
* Anisotropy textures filtering
* Run SPIR-V optimizer for compiled shaders
* Configurable textures config
* Plates (half-blocks)
* Vertically-splitted half-blocks
* Sky drawing (sky, sun, clouds)
* Rain/snow
* Time of day change
* GUI

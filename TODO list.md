Here is the list of futures that needs to be implemented.
The order represents importance and/or dependency.
* Player position-based world movement
* Chunks save/load
* Textures with scale based on block type/side
* Textures without cross-block repetition (with zeroing at each block)
* Flowing water
* Improved textures loading - use smaller staging buffer and free it at the end of the loading process
* Biome-based world generation
* Trees generation
* Update light only after blocks update - in order to avoid black spots in places of removed blocks
* Growing grass improvements - grow randomly
* Grass logic improvements - disappers slower, become yellow, require water or sunlight to grow, etc.
* Frustum-culling for chunks
* Fire (that burns some blocks)
* Player collision against the world
* More block types
* Smooth lighting - average block light values at each vertex
* More light source blocks
* Use device-local memory almost everywhere
* Anisotropy textures filtering
* Run SPIR-V optimizer for compiled shaders
* Configurable textures config
* Chunk vertices allocator - make it faster (remove linear complexity)
* Plates (half-blocks)
* Vertically-splitted half-blocks
* Sky drawing (sky, sun, moon, clouds)
* Rain/snow
* Time of day change
* GUI
* Sparse chunk geometry update improvements - update area around player frequently, use direction of view, etc.

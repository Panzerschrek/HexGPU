Here is the list of futures that needs to be implemented.
The order represents importance and/or dependency.
* Chunks save/load
* Improved textures loading - use smaller staging buffer and free it at the end of the loading process
* Biome-based world generation
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
* Water blocks side polygons
* Twostage water rendering - back faces than front faces, in order to achieve somewhat good transparency ordering
* Underwater fog
* Evaporate water from blocks with low level and no input/output flow (randomly)
* Increase water level in non-full water blocks under the sky when it's raining
* Configurable textures config
* Trees generation with variable density
* Procedural tree models
* Foliage blocks logic - disapper if there is no tree block nearby
* Chunk vertices allocator - make it faster (remove linear complexity)
* Plates (half-blocks)
* Vertically-splitted half-blocks
* Day-night cycle affecting sky brightness and sun direction
* Day-night cycle affecting sky light brightness
* Clouds
* Stars (at night)
* Rain/snow
* GUI
* Pipe blocks for fast directional water transfer
* Sparse chunk geometry update improvements - update area around player frequently, use direction of view, etc.

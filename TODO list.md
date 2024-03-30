Here is the list of futures that needs to be implemented.
The order represents importance and/or dependency.
* Biome-based world generation
* Update light only after blocks update - in order to avoid black spots in places of removed blocks
* Grass logic improvements - disappers slower, become yellow, require water or sunlight to grow, etc.
* Save and load player state
* More block types
* Smooth lighting - average block light values at each vertex
* Run SPIR-V optimizer for compiled shaders
* Use background thread for chunk data compression/decompression
* Water blocks side polygons
* Twostage water rendering - back faces than front faces, in order to achieve somewhat good transparency ordering
* When uderwater reduce fog distance and use other fog color
* Evaporate water from blocks with low level and no input/output flow (randomly)
* Increase water level in non-full water blocks under the sky when it's raining
* Player physics - add gravity
* Player physics - perform proper cylinder to hex-prism collision
* Player physics - fix some edge cases with multiple block collision
* Player physics - limit velocity vector on collisions
* Trees generation with variable density
* Procedural tree models
* Burn wooden blocks first into charcoal blocks
* Chunk vertices allocator - make it faster (remove linear complexity)
* Plates (half-blocks)
* Vertically-splitted half-blocks
* Day-night cycle - change day time each tick
* Stars (at night)
* Rain/snow
* Extinguish fire during rain
* Tune burning probability for different block types
* Pipe blocks for fast directional water transfer
* Sparse chunk geometry update improvements - update area around player frequently, use direction of view, etc.

// Blocks types list.
// Usage - define BLOCK_PROCESS_FUNC, include this file.

#ifndef BLOCK_PROCESS_FUNC
#error "Expected BLOCK_PROCESS_FUNC macro"
#endif

// If this changed, corresponding GLSL code must be changed too!
BLOCK_PROCESS_FUNC(Air)
BLOCK_PROCESS_FUNC(SphericalBlock)
BLOCK_PROCESS_FUNC(Stone)
BLOCK_PROCESS_FUNC(Soil)
BLOCK_PROCESS_FUNC(Wood)
BLOCK_PROCESS_FUNC(Grass)
BLOCK_PROCESS_FUNC(Brick)
BLOCK_PROCESS_FUNC(Foliage)
BLOCK_PROCESS_FUNC(FireStone)
BLOCK_PROCESS_FUNC(Water)
BLOCK_PROCESS_FUNC(Sand)

// Put new block types at the end


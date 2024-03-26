// If this file changed, corresponding C++ file must be changed too!
#ifndef CONSTANTS_GLSL_HEADER
#define CONSTANTS_GLSL_HEADER

const float c_pi= 3.1415926535;

const float c_space_scale_x= sqrt(3.0) / 2.0;

const int c_chunk_width_log2= 4;
const int c_chunk_width= 1 << c_chunk_width_log2;

const int c_chunk_height_log2= 7;
const int c_chunk_height= 1 << c_chunk_height_log2;

// In blocks.
const int c_chunk_volume= c_chunk_width * c_chunk_width * c_chunk_height;

const int c_fire_light_mask= 0x0F;
const int c_sky_light_mask= 0xF0;
const int c_sky_light_shift= 4;

const int c_max_sky_light= 15;

const int c_max_water_level= 255;

const int c_max_foliage_factor= 6;

const int c_initial_fire_power= 1;
const int c_min_fire_power_for_fire_to_spread= 32;
const int c_min_fire_power_for_blocks_burning= 64; // TODO - make this different for different block types.

#endif // CONSTANTS_GLSL_HEADER

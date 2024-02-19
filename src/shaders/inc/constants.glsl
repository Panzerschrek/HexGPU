// If this file changed, corresponding C++ file must be changed too!
#ifndef CONSTANTS_GLSL_HEADER
#define CONSTANTS_GLSL_HEADER

const float c_space_scale_x= sqrt(3.0) / 2.0;

const int c_chunk_width_log2= 4;
const int c_chunk_width= 1 << c_chunk_width_log2;

const int c_chunk_height_log2= 7;
const int c_chunk_height= 1 << c_chunk_height_log2;

// In blocks.
const int c_chunk_volume= c_chunk_width * c_chunk_width * c_chunk_height;

// TODO - make non-constant.
const int c_chunk_matrix_size[2]= int[2](4, 4);

const int c_max_global_x= (c_chunk_matrix_size[0] << c_chunk_width_log2) - 1;
const int c_max_global_y= (c_chunk_matrix_size[1] << c_chunk_width_log2) - 1;

const int c_fire_light_mask= 0x0F;
const int c_sky_light_max= 0xF0;
const int c_sky_light_shift= 4;

#endif // CONSTANTS_GLSL_HEADER

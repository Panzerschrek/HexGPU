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

#endif // CONSTANTS_GLSL_HEADER

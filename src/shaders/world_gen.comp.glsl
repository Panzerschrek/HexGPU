#version 450

#extension GL_GOOGLE_include_directive : require
#include "inc/constants.glsl"

#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(push_constant) uniform uniforms_block
{
	int chunk_position[2];
};

void main()
{
	int chunk_index= chunk_position[0] + chunk_position[1] * c_chunk_matrix_size[0];
	int chunk_data_offset= chunk_index * c_chunk_volume;

	int local_x= int(gl_GlobalInvocationID.x);
	int local_y= int(gl_GlobalInvocationID.y);
	int block_global_x= (chunk_position[0] << c_chunk_width_log2) + local_x;
	int block_global_y= (chunk_position[1] << c_chunk_width_log2) + local_y;

	int ground_z= int(8.0 + 3.0 * sin(float(block_global_x) * 0.2) + 2.0 * sin(float(block_global_y) * 0.3));
	int column_offset= chunk_data_offset + ChunkBlockAddress(local_x, local_y, 0);
	for( int z= 0; z < c_chunk_height; ++z )
	{
		uint8_t block_value= z > ground_z ? uint8_t(0) : uint8_t(1);
		chunks_data[column_offset + z]= block_value;
	}
}

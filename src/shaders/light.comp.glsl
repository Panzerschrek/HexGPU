#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"

layout(push_constant) uniform uniforms_block
{
	int chunk_position[2];
};

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(binding= 1, std430) buffer chunk_input_light_buffer
{
	uint8_t input_light[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(binding= 2, std430) buffer chunk_output_light_buffer
{
	uint8_t output_light[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

void main()
{
	int chunk_index= chunk_position[0] + chunk_position[1] * c_chunk_matrix_size[0];
	int chunk_data_offset= chunk_index * c_chunk_volume;

	ivec3 invocation= ivec3(gl_GlobalInvocationID);

	int block_global_x= (chunk_position[0] << c_chunk_width_log2) + int(invocation.x);
	int block_global_y= (chunk_position[1] << c_chunk_width_log2) + int(invocation.y);

	output_light[chunk_data_offset + ChunkBlockAddress(invocation.x, invocation.y, invocation.z)]= uint8_t((invocation.z * 3) & 15);
}

#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/constants.glsl"
#include "inc/block_type.glsl"
#include "inc/noise.glsl"

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(push_constant) uniform uniforms_block
{
	bool build_triggered;
	bool destroy_triggered;
};

void main()
{
	chunks_data[ChunkBlockAddress(1, 3, 34)]= c_block_type_brick;
}

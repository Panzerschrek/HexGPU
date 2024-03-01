#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 8, local_size_y = 8, local_size_z= 1) in;

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
	ivec2 chunk_position;
	ivec2 chunk_global_position;
};

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[];
};

layout(binding= 1, std430) buffer light_data_buffer
{
	uint8_t light_data[];
};

void main()
{
	int chunk_index= chunk_position.x + chunk_position.y * world_size_chunks.x;
	int chunk_data_offset= chunk_index * c_chunk_volume;

	int local_x= int(gl_GlobalInvocationID.x);
	int local_y= int(gl_GlobalInvocationID.y);
	int global_x= (chunk_position.x << c_chunk_width_log2) + local_x;
	int global_y= (chunk_position.y << c_chunk_width_log2) + local_y;

	int column_offset= chunk_data_offset + ChunkBlockAddress(ivec3(local_x, local_y, 0));

	int z= c_chunk_height - 1;
	// Propagate maximum sky light value down, until non-air block is reached.
	while(z >= 0)
	{
		int offset= column_offset + z;
		if(chunks_data[offset] != c_block_type_air)
			break;

		light_data[offset]= uint8_t(15 << c_sky_light_shift);
		--z;
	}

	// Fill remaining column blocks with zeros.
	while(z >= 0)
	{
		light_data[column_offset + z]= uint8_t(0);
		--z;
	}
}

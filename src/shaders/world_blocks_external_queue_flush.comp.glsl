#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/world_blocks_external_update_queue.glsl"

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
};

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[];
};


layout(binding= 1, std430) buffer world_blocks_external_update_queue_buffer
{
	WorldBlocksExternalUpdateQueue world_blocks_external_update_queue;
};

void main()
{
	for(uint i= 0; i < min(world_blocks_external_update_queue.num_updates, c_max_world_blocks_external_updates); ++i)
	{
		WorldBlockExternalUpdate update= world_blocks_external_update_queue.updates[i];
		// TODO - check in bounds.

		int address= GetBlockFullAddress(update.position.xyz, world_size_chunks);
		chunks_data[address]= update.new_block_type;
	}

	world_blocks_external_update_queue.num_updates= 0;
}

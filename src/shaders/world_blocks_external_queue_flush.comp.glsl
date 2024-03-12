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
	ivec2 world_offset_chunks;
};

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[];
};

layout(binding= 1, std430) buffer world_blocks_external_update_queue_buffer
{
	WorldBlocksExternalUpdateQueue world_blocks_external_update_queue;
};

layout(binding= 2, std430) buffer chunks_auxiliar_data_buffer
{
	uint8_t chunks_auxiliar_data[];
};

void main()
{
	for(uint i= 0; i < min(world_blocks_external_update_queue.num_updates, c_max_world_blocks_external_updates); ++i)
	{
		WorldBlockExternalUpdate update= world_blocks_external_update_queue.updates[i];
		ivec3 position_in_world= update.position.xyz - ivec3(world_offset_chunks << c_chunk_width_log2, 0);
		if(IsInWorldBorders(position_in_world, world_size_chunks))
		{
			int address= GetBlockFullAddress(position_in_world, world_size_chunks);
			uint8_t current_block_value= chunks_data[address];
			if(current_block_value == update.old_block_type)
			{
				// Allow changing block if it isn't changed by someone else.
				chunks_data[address]= update.new_block_type;

				if(update.new_block_type == c_block_type_water)
					chunks_auxiliar_data[address]= uint8_t(c_max_water_level);
				else
					chunks_auxiliar_data[address]= uint8_t(0);
			}
			else
			{
				// TODO - handle this situation somehow.
			}
		}
	}

	// Reset the queue.
	world_blocks_external_update_queue.num_updates= 0;
}

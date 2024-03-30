#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"
#include "inc/player_state.glsl"
#include "inc/player_world_window.glsl"

// maxComputeWorkGroupInvocations is at least 128.
// If this is changed, corresponding C++ code must be changed too!
layout(local_size_x= 4, local_size_y = 4, local_size_z= 8) in;

layout(push_constant) uniform uniforms_block
{
	ivec2 world_size_chunks;
	ivec2 world_offset_chunks;
};

layout(binding= 0, std430) readonly buffer chunks_data_buffer
{
	uint8_t chunks_data[];
};

layout(binding= 1, std430) buffer player_world_window_buffer
{
	PlayerWorldWindow player_world_window;
};

layout(binding= 2, std430) readonly buffer player_state_buffer
{
	PlayerState player_state;
};

layout(binding= 3, std430) readonly buffer light_data_biffer
{
	uint8_t light_data[];
};

void main()
{
	ivec4 player_world_window_offset= player_state.next_player_world_window_offset;

	ivec3 invocation= ivec3(gl_GlobalInvocationID);

	if(invocation == ivec3(0, 0, 0))
	{
		player_world_window.offset= player_world_window_offset;

		ivec3 player_block_global_coord= ivec3(GetHexogonCoord(player_state.pos.xy), int(floor(player_state.pos.z)) + 1);
		ivec3 player_block_coord= player_block_global_coord - ivec3(world_offset_chunks << c_chunk_width_log2, 0);
		if(IsInWorldBorders(player_block_coord, world_size_chunks))
			player_world_window.player_block_light= light_data[GetBlockFullAddress(player_block_coord, world_size_chunks)];
	}

	ivec3 block_coord= player_world_window_offset.xyz - ivec3(world_offset_chunks << c_chunk_width_log2, 0) + invocation;

	uint8_t block_value= c_block_type_air;
	if(IsInWorldBorders(block_coord, world_size_chunks))
	{
		block_value= chunks_data[GetBlockFullAddress(block_coord, world_size_chunks)];
	}

	player_world_window.window_data[GetAddressOfBlockInPlayerWorldWindow(invocation)]= block_value;
}

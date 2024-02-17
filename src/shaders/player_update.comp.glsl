#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/block_type.glsl"
#include "inc/hex_funcs.glsl"

layout(binding= 0, std430) buffer chunks_data_buffer
{
	uint8_t chunks_data[c_chunk_volume * c_chunk_matrix_size[0] * c_chunk_matrix_size[1]];
};

layout(push_constant) uniform uniforms_block
{
	vec4 player_pos;
	// Use "uint8_t", because "bool" in GLSL has size different from C++.
	uint8_t build_triggered;
	uint8_t destroy_triggered;
	uint8_t reserved[2];
};

void main()
{
	// For now build and destroy directly at player position.
	ivec2 player_hex_grid_pos= GetHexogonCoord(player_pos.xy);
	int player_z= int(floor(player_pos.z));
	if( player_hex_grid_pos.x >= 0 && player_hex_grid_pos.x < c_chunk_width * c_chunk_matrix_size[0] &&
		player_hex_grid_pos.y >= 0 && player_hex_grid_pos.y < c_chunk_width * c_chunk_matrix_size[1] &&
		player_z >= 0 && player_z < c_chunk_height)
	{
		int chunk_x= player_hex_grid_pos.x >> c_chunk_width_log2;
		int chunk_y= player_hex_grid_pos.y >> c_chunk_width_log2;
		int local_x= player_hex_grid_pos.x & (c_chunk_width - 1);
		int local_y= player_hex_grid_pos.y & (c_chunk_width - 1);

		int chunk_index= chunk_x + chunk_y * c_chunk_matrix_size[0];
		int chunk_data_offset= chunk_index * c_chunk_volume;

		int block_full_address= chunk_data_offset + ChunkBlockAddress(local_x, local_y, player_z);

		if(build_triggered != uint8_t(0))
		{
			chunks_data[block_full_address]= c_block_type_brick;
		}
		if(destroy_triggered != uint8_t(0))
		{
			chunks_data[block_full_address]= c_block_type_air;
		}
	}
}

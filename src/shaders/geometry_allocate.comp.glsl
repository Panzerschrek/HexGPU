#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

#include "inc/allocator.glsl"
#include "inc/chunk_draw_info.glsl"

layout(push_constant) uniform uniforms_block
{
	uint num_chunks_to_allocate;
	uint16_t chunks_to_allocate_list[62];
};

layout(binding= 0, std430) buffer chunk_draw_info_buffer
{
	ChunkDrawInfo chunk_draw_info[];
};

// Max number of quads is 65536 / 4 (uint16_t index limit).
// Min number should be 32 times less because of allocator limitations.
const uint c_allocation_unut_size_quads= 512;

void main()
{
	// Perform updates for all chunks in single thread.
	for(uint i= 0; i < num_chunks_to_allocate; ++i)
	{
		uint chunk_index= uint(chunks_to_allocate_list[i]);
		chunk_draw_info[chunk_index].num_quads= 0;

		// Calculate rounded up number of memory units.
		uint num_memory_units_required=
			(chunk_draw_info[chunk_index].new_num_quads + (c_allocation_unut_size_quads - 1)) / c_allocation_unut_size_quads;

		if(num_memory_units_required != chunk_draw_info[chunk_index].num_memory_units)
		{
			// Perform allocation in both cases if have not enough memory and if have too much memory.
			// Doing so allows us to free excessive memory if it is no longer needed.
			uint new_unit= AllocatorAllocate(num_memory_units_required);
			if(new_unit != c_allocator_fail_result)
			{
				// Free previous buffer.
				AllocatorFree(chunk_draw_info[chunk_index].first_memory_unit, chunk_draw_info[chunk_index].num_memory_units);

				// Save allocated memory.
				chunk_draw_info[chunk_index].first_memory_unit= new_unit;
				chunk_draw_info[chunk_index].num_memory_units= num_memory_units_required;
			}
			else
			{
				// Do nothing here.
				// TODO - avoid rebuilding chunk geometry if allocation fails.
			}
		}

		chunk_draw_info[chunk_index].first_quad=
			chunk_draw_info[chunk_index].first_memory_unit * c_allocation_unut_size_quads;
	}
}

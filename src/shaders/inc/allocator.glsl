// This allocator manages some memory and allows to allocate ranges from 1 to 32 units of size.
// It is dumb, has a lot of limitation but relatively simpe which allows to avoid possible bugs.
// it's important to keep overall number of units pretty small - a couple of thousands, because allocation complexity is linear.

const int c_allocator_buffer_binding= 3333;

layout(binding= c_allocator_buffer_binding, std430) buffer allocator_buffer
{
	uint allocator_total_number_of_units;
	uint allocator_used_units_bits[];
};

const uint c_allocator_fail_result= 0xFFFFFFFF;

// Allocates given number of units.
// Size should be no more than 32!
// Returns index of unit.
// Returns "c_allocator_fail_result" in case of fail.
// Complexity is linear.
uint AllocatorAllocate(uint size_units)
{
	if(size_units == 0)
		return 0;

	// Each allocation happens in one of 32-bit blocks.
	// If allocation is less than 32 units, free units may be still reused.

	// Contains number ones equal to size.
	uint mask= (1 << size_units) - 1;

	uint num_blocks= allocator_total_number_of_units >> 5;
	for(uint i= 0; i < num_blocks; ++i)
	{
		uint block_bits= allocator_used_units_bits[i];
		if(bitCount(block_bits) < size_units)
			continue; // Amount of free units in this 32bit block is less than required.

		// Perform search of passing range in the block.
		// We require continuous span on "size_units" bits.
		uint num_shifts= 32 - size_units;
		for(uint shift= 0; shift < num_shifts; ++shift)
		{
			uint mask_shifted= mask << shift;
			if((block_bits & mask_shifted) == 0)
			{
				// This range of bits is free. Allocate it.
				allocator_used_units_bits[i]= block_bits | mask_shifted;
				return (i << 5) + shift;
			}
		}
	}

	// Found nothing.
	return c_allocator_fail_result;
}

// Free specified number of units from given start.
void AllocatorFree(uint start_unit, uint size_units)
{
	if(size_units == 0)
		return;

	// Contains number ones equal to size.
	uint mask= (1 << size_units) - 1;

	uint block= start_unit >> 5;
	uint block_bit= start_unit & 31;

	// Zero allocated units.
	allocator_used_units_bits[block] &= ~(mask << block_bit);
}

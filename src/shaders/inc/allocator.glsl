// This allocator manages some memory and allows to allocate ranges from 1 to 32 units in size.
// It's dumb, has a lot of limitations but relatively simpe, which allows to avoid possible bugs.
// it's important to keep overall number of units pretty small - a couple of thousands, because allocation complexity is linear.
// It's also good to avoid unnecessary allocations, but still free unused memory, because fragmentation may be a problem.

const int c_allocator_buffer_binding= 3333;

layout(binding= c_allocator_buffer_binding, std430) buffer allocator_buffer
{
	uint allocator_total_number_of_units;
	uint allocator_used_units_bits[]; // Size is "allocator_total_number_of_units" / 32 (rounded up).
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
	if(size_units > 32)
		return c_allocator_fail_result;

	// Each allocation happens in one of 32-bit blocks.
	// If allocation is less than 32 units, free units withing a block may be used for another allocation.

	// Contains number ones equal to size.
	uint mask= (1 << (size_units & 31)) - 1;

	uint num_blocks= allocator_total_number_of_units >> 5;
	for(uint i= 0; i < num_blocks; ++i)
	{
		// use bitCount to fast skip blocks with overall number of free units less than needed.
		uint block_bits= allocator_used_units_bits[i];
		uint number_of_used_units_in_block= bitCount(block_bits);
		uint number_of_free_units_in_block= 32 - number_of_used_units_in_block;
		if(number_of_free_units_in_block < size_units)
			continue; // Amount of free units in this 32bit block is less than required.

		// Perform search of passing range in the block.
		// We require continuous span of "size_units" size.
		uint num_shifts= 33 - size_units;
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

	// Mark given units of this block as free.
	allocator_used_units_bits[block] &= ~(mask << block_bit);
}

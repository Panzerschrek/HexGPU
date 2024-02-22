/*
  This file contains a simple GPU-side allocator, which indirectly manages some large buffer.
  The allocator itself has its header and an array of variable-sized control structures.
  The allocator is single-threaded only.
  Allocation is performed in abstract units, an allocator user should convert these units into relevant offsets (quads, vertices, indices, etc.).
*/

// The main allocator structure.
const uint c_null_memory_region= 0;

// If these structs are changed, C++ structs must be changed too!

struct Allocator
{
	uint total_memory_size_units;
	uint max_regions;
	uint first_region_index; // Head of sequential regions list.
	uint first_free_region_index; // Head of free regions list.
	uint first_available_region_index; // Head of available regions list.
};

// Allocator result structure.
struct AllocationInfo
{
	uint region_index; // "c_null_memory_region" if allocation failed.
	uint offset_units;
	uint size_units;
};

// Linked list node structure.
struct AllocatorLinkedListNode
{
	// Index "c_null_memory_region" means no prev/next node.
	uint prev_index;
	uint next_index;
};

// Memory is managed in regions.
struct AllocatorMemoryRegion
{
	// Represents linked list of regions.
	AllocatorLinkedListNode regions_sequence_list_node;
	AllocatorLinkedListNode free_regions_list_node;
	AllocatorLinkedListNode available_regions_list_node;
	uint offset_units;
	uint size_units;
	bool is_used;
};

// Data passed to this shader.

const int c_allocator_buffer_binding= 3333;

layout(binding= c_allocator_buffer_binding, std430) buffer allocator_buffer
{
	Allocator allocator;
	// Number of regions is variable-sized.
	AllocatorMemoryRegion allocator_memory_regions[];
};

// Allocator methods.

AllocationInfo AllocatorAllocate(uint size_units)
{
	AllocationInfo result;
	result.region_index= c_null_memory_region;
	result.size_units= 0;

	if(allocator.first_free_region_index == c_null_memory_region)
		return result; // Can't allocate - no free blocks left.

	// Find first region with required size in the list.
	// TODO - find best fit instead.
	// TODO - remove linear complexity.
	// Use some faster way, like having separate free lists for each power of two size.
	uint region_index= allocator.first_free_region_index;
	while(true)
	{
		if(allocator_memory_regions[region_index].size_units >= size_units)
			break; // Found suitable size block.
		region_index= allocator_memory_regions[region_index].free_regions_list_node.next_index;
		if(region_index == c_null_memory_region)
			return result; // Can't allocate - can't find region with enough size.
	}

	// Use this region for result.
	result.region_index= region_index;
	result.offset_units= allocator_memory_regions[region_index].offset_units;
	result.size_units= allocator_memory_regions[region_index].size_units;

	// Mark this region as non-free.
	allocator_memory_regions[region_index].is_used= true;

	// Remove this region form free regions linked list.
	{
		uint prev_index= allocator_memory_regions[region_index].free_regions_list_node.prev_index;
		uint next_index= allocator_memory_regions[region_index].free_regions_list_node.next_index;
		if(next_index != c_null_memory_region)
		{
			allocator_memory_regions[next_index].free_regions_list_node.prev_index= prev_index;
		}
		if(prev_index != c_null_memory_region)
		{
			allocator_memory_regions[prev_index].free_regions_list_node.next_index= next_index;
		}
		else
		{
			// prev is null - this should be the list head.
			allocator.first_free_region_index= next_index;
		}
		allocator_memory_regions[region_index].free_regions_list_node.prev_index= c_null_memory_region;
		allocator_memory_regions[region_index].free_regions_list_node.prev_index= c_null_memory_region;
	}

	// TODO - cut tail into another region.

	return result;
}

void AllocatorFree(in AllocationInfo allocation_info)
{
	// Just make sure region is correct.
	if(allocation_info.region_index == c_null_memory_region)
		return;

	uint region_index= allocation_info.region_index;

	// Mark this region as free.
	allocator_memory_regions[region_index].is_used= false;

	// Add this region to the linked list of free regions.
	allocator_memory_regions[region_index].regions_sequence_list_node.next_index= allocator.first_free_region_index;
	allocator.first_free_region_index= region_index;

	// TODO - combine this region with adjacent free regions to reduce fragmentation.
}

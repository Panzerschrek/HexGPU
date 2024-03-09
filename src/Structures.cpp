#include "Structures.hpp"
#include "Assert.hpp"

namespace HexGPU
{

namespace
{

StructureDescription GenLargeTree(std::vector<BlockType>& out_data)
{
	StructureDescription structure;
	structure.size[0]= 6;
	structure.size[1]= 6;
	structure.size[2]= 9;
	structure.center[0]= 2;
	structure.center[1]= 2;
	structure.center[2]= 0;

	const auto volume= structure.size[0] * structure.size[1] * structure.size[2];

	structure.data_offset= uint32_t(out_data.size());
	out_data.resize(out_data.size() + volume, BlockType::Air);

	const auto set_block=
		[&](const uint32_t x, const uint32_t y, const uint32_t z, const BlockType block_type)
		{
			HEX_ASSERT(x < structure.size[0]);
			HEX_ASSERT(y < structure.size[1]);
			HEX_ASSERT(z < structure.size[2]);
			const uint32_t offset= z + y * structure.size[2] + x * (structure.size[2] * structure.size[1]);
			out_data[structure.data_offset + offset]= block_type;
		};

	const uint32_t x= 2;
	const uint32_t y= 2;

	for(uint32_t z= 0; z < 7; ++z)
	{
		set_block(x, y, z, BlockType::Wood);
		set_block(x + 1, y + ((x + 1) & 1), z, BlockType::Wood);
		set_block(x + 1, y - (x & 1), z, BlockType::Wood);
	}

	for(uint32_t z= 7; z < 9; ++z)
	{
		set_block(x, y, z, BlockType::Foliage);
		set_block(x + 1, y + ((x + 1) & 1), z, BlockType::Foliage);
		set_block(x + 1, y - (x & 1), z, BlockType::Foliage);
	}

	for(uint32_t z= 2; z < 8; ++z)
	{
		set_block(x, y + 1, z, BlockType::Foliage);
		set_block(x, y - 1, z, BlockType::Foliage);
		set_block(x + 2, y, z, BlockType::Foliage);
		set_block(x + 2, y + 1, z, BlockType::Foliage);
		set_block(x + 2, y - 1, z, BlockType::Foliage);
		set_block(x - 1, y - (x & 1), z, BlockType::Foliage);
		set_block(x - 1, y + ((x + 1) & 1), z, BlockType::Foliage);
		set_block(x + 1, y + 1 + ((x + 1) & 1), z, BlockType::Foliage);
		set_block(x + 1, y - 1 - (x & 1), z, BlockType::Foliage);
	}
	for(uint32_t z= 3; z < 7; ++z)
	{
		set_block(x, y + 2, z, BlockType::Foliage);
		set_block(x, y - 2, z, BlockType::Foliage);

		set_block(x - 1, y - 1 - (x & 1), z, BlockType::Foliage);
		set_block(x - 1, y + 1 + ((x + 1) & 1), z, BlockType::Foliage);

		set_block(x + 3, y + ((x + 1) & 1), z, BlockType::Foliage);
		set_block(x + 3, y - (x & 1), z, BlockType::Foliage);
	}

	for(uint32_t z= 4; z < 6; ++z)
	{
		set_block(x-2, y, z, BlockType::Foliage);
		set_block(x-2, y - 1, z, BlockType::Foliage);
		set_block(x-2, y + 1, z, BlockType::Foliage);

		set_block(x + 1, y + 2 + ((x + 1) & 1), z, BlockType::Foliage);
		set_block(x + 1,y - 2 - (x & 1), z, BlockType::Foliage);

		set_block(x + 2, y + 2, z, BlockType::Foliage);
		set_block(x + 2, y - 2, z, BlockType::Foliage);

		set_block(x + 3, y + 1 + ((x + 1) & 1), z, BlockType::Foliage);
		set_block(x + 3, y - 1 - (x & 1), z, BlockType::Foliage);
	}

	return structure;
}

StructureDescription GenBasicTree(std::vector<BlockType>& out_data)
{
	StructureDescription structure;
	structure.size[0]= 3;
	structure.size[1]= 3;
	structure.size[2]= 5;
	structure.center[0]= 1;
	structure.center[1]= 1;
	structure.center[2]= 0;

	const auto volume= structure.size[0] * structure.size[1] * structure.size[2];

	structure.data_offset= uint32_t(out_data.size());
	out_data.resize(out_data.size() + volume, BlockType::Air);

	const auto set_block=
		[&](const uint32_t x, const uint32_t y, const uint32_t z, const BlockType block_type)
		{
			HEX_ASSERT(x < structure.size[0]);
			HEX_ASSERT(y < structure.size[1]);
			HEX_ASSERT(z < structure.size[2]);
			const uint32_t offset= z + y * structure.size[2] + x * (structure.size[2] * structure.size[1]);
			out_data[structure.data_offset + offset]= block_type;
		};

	const uint32_t trunk_x= 1;
	const uint32_t trunk_y= 1;
	for(uint32_t z= 0; z < 4; ++z)
	{
		set_block(trunk_x, trunk_y, z, BlockType::Wood);

		if(z >= 2)
		{
			set_block(trunk_x, trunk_y + 1, z, BlockType::Foliage);
			set_block(trunk_x, trunk_y - 1, z, BlockType::Foliage);
			set_block(trunk_x + 1, trunk_y + ((trunk_x + 1) & 1), z, BlockType::Foliage);
			set_block(trunk_x + 1, trunk_y - (trunk_x & 1), z, BlockType::Foliage);
			set_block(trunk_x - 1, trunk_y + ((trunk_x + 1) & 1), z, BlockType::Foliage);
			set_block(trunk_x - 1, trunk_y - (trunk_x & 1), z, BlockType::Foliage);
		}
	}
	set_block(trunk_x, trunk_y, 4, BlockType::Foliage);

	return structure;
}

} // namespace
Structures GenStructures()
{
	Structures structures;
	structures.descriptions.push_back(GenLargeTree(structures.data));
	structures.descriptions.push_back(GenBasicTree(structures.data));

	return structures;
}

} // namespace HexGPU

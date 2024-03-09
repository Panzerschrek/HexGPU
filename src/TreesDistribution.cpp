#include "TreesDistribution.hpp"
#include "Assert.hpp"
#include "Tga.hpp"
#include <array>
#include <random>
#include <vector>

namespace HexGPU
{

namespace
{

using CubeHexCoord= std::array<int32_t, 3>;

CubeHexCoord GetCubeCoord(const std::array<int32_t, 2>& coord)
{
	// See https://www.redblobgames.com/grids/hexagons/#conversions.
	const int32_t q= coord[0];
	const int32_t r= coord[1] - ((coord[0] + (coord[0] & 1)) >> 1);
	return {q, r, -q - r};
}

int32_t HexDist(const std::array<int32_t, 2>& from, const std::array<int32_t, 2>& to)
{
	// See https://www.redblobgames.com/grids/hexagons/#distances.
	const CubeHexCoord from_cube= GetCubeCoord(from);
	const CubeHexCoord to_cube= GetCubeCoord(to);
	const int32_t diff[3]{from_cube[0] - to_cube[0], from_cube[1] - to_cube[1], from_cube[2] - to_cube[2]};
	return (std::abs(diff[0]) + std::abs(diff[1]) + std::abs(diff[2])) >> 1;
}

using DistributionSize= std::array<uint32_t, 2>;

struct Point
{
	std::array<int32_t, 2> coord{};
	uint32_t radius= 0;
};

void SaveTestDistribution(const std::vector<Point>& points, const DistributionSize size)
{
	std::vector<PixelType> pixels(size[0] * size[1], 0xFF000000);

	for(const Point& point : points)
	{
		for(int32_t dy= -int32_t(point.radius); dy <= int32_t(point.radius); ++dy)
		for(int32_t dx= -int32_t(point.radius); dx <= int32_t(point.radius); ++dx)
		{
			const int32_t dist= HexDist(point.coord, {point.coord[0] + dx, point.coord[1] + dy});
			if(dist > int32_t(point.radius))
				continue;
			const int32_t brightness= 255u * uint32_t(1 + point.radius - uint32_t(dist)) / (1 + point.radius);

			const int32_t x= uint32_t(dx + point.coord[0]) % size[0];
			const int32_t y= uint32_t(dy + point.coord[1]) % size[1];

			{
				PixelType& dst_pixel= pixels[x + y * size[0]];
				dst_pixel= 0xFF000000 | brightness | (brightness << 8) | (brightness << 16);
			}
		}
	}

	WriteTGA(uint16_t(size[0]), uint16_t(size[1]), pixels.data(), "test.tga");
}

// This generator should produce identical sequences on all platforms.
using RangGen= std::mt19937;

// Use our own functions for random generator sampling - for determinism.

RangGen::result_type RandUpTo(RangGen& gen, const uint32_t max_plus_one)
{
	return gen() % max_plus_one;
}

RangGen::result_type RandInRange(RangGen& gen, const uint32_t min, const uint32_t max_plus_one)
{
	HEX_ASSERT(min < max_plus_one);
	return min + gen() % (max_plus_one - min);
}

} // namespace

TreeMap GenTreeMap(const uint32_t seed)
{
	RangGen gen(seed);

	std::vector<Point> points;

	// TODO - remove quadratic complexity?
	const uint32_t num_points= 10000;

	// Caution! Minimal distance must be greater than maximum distance within one tree map cell!
	const uint32_t c_min_radius= 2;
	const uint32_t c_max_radius_plus_one= 5;

	for(uint32_t i= 0; i < num_points; ++i)
	{
		const Point point
		{
			{
				int32_t(RandUpTo(gen, c_tree_map_size[0])),
				int32_t(RandUpTo(gen, c_tree_map_size[1])),
			},
			uint32_t(RandInRange(gen, c_min_radius, c_max_radius_plus_one)),
		};
		HEX_ASSERT(point.coord[0] >= 0 && point.coord[0] < int32_t(c_tree_map_size[0]));
		HEX_ASSERT(point.coord[1] >= 0 && point.coord[1] < int32_t(c_tree_map_size[1]));

		bool too_close= false;

		for(const Point& prev_point : points)
		{
			std::array<int32_t, 2> prev_coord= prev_point.coord;
			// Add wrapping.
			for(uint32_t j= 0; j < 2; ++j)
			{
				if(prev_coord[j] - point.coord[j] >= +int32_t(c_tree_map_size[j] / 2))
					prev_coord[j]-= int32_t(c_tree_map_size[j]);
				if(prev_coord[j] - point.coord[j] <= -int32_t(c_tree_map_size[j] / 2))
					prev_coord[j]+= int32_t(c_tree_map_size[j]);
			}

			const int32_t dist= HexDist(point.coord, prev_coord);
			if(dist < int32_t(point.radius + prev_point.radius))
			{
				too_close= true;
				break;
			}
		}

		if(!too_close)
			points.push_back(point);
	}

	if(false)
		SaveTestDistribution(points, c_tree_map_size);

	// Make cell map from list of points.
	TreeMap tree_map;
	HEX_ASSERT(points.size() < 65535);
	for(size_t i= 0; i < points.size(); ++i)
	{
		const Point& point= points[i];
		const uint32_t cell_coord[2]
		{
			uint32_t(point.coord[0]) / c_tree_map_cell_size[0],
			uint32_t(point.coord[1]) / c_tree_map_cell_size[1],
		};
		HEX_ASSERT(cell_coord[0] < c_tree_map_cell_grid_size[0]);
		HEX_ASSERT(cell_coord[1] < c_tree_map_cell_grid_size[1]);

		TreeMapCell& cell= tree_map[cell_coord[0] + cell_coord[1] * c_tree_map_cell_grid_size[0]];
		HEX_ASSERT(cell.sequential_index == 0); // Should produce no more than 1 point per cell.

		cell.sequential_index= uint16_t(i + 1); // Skip 0 - no point indicator.
		cell.coord[0]= uint8_t(point.coord[0] & (c_tree_map_cell_size[0] - 1));
		cell.coord[1]= uint8_t(point.coord[1] & (c_tree_map_cell_size[1] - 1));
	}

	return tree_map;
}

} // namespace HexGPU

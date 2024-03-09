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
	const int32_t r= coord[1] - ((coord[0] - (coord[0] & 1)) >> 1);
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

void GenTestTreesDistribution()
{
	const DistributionSize size{512, 512};

	using RangGen= std::mt19937;
	const RangGen::result_type seed= 0;
	RangGen gen(seed);

	(void)RandInRange;

	std::vector<Point> points;

	// Use radii from big to low.
	const uint32_t radii[]= {32, 8, 4};
	for(const uint32_t radius : radii)
	{
		const uint32_t num_points= 100000 / (radius * radius);

		for(uint32_t i= 0; i < num_points; ++i)
		{
			const Point point
			{
				{
					int32_t(RandUpTo(gen, size[0])),
					int32_t(RandUpTo(gen, size[1])),
				},
				radius,
			};
			HEX_ASSERT(point.coord[0] >= 0 && point.coord[0] < int32_t(size[0]));
			HEX_ASSERT(point.coord[1] >= 0 && point.coord[1] < int32_t(size[1]));

			bool too_close= false;

			for(const Point& prev_point : points)
			{
				std::array<int32_t, 2> prev_coord= prev_point.coord;
				// Add wrapping.
				for(uint32_t j= 0; j < 2; ++j)
				{
					if(prev_coord[j] - point.coord[j] >= +int32_t(size[j] / 2))
						prev_coord[j]-= int32_t(size[j]);
					if(prev_coord[j] - point.coord[j] <= -int32_t(size[j] / 2))
						prev_coord[j]+= int32_t(size[j]);
				}

				const int32_t dist= HexDist(point.coord, prev_coord);
				if(dist < int32_t(radius * 2))
				{
					too_close= true;
					break;
				}
			}

			if(!too_close)
				points.push_back(point);
		}
	}

	SaveTestDistribution(points, size);
}

} // namespace HexGPU

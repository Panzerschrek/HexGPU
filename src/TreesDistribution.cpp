#include "TreesDistribution.hpp"
#include "Tga.hpp"
#include <array>
#include <random>
#include <vector>

namespace HexGPU
{

namespace
{

using DistributionSize= std::array<uint32_t, 2>;

using Point= std::array<int32_t, 2>;

void SaveTestDistribution(const std::vector<Point>& points, const DistributionSize size)
{
	std::vector<PixelType> pixels(size[0] * size[1], 0xFF000000);

	for(const Point& point : points)
	{
		if( point[0] >= 0 && point[0] < int32_t(size[0]) &&
			point[1] >= 0 && point[1] < int32_t(size[1]))
			pixels[uint32_t(point[0]) + uint32_t(point[1]) * size[0]]= 0xFFFFFFFF;
	}

	WriteTGA(uint16_t(size[0]), uint16_t(size[1]), pixels.data(), "test.tga");
}

}

void GenTestTreesDistribution()
{
	const DistributionSize size{256, 256};

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis[2]
	{
		std::uniform_real_distribution<float>(0.0f, float(size[0])),
		std::uniform_real_distribution<float>(0.0f, float(size[1])),
	};

	std::vector<Point> points;

	const int32_t min_distance= 8;
	const int32_t min_square_distance= min_distance * min_distance;

	// Make fixed number of tries dependent on size and distance.
	const uint32_t num_points= 20 * int32_t(size[0] * size[1]) / (min_distance * min_distance);

	for(uint32_t i= 0; i < num_points; ++i)
	{
		const Point point
		{
			int32_t(std::floor(dis[0](gen))),
			int32_t(std::floor(dis[1](gen))),
		};

		bool too_close= false;

		for(const Point& prev_point : points)
		{
			const int32_t diff[2]
			{
				point[0] - prev_point[0],
				point[1] - prev_point[1],
			};
			const int32_t square_dist= diff[0] * diff[0] + diff[1] * diff[1];
			if(square_dist < min_square_distance)
			{
				too_close= true;
				break;
			}
		}

		if(!too_close)
			points.push_back(point);
	}

	SaveTestDistribution(points, size);
}

} // namespace HexGPU

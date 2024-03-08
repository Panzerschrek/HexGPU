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
		const int32_t square_radius= int32_t(point.radius * point.radius);

		for(int32_t dy= -int32_t(point.radius); dy <= int32_t(point.radius); ++dy)
		for(int32_t dx= -int32_t(point.radius); dx <= int32_t(point.radius); ++dx)
		{
			const int32_t square_distnace= dx * dx + dy * dy;
			if(square_distnace >= square_radius)
				continue;

			const int32_t brightness= 255 - 240 * (square_radius - square_distnace) / square_radius;

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

} // namespace

void GenTestTreesDistribution()
{
	const DistributionSize size{512, 512};

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis[2]
	{
		std::uniform_real_distribution<float>(0.0f, float(size[0])),
		std::uniform_real_distribution<float>(0.0f, float(size[1])),
	};

	std::uniform_real_distribution<float> radius_distribution{4.0f, 16.0f};

	std::vector<Point> points;

	const uint32_t num_points= 100000;

	for(uint32_t i= 0; i < num_points; ++i)
	{
		const Point point
		{
			{
				int32_t(std::floor(dis[0](gen))),
				int32_t(std::floor(dis[1](gen))),
			},
			uint32_t(std::floor(radius_distribution(gen))),
		};

		bool too_close= false;

		for(const Point& prev_point : points)
		{
			int32_t diff[2]
			{
				point.coord[0] - prev_point.coord[0],
				point.coord[1] - prev_point.coord[1],
			};
			// Add wrapping.
			for(uint32_t j= 0; j < 2; ++j)
			{
				if(diff[j] <= -int32_t(size[j]) / 2)
					diff[j]+= int32_t(size[j]);
				else if(diff[j] >= int32_t(size[j]) / 2)
					diff[j]-= int32_t(size[j]);
			}

			const int32_t square_dist= diff[0] * diff[0] + diff[1] * diff[1];
			const int32_t min_ditance= int32_t(point.radius + prev_point.radius);
			if(square_dist < min_ditance * min_ditance)
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

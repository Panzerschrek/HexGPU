#pragma once
#include "Assert.hpp"
#include <cstdint>

namespace HexGPU
{

// Assumes y is non-negative.
inline int32_t EuclidianRemainder(const int32_t x, const int32_t y)
{
	HEX_ASSERT(y > 0);
	const int32_t r = x % y;
	const int32_t r_corrected= r >= 0 ? r : r + y;
	HEX_ASSERT(r_corrected >= 0 && r_corrected < y);
	return r_corrected;
}

} // namespace HexGPU

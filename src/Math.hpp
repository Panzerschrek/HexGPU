#pragma once
#include "Assert.hpp"
#include <cstdint>

namespace HexGPU
{

// Assumes y is positive.
inline int32_t EuclidianRemainder(const int32_t x, const int32_t y)
{
	HEX_ASSERT(y > 0);
	const int32_t r = x % y;
	const int32_t r_corrected= r >= 0 ? r : r + y;
	HEX_ASSERT(r_corrected >= 0 && r_corrected < y);
	return r_corrected;
}

// Assumes y is positive.
inline int32_t EuclidianDiv(const int32_t x, const int32_t y)
{
	HEX_ASSERT(y > 0);
	const int32_t mod= x % y;
	int32_t div= x / y;
	if(mod < 0)
		--div;
	return div;
}


} // namespace HexGPU

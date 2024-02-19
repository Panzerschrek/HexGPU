#pragma once
#include <cassert>

// Simple assert wrapper.
// If you wish disable asserts, or do something else redefine this macro.
#ifdef DEBUG
#define HEX_ASSERT(x) \
	{ assert(x); }
#else
#define HEX_ASSERT(x) { (void)(x); }
#endif

#define HEX_UNUSED(x) { (void)(x); }

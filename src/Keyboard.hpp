#pragma once
#include <cstdint>

namespace HexGPU
{

// Bits of keyboard state. Must match bits in GLSL code.

using KeyboardState= uint32_t;

const KeyboardState c_key_mask_forward= 1;
const KeyboardState c_key_mask_backward= 2;
const KeyboardState c_key_mask_step_left= 4;
const KeyboardState c_key_mask_step_right= 8;
const KeyboardState c_key_mask_fly_up= 16;
const KeyboardState c_key_mask_fly_down= 32;
const KeyboardState c_key_mask_rotate_left= 64;
const KeyboardState c_key_mask_rotate_right= 128;
const KeyboardState c_key_mask_rotate_up= 256;
const KeyboardState c_key_mask_rotate_down= 512;

} // namespace HexGPU

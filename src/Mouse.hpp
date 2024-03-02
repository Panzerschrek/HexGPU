#pragma once

namespace HexGPU
{

// Bits of mouse state. Must match bits in GLSL code.

using MouseState= uint32_t;

const MouseState c_mouse_mask_l_clicked= 1;
const MouseState c_mouse_mask_r_clicked= 2;
const MouseState c_mouse_mask_m_clicked= 4;
const MouseState c_mouse_mask_wheel_up_clicked= 8;
const MouseState c_mouse_mask_wheel_down_clicked= 16;

} // namespace HexGPU

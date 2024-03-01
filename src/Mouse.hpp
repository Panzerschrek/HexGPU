#pragma once

namespace HexGPU
{

// Bits of mouse state. Must match bits in GLSL code.

using MouseState= uint32_t;

const MouseState c_mouse_l_clicked_bit= 0;
const MouseState c_mouse_r_clicled_bit= 1;
const MouseState c_mouse_m_clicled_bit= 2;

} // namespace HexGPU

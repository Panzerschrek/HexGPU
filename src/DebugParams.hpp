#pragma once

namespace HexGPU
{

struct DebugParams
{
	float time_of_day= 0.5f;
	float rain_intensity= 0.0f;
	bool drought= false;
	int32_t snow_z_level= 128;
	bool frame_rate_world_update= false; // If true - try to perform full world update each frame.
};

} // namespace HexGPU

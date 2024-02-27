#pragma once
#include "BuildPrismRenderer.hpp"
#include "CameraController.hpp"
#include "SystemWindow.hpp"
#include "WorldRenderer.hpp"
#include <chrono>

namespace HexGPU
{

class Host final
{
public:
	Host();

	// Returns false on quit
	bool Loop();

private:
	using Clock= std::chrono::steady_clock;

	SystemWindow system_window_;
	WindowVulkan window_vulkan_;
	vk::UniqueDescriptorPool global_descriptor_pool_;
	WorldProcessor world_processor_;
	WorldRenderer world_renderer_;
	BuildPrismRenderer build_prism_renderer_;

	CameraController camera_controller_;

	BlockType build_block_type_= BlockType::Stone;

	const Clock::time_point init_time_;
	Clock::time_point prev_tick_time_;

	bool quit_requested_= false;
};

} // namespace HexGPU

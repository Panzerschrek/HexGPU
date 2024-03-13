#pragma once
#include "BuildPrismRenderer.hpp"
#include "ImGuiWrapper.hpp"
#include "SkyRenderer.hpp"
#include "TaskOrganizer.hpp"
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

	Settings settings_;
	SystemWindow system_window_;
	WindowVulkan window_vulkan_;
	TaskOrganizer task_organizer_;
	const vk::UniqueDescriptorPool global_descriptor_pool_;
	ImGuiWrapper im_gui_wrapper_;
	WorldProcessor world_processor_;
	WorldRenderer world_renderer_;
	SkyRenderer sky_renderer_;
	BuildPrismRenderer build_prism_renderer_;

	const Clock::time_point init_time_;
	Clock::time_point prev_tick_time_;

	bool quit_requested_= false;
};

} // namespace HexGPU

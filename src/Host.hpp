#pragma once
#include "BuildPrismRenderer.hpp"
#include "ImGuiWrapper.hpp"
#include "SkyRenderer.hpp"
#include "TaskOrganizer.hpp"
#include "TicksCounter.hpp"
#include "WorldRenderer.hpp"
#include <chrono>

namespace HexGPU
{

class Host
{
public:
	Host();

	// Returns false on quit
	bool Loop();

private:

	void DrawFPS();
	void DrawUI();
	void DrawBlockSelectionUI();
	void DrawCrosshair();
	void DrawDebugInfo();
	void DrawDebugParamsUI();

private:
	using Clock= std::chrono::steady_clock;

	Settings settings_;
	SystemWindow system_window_;
	WindowVulkan window_vulkan_;
	TaskOrganizer task_organizer_;
	GPUDataUploader gpu_data_uploader_;
	const vk::UniqueDescriptorPool global_descriptor_pool_;
	ImGuiWrapper im_gui_wrapper_;
	WorldRenderPass world_render_pass_;
	WorldProcessor world_processor_;
	WorldRenderer world_renderer_;
	SkyRenderer sky_renderer_;
	BuildPrismRenderer build_prism_renderer_;

	const Clock::time_point init_time_;
	Clock::time_point prev_tick_time_;
	float accumulated_time_s_= 0.0f;

	TicksCounter ticks_counter_;

	DebugParams debug_params_;

	bool show_debug_menus_= false;

	bool blocks_selection_menu_active_= false;
	BlockType selected_block_type_= BlockType::Air;
};

} // namespace HexGPU

#include "Host.hpp"
#include "Assert.hpp"
#include "GlobalDescriptorPool.hpp"
#include "Log.hpp"
#include <thread>


namespace HexGPU
{

namespace
{

float CalculateAspect(const vk::Extent2D& viewport_size)
{
	return float(viewport_size.width) / float(viewport_size.height);
}

} // namespace

Host::Host()
	: settings_("HexGPU.cfg")
	, system_window_(settings_)
	, window_vulkan_(system_window_)
	, global_descriptor_pool_(CreateGlobalDescriptorPool(window_vulkan_.GetVulkanDevice()))
	, world_processor_(window_vulkan_, *global_descriptor_pool_, settings_)
	, world_renderer_(window_vulkan_, world_processor_, *global_descriptor_pool_)
	, build_prism_renderer_(window_vulkan_, world_processor_, *global_descriptor_pool_)
	, camera_controller_(CalculateAspect(window_vulkan_.GetViewportSize()))
	, init_time_(Clock::now())
	, prev_tick_time_(init_time_)
{
}

bool Host::Loop()
{
	const Clock::time_point tick_start_time= Clock::now();
	const auto dt= tick_start_time - prev_tick_time_;
	prev_tick_time_ = tick_start_time;

	const float dt_s= float(dt.count()) * float(Clock::duration::period::num) / float(Clock::duration::period::den);

	bool build_triggered= false, destroy_triggered= false;
	for( const SDL_Event& event : system_window_.ProcessEvents() )
	{
		if(event.type == SDL_QUIT)
			quit_requested_= true;
		if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
			quit_requested_= true;
		if(event.type == SDL_MOUSEBUTTONDOWN)
		{
			if(event.button.button == SDL_BUTTON_LEFT)
				destroy_triggered= true;
			if(event.button.button == SDL_BUTTON_RIGHT)
				build_triggered= true;
		}
		if(event.type == SDL_MOUSEWHEEL)
		{
			if(event.wheel.y > 0)
			{
				build_block_type_= BlockType((int32_t(build_block_type_) + 1) % int32_t(BlockType::NumBlockTypes));
				if(build_block_type_ == BlockType::Air)
					build_block_type_= BlockType(int32_t(BlockType::Air) + 1);
			}
			if(event.wheel.y < 0)
			{
				build_block_type_= BlockType((int32_t(build_block_type_) + int32_t(BlockType::NumBlockTypes) - 1) % int32_t(BlockType::NumBlockTypes));
			}
		}
	}

	camera_controller_.Update(dt_s, system_window_.GetKeyboardState());
	const m_Mat4 view_matrix= camera_controller_.CalculateFullViewMatrix();

	const vk::CommandBuffer command_buffer= window_vulkan_.BeginFrame();

	// TODO - pass directly events and keyboard state.
	world_processor_.Update(
		command_buffer,
		dt_s,
		camera_controller_.GetCameraPosition(),
		camera_controller_.GetCameraDirection(),
		build_block_type_,
		build_triggered,
		destroy_triggered);

	world_renderer_.PrepareFrame(command_buffer);
	build_prism_renderer_.PrepareFrame(command_buffer, view_matrix);

	window_vulkan_.EndFrame(
		[&](const vk::CommandBuffer command_buffer)
		{
			world_renderer_.Draw(command_buffer, view_matrix);
			build_prism_renderer_.Draw(command_buffer);
		});

	const Clock::time_point tick_end_time= Clock::now();
	const auto frame_dt= tick_end_time - tick_start_time;

	// std::cout << "\rframe time: " << std::chrono::duration_cast<std::chrono::milliseconds>(frame_dt).count() << "ms" << std::endl;

	const float max_fps= 120.0f;

	const std::chrono::milliseconds min_frame_duration(uint32_t(1000.0f / max_fps));
	if(frame_dt <= min_frame_duration)
	{
		std::this_thread::sleep_for(min_frame_duration - frame_dt);
	}

	return quit_requested_;
}

} // namespace HexGPU

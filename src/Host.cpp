#include "Host.hpp"
#include "Assert.hpp"
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
	: system_window_()
	, window_vulkan_(system_window_)
	, world_processor_(window_vulkan_)
	, world_renderer_(window_vulkan_, world_processor_)
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

	for( const SDL_Event& event : system_window_.ProcessEvents() )
	{
		if( event.type == SDL_QUIT )
			quit_requested_= true;
		if( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE )
			quit_requested_= true;
	}

	camera_controller_.Update(dt_s, system_window_.GetKeyboardState());

	const vk::CommandBuffer command_buffer= window_vulkan_.BeginFrame();

	world_processor_.PrepareFrame(command_buffer);
	world_renderer_.PrepareFrame(command_buffer);

	window_vulkan_.EndFrame(
		[&](const vk::CommandBuffer command_buffer)
		{
			world_renderer_.Draw(command_buffer, camera_controller_.CalculateFullViewMatrix());
		});

	const Clock::time_point tick_end_time= Clock::now();
	const auto frame_dt= tick_end_time - tick_start_time;

	const float max_fps= 20.0f;

	const std::chrono::milliseconds min_frame_duration(uint32_t(1000.0f / max_fps));
	if(frame_dt <= min_frame_duration)
	{
		std::this_thread::sleep_for(min_frame_duration - frame_dt);
	}

	return quit_requested_;
}

} // namespace HexGPU

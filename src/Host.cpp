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

	MouseState mouse_state= 0;
	for(const SDL_Event& event : system_window_.ProcessEvents())
	{
		if(event.type == SDL_QUIT)
			quit_requested_= true;
		if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
			quit_requested_= true;
		if(event.type == SDL_MOUSEBUTTONDOWN)
		{
			if(event.button.button == SDL_BUTTON_LEFT)
				mouse_state|= c_mouse_mask_l_clicked;
			if(event.button.button == SDL_BUTTON_RIGHT)
				mouse_state|= c_mouse_mask_r_clicked;
		}
		if(event.type == SDL_MOUSEWHEEL)
		{
			if(event.wheel.y > 0)
				mouse_state|= c_mouse_mask_wheel_up_clicked;
			if(event.wheel.y < 0)
				mouse_state|= c_mouse_mask_wheel_down_clicked;
		}
	}

	const auto keys_state= system_window_.GetKeyboardState();
	KeyboardState keyboard_state= 0;
	if(keys_state[size_t(SDL_SCANCODE_W)])
		keyboard_state|= c_key_mask_forward;
	if(keys_state[size_t(SDL_SCANCODE_S)])
		keyboard_state|= c_key_mask_backward;
	if(keys_state[size_t(SDL_SCANCODE_D)])
		keyboard_state|= c_key_mask_step_left;
	if(keys_state[size_t(SDL_SCANCODE_A)])
		keyboard_state|= c_key_mask_step_right;
	if(keys_state[size_t(SDL_SCANCODE_SPACE)])
		keyboard_state|= c_key_mask_fly_up;
	if(keys_state[size_t(SDL_SCANCODE_C)])
		keyboard_state|= c_key_mask_fly_down;
	if(keys_state[size_t(SDL_SCANCODE_LEFT)])
		keyboard_state|= c_key_mask_rotate_left;
	if(keys_state[size_t(SDL_SCANCODE_RIGHT)])
		keyboard_state|= c_key_mask_rotate_right;
	if(keys_state[size_t(SDL_SCANCODE_UP)])
		keyboard_state|= c_key_mask_rotate_up;
	if(keys_state[size_t(SDL_SCANCODE_DOWN)])
		keyboard_state|= c_key_mask_rotate_down;

	const vk::CommandBuffer command_buffer= window_vulkan_.BeginFrame();

	world_processor_.Update(
		command_buffer,
		dt_s,
		keyboard_state,
		mouse_state,
		CalculateAspect(window_vulkan_.GetViewportSize()));

	world_renderer_.PrepareFrame(command_buffer);
	build_prism_renderer_.PrepareFrame(command_buffer);

	window_vulkan_.EndFrame(
		[&](const vk::CommandBuffer command_buffer)
		{
			world_renderer_.Draw(command_buffer);
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

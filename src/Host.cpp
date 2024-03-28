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

KeyboardState CreateKeyboardState(const std::vector<bool>& keys_state)
{
	// TODO - read key bindings from config.
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
	if(keys_state[size_t(SDL_SCANCODE_LSHIFT)])
		keyboard_state|= c_key_mask_sprint;

	return keyboard_state;
}

MouseState CreateMouseState(const std::vector<SDL_Event>& events)
{
	MouseState mouse_state= 0;

	for(const SDL_Event& event : events)
	{
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

	return mouse_state;
}

std::array<float, 2> GetMouseMove(const std::vector<SDL_Event>& events, Settings& settings)
{
	const float mouse_speed= std::max(1.0f, std::min(settings.GetReal("in_mouse_speed", 4.0f), 16.0f));
	settings.SetReal("in_mouse_speed", mouse_speed);

	const bool invert_mouse_y= settings.GetOrSetInt("in_invert_mouse_y", 0) != 0;

	std::array<float, 2> move{0.0f, 0.0f};

	for(const SDL_Event& event : events)
	{
		if(event.type == SDL_MOUSEMOTION)
		{
			move[0]-= float(event.motion.xrel) * (mouse_speed / 1024.0f);
			move[1]-= float(event.motion.yrel) * (mouse_speed / 1024.0f) * (invert_mouse_y ? -1.0f : 1.0f);
		}
	}

	return move;
}

} // namespace

Host::Host()
	: settings_("HexGPU.cfg")
	, system_window_(settings_)
	, window_vulkan_(system_window_, settings_)
	, task_organizer_(window_vulkan_)
	, gpu_data_uploader_(window_vulkan_)
	, global_descriptor_pool_(CreateGlobalDescriptorPool(window_vulkan_.GetVulkanDevice()))
	, im_gui_wrapper_(system_window_, window_vulkan_)
	, world_render_pass_(window_vulkan_, *global_descriptor_pool_)
	, world_processor_(window_vulkan_, gpu_data_uploader_, *global_descriptor_pool_, settings_)
	, world_renderer_(window_vulkan_, gpu_data_uploader_, world_processor_, *global_descriptor_pool_)
	, sky_renderer_(window_vulkan_, world_render_pass_, world_processor_, *global_descriptor_pool_)
	, build_prism_renderer_(window_vulkan_, world_processor_, *global_descriptor_pool_)
	, init_time_(Clock::now())
	, prev_tick_time_(init_time_)
	, ticks_counter_(std::chrono::milliseconds(500))
{
}

bool Host::Loop()
{
	const Clock::time_point tick_start_time= Clock::now();
	const auto dt= tick_start_time - prev_tick_time_;
	prev_tick_time_ = tick_start_time;

	const float dt_s= float(dt.count()) * float(Clock::duration::period::num) / float(Clock::duration::period::den);
	// Prevent too little or too much frame delta whic is used for world/physics simulation.
	// This helps especially in debugging.
	const float dt_s_limited= std::max(1.0f / 2048.0f, std::min(dt_s, 1.0f / 8.0f));

	accumulated_time_s_+= dt_s_limited;

	ticks_counter_.Tick();

	const auto keys_state= system_window_.GetKeyboardState();
	const auto events= system_window_.ProcessEvents();

	for(const SDL_Event& event : events)
	{
		if(event.type == SDL_QUIT)
			return true;
		if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
			return true;
		if(event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
			return true;
		if(event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_GRAVE)
			show_debug_menus_= !show_debug_menus_;
		if(event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_E)
			blocks_selection_menu_active_= !blocks_selection_menu_active_;
	}

	im_gui_wrapper_.ProcessEvents(events);
	im_gui_wrapper_.BeginFrame();

	DrawFPS();
	DrawUI();

	if(blocks_selection_menu_active_)
		DrawBlockSelectionUI();
	else
		selected_block_type_= BlockType::Air;

	DrawCrosshair();
	if(show_debug_menus_)
	{
		DrawDebugInfo();
		DrawDebugParamsUI();
	}

	const bool game_has_focus= !show_debug_menus_ && !blocks_selection_menu_active_;
	system_window_.SetMouseCaptured(game_has_focus);

	const vk::CommandBuffer command_buffer= window_vulkan_.BeginFrame();
	task_organizer_.SetCommandBuffer(command_buffer);

	world_processor_.Update(
		task_organizer_,
		dt_s_limited,
		game_has_focus ? CreateKeyboardState(keys_state) : 0,
		game_has_focus ? CreateMouseState(events) : 0,
		game_has_focus ? GetMouseMove(events, settings_) : std::array<float, 2>{0.0f, 0.0f},
		selected_block_type_,
		CalculateAspect(window_vulkan_.GetViewportSize()),
		debug_params_);

	world_renderer_.PrepareFrame(task_organizer_);
	build_prism_renderer_.PrepareFrame(task_organizer_);
	sky_renderer_.PrepareFrame(task_organizer_);

	{
		TaskOrganizer::GraphicsTaskParams task_params;
		sky_renderer_.CollectFrameInputs(task_params);

		task_params.framebuffer= world_render_pass_.GetFramebuffer();
		task_params.viewport_size= world_render_pass_.GetFramebufferSize();
		task_params.render_pass= world_render_pass_.GetRenderPass();
		world_render_pass_.CollectPassOutputs(task_params);

		task_params.clear_values=
		{
			vk::ClearColorValue(std::array<float,4>{1.0f, 0.0f, 1.0f, 1.0f}), // Clear with pink to catch some mistakes.
			vk::ClearDepthStencilValue(1.0f, 0u),
		};

		const auto func=
			[this](const vk::CommandBuffer command_buffer)
		{
			sky_renderer_.Draw(command_buffer, accumulated_time_s_);
		};

		task_organizer_.ExecuteTask(task_params, func);
	}

	TaskOrganizer::GraphicsTaskParams graphics_task_params;
	world_renderer_.CollectFrameInputs(graphics_task_params);
	build_prism_renderer_.CollectFrameInputs(graphics_task_params);

	graphics_task_params.render_pass= window_vulkan_.GetRenderPass();
	graphics_task_params.viewport_size= window_vulkan_.GetViewportSize();

	const auto graphics_task_func=
		[this](const vk::CommandBuffer command_buffer)
		{
			world_renderer_.DrawOpaque(command_buffer);
			world_renderer_.DrawTransparent(command_buffer, accumulated_time_s_);
			build_prism_renderer_.Draw(command_buffer);
			world_render_pass_.Draw(command_buffer);

			im_gui_wrapper_.EndFrame(command_buffer);
		};

	graphics_task_params.clear_values=
	{
		vk::ClearColorValue(std::array<float,4>{1.0f, 0.0f, 1.0f, 1.0f}), // Clear with pink to catch some mistakes.
		vk::ClearDepthStencilValue(1.0f, 0u),
	};

	window_vulkan_.EndFrame(
		[&](const vk::Framebuffer framebuffer)
		{
			graphics_task_params.framebuffer= framebuffer;
			task_organizer_.ExecuteTask(graphics_task_params, graphics_task_func);
		});

	const Clock::time_point tick_end_time= Clock::now();
	const auto frame_dt= tick_end_time - tick_start_time;

	// std::cout << "\rframe time: " << std::chrono::duration_cast<std::chrono::milliseconds>(frame_dt).count() << "ms" << std::endl;

	const float max_fps= std::max(1.0f, std::min(settings_.GetReal("r_max_fps", 120.0f), 500.0f));
	settings_.SetReal("r_max_fps", max_fps);

	const std::chrono::milliseconds min_frame_duration(uint32_t(1000.0f / max_fps));
	if(frame_dt <= min_frame_duration)
	{
		std::this_thread::sleep_for(min_frame_duration - frame_dt);
	}

	return false;
}

void Host::DrawFPS()
{
	const float offset= 90.0f;

	ImGui::SetNextWindowPos(
		{float(window_vulkan_.GetViewportSize().width) - offset, 0});

	ImGui::SetNextWindowSize({offset, 44.0f});

	ImGui::SetNextWindowBgAlpha(0.25f);
	ImGui::Begin(
		"FPS conter",
		nullptr,
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::Text("fps: %3.2f", ticks_counter_.GetTicksFrequency());
	ImGui::Text("%3.2f ms", 1000.0f / ticks_counter_.GetTicksFrequency());

	ImGui::End();
}

void Host::DrawUI()
{
	const auto player_state= world_processor_.GetLastKnownPlayerState();
	if(player_state == nullptr)
		return;

	ImGui::PushFont(im_gui_wrapper_.GetLargeFont());

	const std::string text= std::string("build block: ") + BlockTypeToString(player_state->build_block_type);

	const auto text_size= ImGui::CalcTextSize(text.c_str());
	const auto padding= ImGui::GetStyle().WindowPadding;

	ImGui::SetNextWindowPos({0.0f, float(window_vulkan_.GetViewportSize().height) - 64.0f});

	ImGui::SetNextWindowSize({text_size.x + padding.x * 2.0f, text_size.y + padding.y * 2.0f});

	ImGui::SetNextWindowBgAlpha(0.25f);
	ImGui::Begin(
		"Build Block UI",
		nullptr,
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::Text("%s", text.c_str());

	ImGui::End();

	ImGui::PopFont();
}

void Host::DrawBlockSelectionUI()
{
	const auto font= im_gui_wrapper_.GetMediumFont();
	const auto& style= ImGui::GetStyle();

	const uint32_t num_block_types_shown= uint32_t(BlockType::NumBlockTypes) - 1;

	const float window_height=
		style.WindowPadding.y * 2.0f +
		(font->FontSize + style.ItemSpacing.y) * float(num_block_types_shown + 1);

	ImGui::PushFont(font);

	ImGui::SetNextWindowPos({128.0f, 64.0f});

	ImGui::SetNextWindowSize({200.0f, window_height});

	ImGui::SetNextWindowBgAlpha(0.25f);
	ImGui::Begin(
		"Block select",
		nullptr,
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

	for(size_t i= 1; i < size_t(BlockType::NumBlockTypes); ++i)
	{
		if(ImGui::SmallButton(BlockTypeToString(BlockType(i))))
		{
			selected_block_type_= BlockType(i);
			blocks_selection_menu_active_= false; // Deactivete block selection menu when block is selected.
		}
	}

	ImGui::End();

	ImGui::PopFont();
}

void Host::DrawCrosshair()
{
	// For now use simplest window to show something like crosshair.
	// TODO - draw a simple image instead.

	const auto viewport_size= window_vulkan_.GetViewportSize();

	const float size= 12.0f;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, size / 2.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {size, size});

	ImGui::SetNextWindowPos(
		{float(viewport_size.width) / 2.0f, float(viewport_size.height) / 2.0f},
		ImGuiCond_Always,
		{0.5f, 0.5f});

	ImGui::SetNextWindowSize({size, size});

	ImGui::Begin(
		"CrosshairWindow",
		nullptr,
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::End();

	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}

void Host::DrawDebugInfo()
{
	ImGui::SetNextWindowBgAlpha(0.25f);

	ImGui::SetNextWindowSizeConstraints({200.0f, 64.0f}, {800.0f, 600.0f});
	ImGui::SetNextWindowSize({300.0f, 200.0f});
	ImGui::SetNextWindowPos({0.0f, 0.0f}, ImGuiCond_Appearing);

	ImGui::Begin(
		"HexGPU debug info",
		nullptr);

	const auto world_size= world_processor_.GetWorldSize();
	ImGui::Text("World size (chunks): %dx%d", world_size[0], world_size[1]);

	const auto world_offset= world_processor_.GetWorldOffset();
	ImGui::Text("World offset (chunks): %d, %d", world_offset[0], world_offset[1]);

	if(const auto player_state= world_processor_.GetLastKnownPlayerState())
		ImGui::Text("Player pos: %4.2f, %4.2f, %4.2f", player_state->pos[0], player_state->pos[1], player_state->pos[2]);

	ImGui::End();
}

void Host::DrawDebugParamsUI()
{
	ImGui::SetNextWindowBgAlpha(0.25f);
	ImGui::SetNextWindowSize({450.0f, 300.0f}, ImGuiCond_Appearing);
	ImGui::SetNextWindowPos({float(window_vulkan_.GetViewportSize().width) - 450.0f, 128.0f}, ImGuiCond_Appearing);
	ImGui::Begin("HexGPU debug params", nullptr);
	{
		ImGui::SliderFloat("Time of day", &debug_params_.time_of_day, 0.0f, 1.0f);
		ImGui::SliderFloat("Rain intensity", &debug_params_.rain_intensity, 0.0f, 1.0f);
		ImGui::Checkbox("Frame rate world update", &debug_params_.frame_rate_world_update);
	}

	ImGui::End();

}

} // namespace HexGPU

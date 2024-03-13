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

} // namespace

Host::Host()
	: settings_("HexGPU.cfg")
	, system_window_(settings_)
	, window_vulkan_(system_window_)
	, imgui_context_(ImGui::CreateContext())
	, task_organizer_(window_vulkan_)
	, global_descriptor_pool_(CreateGlobalDescriptorPool(window_vulkan_.GetVulkanDevice()))
	, world_processor_(window_vulkan_, *global_descriptor_pool_, settings_)
	, world_renderer_(window_vulkan_, world_processor_, *global_descriptor_pool_)
	, sky_renderer_(window_vulkan_, world_processor_, *global_descriptor_pool_)
	, build_prism_renderer_(window_vulkan_, world_processor_, *global_descriptor_pool_)
	, init_time_(Clock::now())
	, prev_tick_time_(init_time_)
{
	ImGui_ImplSDL2_InitForVulkan(system_window_.GetSDLWindow());

	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.Instance = window_vulkan_.GetVulkanInstance();
	init_info.PhysicalDevice = window_vulkan_.GetPhysicalDevice();
	init_info.Device = window_vulkan_.GetVulkanDevice();
	init_info.QueueFamily = window_vulkan_.GetQueueFamilyIndex();
	init_info.Queue = window_vulkan_.GetQueue();
	init_info.PipelineCache = nullptr;
	init_info.DescriptorPool = *global_descriptor_pool_;
	init_info.RenderPass = window_vulkan_.GetRenderPass();
	init_info.Subpass = 0;
	init_info.MinImageCount = window_vulkan_.GetFramebufferImageCount();
	init_info.ImageCount = window_vulkan_.GetFramebufferImageCount();
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator = nullptr;
	init_info.CheckVkResultFn = nullptr;
	ImGui_ImplVulkan_Init(&init_info);

	ImGuiIO& io= ImGui::GetIO();
	io.Fonts->AddFontDefault();
}

Host::~Host()
{
	window_vulkan_.GetVulkanDevice().waitIdle();
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext(imgui_context_);
}

bool Host::Loop()
{
	const Clock::time_point tick_start_time= Clock::now();
	const auto dt= tick_start_time - prev_tick_time_;
	prev_tick_time_ = tick_start_time;

	const float dt_s= float(dt.count()) * float(Clock::duration::period::num) / float(Clock::duration::period::den);

	const float absoulte_time_s=
		float((tick_start_time - init_time_).count()) * float(Clock::duration::period::num) / float(Clock::duration::period::den);

	const auto keys_state= system_window_.GetKeyboardState();
	const auto events= system_window_.ProcessEvents();

	for(const SDL_Event& event : events)
	{
		ImGui_ImplSDL2_ProcessEvent(&event);

		if(event.type == SDL_QUIT)
			quit_requested_= true;
		if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
			quit_requested_= true;
	}

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("Hello, world!");
		ImGui::Text("This is some useful text.");
		ImGui::End();
	}

	ImGui::Render();

	ImDrawData* const draw_data= ImGui::GetDrawData();

	const vk::CommandBuffer command_buffer= window_vulkan_.BeginFrame();
	task_organizer_.SetCommandBuffer(command_buffer);

	world_processor_.Update(
		task_organizer_,
		dt_s,
		CreateKeyboardState(keys_state),
		CreateMouseState(events),
		CalculateAspect(window_vulkan_.GetViewportSize()));

	world_renderer_.PrepareFrame(task_organizer_);
	build_prism_renderer_.PrepareFrame(task_organizer_);
	sky_renderer_.PrepareFrame(task_organizer_);

	TaskOrganizer::GraphicsTaskParams graphics_task_params;
	world_renderer_.CollectFrameInputs(graphics_task_params);
	build_prism_renderer_.CollectFrameInputs(graphics_task_params);
	sky_renderer_.CollectFrameInputs(graphics_task_params);

	graphics_task_params.render_pass= window_vulkan_.GetRenderPass();
	graphics_task_params.viewport_size= window_vulkan_.GetViewportSize();

	const auto graphics_task_func=
		[this, absoulte_time_s, draw_data](const vk::CommandBuffer command_buffer)
		{
			world_renderer_.DrawOpaque(command_buffer);
			sky_renderer_.Draw(command_buffer);
			world_renderer_.DrawTransparent(command_buffer, absoulte_time_s);
			build_prism_renderer_.Draw(command_buffer);

			ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
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

	const float max_fps= 120.0f;

	const std::chrono::milliseconds min_frame_duration(uint32_t(1000.0f / max_fps));
	if(frame_dt <= min_frame_duration)
	{
		std::this_thread::sleep_for(min_frame_duration - frame_dt);
	}

	return quit_requested_;
}

} // namespace HexGPU

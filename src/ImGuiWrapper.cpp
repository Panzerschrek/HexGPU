#include "ImGuiWrapper.hpp"

namespace HexGPU
{

ImGuiWrapper::ImGuiWrapper(
	SystemWindow& system_window,
	WindowVulkan& window_vulkan,
	const vk::DescriptorPool global_descriptor_pool)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, context_(ImGui::CreateContext())
{
	ImGui_ImplSDL2_InitForVulkan(system_window.GetSDLWindow());

	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.Instance = window_vulkan.GetVulkanInstance();
	init_info.PhysicalDevice = window_vulkan.GetPhysicalDevice();
	init_info.Device = window_vulkan.GetVulkanDevice();
	init_info.QueueFamily = window_vulkan.GetQueueFamilyIndex();
	init_info.Queue = window_vulkan.GetQueue();
	init_info.PipelineCache = nullptr;
	init_info.DescriptorPool = global_descriptor_pool;
	init_info.RenderPass = window_vulkan.GetRenderPass();
	init_info.Subpass = 0;
	init_info.MinImageCount = window_vulkan.GetFramebufferImageCount();
	init_info.ImageCount = window_vulkan.GetFramebufferImageCount();
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator = nullptr;
	init_info.CheckVkResultFn = nullptr;
	ImGui_ImplVulkan_Init(&init_info);

	ImGuiIO& io= ImGui::GetIO();
	io.Fonts->AddFontDefault();
}

ImGuiWrapper::~ImGuiWrapper()
{
	vk_device_.waitIdle();
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext(context_);
}

void ImGuiWrapper::ProcessEvents(const std::vector<SDL_Event>& events)
{
	for(const SDL_Event& event : events)
		ImGui_ImplSDL2_ProcessEvent(&event);
}

void ImGuiWrapper::BeginFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("Hello, world!");
		ImGui::Text("This is some useful text.");
		ImGui::End();
	}
}

void ImGuiWrapper::EndFrame(const vk::CommandBuffer command_buffer)
{
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
}

} // namespace HexGPU

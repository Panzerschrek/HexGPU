#include "ImGuiWrapper.hpp"

namespace HexGPU
{

namespace
{

vk::UniqueDescriptorPool CreateImGuiDescriptorPool(const vk::Device vk_device)
{
	const uint32_t max_sets= 64u;

	// Add here other sizes if necessary.
	const vk::DescriptorPoolSize descriptor_pool_sizes[]
	{
		{vk::DescriptorType::eCombinedImageSampler, 64u},
	};

	return
		vk_device.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, // Allow freeing individual sets.
				max_sets,
				uint32_t(std::size(descriptor_pool_sizes)), descriptor_pool_sizes));
}

} // namespace

ImGuiWrapper::ImGuiWrapper(
	SystemWindow& system_window,
	WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, descriptor_pool_(CreateImGuiDescriptorPool(vk_device_))
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
	init_info.DescriptorPool = *descriptor_pool_;
	init_info.RenderPass = window_vulkan.GetRenderPass();
	init_info.Subpass = 0;
	init_info.MinImageCount = window_vulkan.GetFramebufferImageCount();
	init_info.ImageCount = window_vulkan.GetFramebufferImageCount();
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator = nullptr;
	init_info.CheckVkResultFn = nullptr;
	ImGui_ImplVulkan_Init(&init_info);

	ImGuiIO& io= ImGui::GetIO();

	io.IniFilename= nullptr; // Disable usage of config file.

	io.Fonts->AddFontDefault();

	medium_font_= io.Fonts->AddFontFromFileTTF("fonts/plastic_bag.otf", 20);
	large_font_= io.Fonts->AddFontFromFileTTF("fonts/plastic_bag.otf", 24);
}

ImGuiWrapper::~ImGuiWrapper()
{
	vk_device_.waitIdle();
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext(context_);
}

ImFont* ImGuiWrapper::GetMediumFont() const
{
	return medium_font_;
}

ImFont* ImGuiWrapper::GetLargeFont() const
{
	return large_font_;
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
}

void ImGuiWrapper::EndFrame(const vk::CommandBuffer command_buffer)
{
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
}

} // namespace HexGPU

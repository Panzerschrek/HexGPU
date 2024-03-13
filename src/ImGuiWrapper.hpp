#pragma once
#include "WindowVulkan.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

namespace HexGPU
{

class ImGuiWrapper
{
public:
	ImGuiWrapper(
		SystemWindow& system_window,
		WindowVulkan& window_vulkan,
		vk::DescriptorPool global_descriptor_pool);
	~ImGuiWrapper();

	void ProcessEvents(const std::vector<SDL_Event>& events);

	void BeginFrame();

	// Call this inside main render pass.
	void EndFrame(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;
	ImGuiContext* const context_;
};

} // namespace HexGPU

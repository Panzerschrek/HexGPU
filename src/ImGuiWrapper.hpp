#pragma once
#include "WindowVulkan.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

namespace HexGPU
{

// A helper class for ImGui initialization.
class ImGuiWrapper
{
public:
	ImGuiWrapper(
		SystemWindow& system_window,
		WindowVulkan& window_vulkan);
	~ImGuiWrapper();

	void ProcessEvents(const std::vector<SDL_Event>& events);

	void BeginFrame();

	// Call this inside main render pass.
	void EndFrame(vk::CommandBuffer command_buffer);

private:
	const vk::Device vk_device_;
	const vk::UniqueDescriptorPool descriptor_pool_;
	ImGuiContext* const context_;
};

} // namespace HexGPU

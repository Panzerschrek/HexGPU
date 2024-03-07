#include "WindowVulkan.hpp"
#include "Assert.hpp"
#include "Log.hpp"
#include "SystemWindow.hpp"
#include <SDL_vulkan.h>
#include <algorithm>
#include <cstring>

namespace HexGPU
{

namespace
{

vk::PhysicalDeviceFeatures GetRequiredDeviceFeatures()
{
	// http://vulkan.gpuinfo.org/listfeatures.php

	vk::PhysicalDeviceFeatures features;
	features.multiDrawIndirect= true;
	features.geometryShader= true;
	return features;
}

std::string VulkanVersionToString(const uint32_t version)
{
	return
		std::to_string(version >> 22u) + "." +
		std::to_string((version >> 12u) & ((1u << 10u) - 1u)) + "." +
		std::to_string(version & ((1u << 12u) - 1u));
}

VkBool32 VulkanDebugReportCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT object_type,
	uint64_t  object,
	size_t location,
	int32_t message_code,
	const char* const layer_prefix,
	const char* const message,
	void* user_data)
{
	(void)flags;
	(void)location;
	(void)message_code;
	(void)layer_prefix;
	(void)user_data;

	Log::Warning(
		message, "\n",
		" object= ", object,
		" type= ", vk::to_string(vk::DebugReportObjectTypeEXT(object_type)));

	return VK_FALSE;
}

} // namespace

WindowVulkan::WindowVulkan(const SystemWindow& system_window)
{
	#ifdef DEBUG
	const bool use_debug_extensions_and_layers= true;
	#else
	const bool use_debug_extensions_and_layers= false;
	#endif

	const bool vsync= true;

	// Get vulkan extensiion, needed by SDL.
	unsigned int extension_names_count= 0;
	if( !SDL_Vulkan_GetInstanceExtensions(system_window.GetSDLWindow(), &extension_names_count, nullptr) )
		Log::FatalError("Could not get Vulkan instance extensions");

	std::vector<const char*> extensions_list;
	extensions_list.resize(extension_names_count, nullptr);

	if( !SDL_Vulkan_GetInstanceExtensions(system_window.GetSDLWindow(), &extension_names_count, extensions_list.data()) )
		Log::FatalError("Could not get Vulkan instance extensions");

	if(use_debug_extensions_and_layers)
	{
		extensions_list.push_back("VK_EXT_debug_report");
		++extension_names_count;
	}

	// Create Vulkan instance.
	const vk::ApplicationInfo app_info(
		"HexGPU",
		VK_MAKE_VERSION(0, 0, 1),
		"HexGPU",
		VK_MAKE_VERSION(0, 0, 1),
		VK_MAKE_VERSION(1, 1, 0));

	vk::InstanceCreateInfo instance_create_info(
		vk::InstanceCreateFlags(),
		&app_info,
		0u, nullptr,
		extension_names_count, extensions_list.data());

	if(use_debug_extensions_and_layers)
	{
		const std::vector<vk::LayerProperties> layer_properties= vk::enumerateInstanceLayerProperties();
		static const char* const possible_validation_layers[]
		{
			"VK_LAYER_LUNARG_core_validation",
			"VK_LAYER_KHRONOS_validation",
		};

		for(const char* const& layer_name : possible_validation_layers)
			for(const vk::LayerProperties& property : layer_properties)
				if(std::strcmp(property.layerName, layer_name) == 0)
				{
					instance_create_info.enabledLayerCount= 1u;
					instance_create_info.ppEnabledLayerNames= &layer_name;
					break;
				}
	}

	instance_= vk::createInstanceUnique(instance_create_info);
	Log::Info("Vulkan instance created");

	if(use_debug_extensions_and_layers)
	{
		if(const auto vkCreateDebugReportCallbackEXT=
			PFN_vkCreateDebugReportCallbackEXT(instance_->getProcAddr("vkCreateDebugReportCallbackEXT")))
		{
			const vk::DebugReportCallbackCreateInfoEXT debug_report_callback_create_info(
				vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::eError,
				VulkanDebugReportCallback);

			vkCreateDebugReportCallbackEXT(
				*instance_,
				&static_cast<const VkDebugReportCallbackCreateInfoEXT&>(debug_report_callback_create_info),
				nullptr,
				&debug_report_callback_);
			if(debug_report_callback_ != VK_NULL_HANDLE)
				Log::Info("Vulkan debug callback installed");
		}
	}

	// Create surface.
	VkSurfaceKHR tmp_surface;
	if(!SDL_Vulkan_CreateSurface(system_window.GetSDLWindow(), *instance_, &tmp_surface))
		Log::FatalError("Could not create Vulkan surface");
	surface_= vk::UniqueSurfaceKHR(tmp_surface, vk::ObjectDestroy<vk::Instance>(*instance_));

	SDL_Vulkan_GetDrawableSize(system_window.GetSDLWindow(), reinterpret_cast<int*>(&viewport_size_.width), reinterpret_cast<int*>(&viewport_size_.height));

	// Create physical device. Prefer usage of discrete GPU. TODO - allow user to select device.
	const std::vector<vk::PhysicalDevice> physical_devices= instance_->enumeratePhysicalDevices();
	vk::PhysicalDevice physical_device= physical_devices.front();
	if(physical_devices.size() > 1u)
	{
		for(const vk::PhysicalDevice& physical_device_candidate : physical_devices)
		{
			const vk::PhysicalDeviceProperties properties= physical_device_candidate.getProperties();
			if(properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			{
				physical_device= physical_device_candidate;
				break;
			}
		}
	}

	physical_device_= physical_device;
	memory_properties_= physical_device.getMemoryProperties();

	{
		const vk::PhysicalDeviceProperties properties= physical_device.getProperties();
		Log::Info("");
		Log::Info("Vulkan physical device selected");
		Log::Info("API version: ", VulkanVersionToString(properties.apiVersion));
		Log::Info("Driver version: ", VulkanVersionToString(properties.driverVersion));
		Log::Info("Vendor ID: ", properties.vendorID);
		Log::Info("Device ID: ", properties.deviceID);
		Log::Info("Device type: ", vk::to_string(properties.deviceType));
		Log::Info("Device name: ", properties.deviceName);
		Log::Info("");
	}

	// Select queue family.
	const std::vector<vk::QueueFamilyProperties> queue_family_properties= physical_device.getQueueFamilyProperties();
	uint32_t queue_family_index= ~0u;
	// Use for now the queue with both graphics and compute capabilities - for code simplicity.
	const vk::Flags<vk::QueueFlagBits> required_queue_family_flags= vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
	for(uint32_t i= 0u; i < queue_family_properties.size(); ++i)
	{
		const VkBool32 supported= physical_device.getSurfaceSupportKHR(i, *surface_);
		if(supported != 0 &&
			queue_family_properties[i].queueCount > 0 &&
			(queue_family_properties[i].queueFlags & required_queue_family_flags) == required_queue_family_flags)
		{
			queue_family_index= i;
			break;
		}
	}

	if(queue_family_index == ~0u)
		Log::FatalError("Could not select queue family index");
	else
		Log::Info("Queue familiy index: ", queue_family_index);

	queue_family_index_= queue_family_index;

	const float queue_priority= 1.0f;
	const vk::DeviceQueueCreateInfo device_queue_create_info(
		vk::DeviceQueueCreateFlags(),
		queue_family_index,
		1u, &queue_priority);

	const char* const device_extension_names[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	const vk::PhysicalDeviceFeatures physical_device_features= GetRequiredDeviceFeatures();

	const vk::DeviceCreateInfo device_create_info(
		vk::DeviceCreateFlags(),
		1u, &device_queue_create_info,
		0u, nullptr,
		uint32_t(std::size(device_extension_names)), device_extension_names,
		&physical_device_features);

	// Create physical device.
	// HACK! createDeviceUnique works wrong! Use other method instead.
	//vk_device_= physical_device.createDeviceUnique(vk_device_create_info);
	vk::Device device_tmp;
	if((physical_device.createDevice(&device_create_info, nullptr, &device_tmp)) != vk::Result::eSuccess)
		Log::FatalError("Could not create Vulkan device");
	vk_device_.reset(device_tmp);
	Log::Info("Vulkan logical device created");

	queue_= vk_device_->getQueue(queue_family_index, 0u);

	// Select surface format. Prefer usage of normalized rbga32.
	const std::vector<vk::SurfaceFormatKHR> surface_formats= physical_device.getSurfaceFormatsKHR(*surface_);
	vk::SurfaceFormatKHR surface_format= surface_formats.back();
	for(const vk::SurfaceFormatKHR& surface_format_variant : surface_formats)
	{
		if( surface_format_variant.format == vk::Format::eR8G8B8A8Unorm ||
			surface_format_variant.format == vk::Format::eB8G8R8A8Unorm)
		{
			surface_format= surface_format_variant;
			break;
		}
	}
	Log::Info("Swapchan surface format: ", vk::to_string(surface_format.format), " ", vk::to_string(surface_format.colorSpace));

	// TODO - avoid creating depth buffer here.

	// Select depth buffer format.
	const vk::Format depth_formats[]
	{
		// Depth formats by priority.
		vk::Format::eD32Sfloat,
		vk::Format::eD24UnormS8Uint,
		vk::Format::eX8D24UnormPack32,
		vk::Format::eD32SfloatS8Uint,
		vk::Format::eD16Unorm,
		vk::Format::eD16UnormS8Uint,
	};
	vk::Format framebuffer_depth_format= vk::Format::eD16Unorm;
	for(const vk::Format depth_format_candidate : depth_formats)
	{
		const vk::FormatProperties format_properties=
			physical_device.getFormatProperties(depth_format_candidate);

		const vk::FormatFeatureFlags required_falgs= vk::FormatFeatureFlagBits::eDepthStencilAttachment;
		if((format_properties.optimalTilingFeatures & required_falgs) == required_falgs)
		{
			framebuffer_depth_format= depth_format_candidate;
			break;
		}
	}
	Log::Info("Framebuffer depth format: ", vk::to_string(framebuffer_depth_format));

	// Select present mode. Prefer usage of tripple buffering, than double buffering.
	const std::vector<vk::PresentModeKHR> present_modes= physical_device.getSurfacePresentModesKHR(*surface_);
	vk::PresentModeKHR present_mode= present_modes.front();
	if(vsync)
	{
		if(std::find(present_modes.begin(), present_modes.end(), vk::PresentModeKHR::eFifo) != present_modes.end())
			present_mode= vk::PresentModeKHR::eFifo;
	}
	else
	{
		if(std::find(present_modes.begin(), present_modes.end(), vk::PresentModeKHR::eMailbox) != present_modes.end())
			present_mode= vk::PresentModeKHR::eMailbox;
	}
	Log::Info("Present mode: ", vk::to_string(present_mode));

	const vk::SurfaceCapabilitiesKHR surface_capabilities= physical_device.getSurfaceCapabilitiesKHR(*surface_);

	swapchain_= vk_device_->createSwapchainKHRUnique(
		vk::SwapchainCreateInfoKHR(
			vk::SwapchainCreateFlagsKHR(),
			*surface_,
			surface_capabilities.minImageCount,
			surface_format.format,
			surface_format.colorSpace,
			surface_capabilities.maxImageExtent,
			1u,
			vk::ImageUsageFlagBits::eColorAttachment,
			vk::SharingMode::eExclusive,
			1u, &queue_family_index,
			vk::SurfaceTransformFlagBitsKHR::eIdentity,
			vk::CompositeAlphaFlagBitsKHR::eOpaque,
			present_mode));

	// Create render pass and framebuffers for drawing into screen.

	const vk::AttachmentDescription attachment_descriptions[]
	{
		{
			vk::AttachmentDescriptionFlags(),
			surface_format.format,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR
		},
		{
			vk::AttachmentDescriptionFlags(),
			framebuffer_depth_format,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthStencilAttachmentOptimal, // Actually we do not care about layout after this pass.
		},
	};

	const vk::AttachmentReference attachment_reference_color(0u, vk::ImageLayout::eColorAttachmentOptimal);
	const vk::AttachmentReference attachment_reference_depth(1u, vk::ImageLayout::eGeneral);

	const vk::SubpassDescription subpass_description(
		vk::SubpassDescriptionFlags(),
		vk::PipelineBindPoint::eGraphics,
		0u, nullptr,
		1u, &attachment_reference_color,
		nullptr,
		&attachment_reference_depth);

	render_pass_=
		vk_device_->createRenderPassUnique(
			vk::RenderPassCreateInfo(
				vk::RenderPassCreateFlags(),
				uint32_t(std::size(attachment_descriptions)), attachment_descriptions,
				1u, &subpass_description));

	const std::vector<vk::Image> swapchain_images= vk_device_->getSwapchainImagesKHR(*swapchain_);
	framebuffers_.resize(swapchain_images.size());
	for(size_t i= 0u; i < framebuffers_.size(); ++i)
	{
		{
			framebuffers_[i].depth_image=
				vk_device_->createImageUnique(
					vk::ImageCreateInfo(
						vk::ImageCreateFlags(),
						vk::ImageType::e2D,
						framebuffer_depth_format,
						vk::Extent3D(viewport_size_.width, viewport_size_.height, 1u),
						1u,
						1u,
						vk::SampleCountFlagBits::e1,
						vk::ImageTiling::eOptimal,
						vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
						vk::SharingMode::eExclusive,
						0u, nullptr,
						vk::ImageLayout::eUndefined));

			const vk::MemoryRequirements image_memory_requirements= vk_device_->getImageMemoryRequirements(*framebuffers_[i].depth_image);

			vk::MemoryAllocateInfo memory_allocate_info(image_memory_requirements.size);
			for(uint32_t i= 0u; i < memory_properties_.memoryTypeCount; ++i)
			{
				if((image_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
					(memory_properties_.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags())
				{
					memory_allocate_info.memoryTypeIndex= i;
					break;
				}
			}

			framebuffers_[i].depth_image_memory= vk_device_->allocateMemoryUnique(memory_allocate_info);
			vk_device_->bindImageMemory(*framebuffers_[i].depth_image, *framebuffers_[i].depth_image_memory, 0u);

			framebuffers_[i].depth_image_view=
				vk_device_->createImageViewUnique(
					vk::ImageViewCreateInfo(
						vk::ImageViewCreateFlags(),
						*framebuffers_[i].depth_image,
						vk::ImageViewType::e2D,
						framebuffer_depth_format,
						vk::ComponentMapping(),
						vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0u, 1u, 0u, 1u)));
		}

		framebuffers_[i].image= swapchain_images[i];

		framebuffers_[i].image_view=
			vk_device_->createImageViewUnique(
				vk::ImageViewCreateInfo(
					vk::ImageViewCreateFlags(),
					framebuffers_[i].image,
					vk::ImageViewType::e2D,
					surface_format.format,
					vk::ComponentMapping(),
					vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u)));

		const vk::ImageView attachments[]{ *framebuffers_[i].image_view, *framebuffers_[i].depth_image_view };
		framebuffers_[i].framebuffer=
			vk_device_->createFramebufferUnique(
				vk::FramebufferCreateInfo(
					vk::FramebufferCreateFlags(),
					*render_pass_,
					uint32_t(std::size(attachments)), attachments,
					viewport_size_.width, viewport_size_.height, 1u));

	}

	// Create command pull.
	command_pool_= vk_device_->createCommandPoolUnique(
		vk::CommandPoolCreateInfo(
			vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			queue_family_index));

	// Create command buffers and it's synchronization primitives.
	// Use double buffering for command buffers.
	// 2 is enough - one is executing on the GPU now, the other is prepared by the CPU.
	command_buffers_.resize(2u);
	for(CommandBufferData& frame_data : command_buffers_)
	{
		frame_data.command_buffer=
			std::move(
			vk_device_->allocateCommandBuffersUnique(
				vk::CommandBufferAllocateInfo(
					*command_pool_,
					vk::CommandBufferLevel::ePrimary,
					1u)).front());

		frame_data.image_available_semaphore= vk_device_->createSemaphoreUnique(vk::SemaphoreCreateInfo());
		frame_data.rendering_finished_semaphore= vk_device_->createSemaphoreUnique(vk::SemaphoreCreateInfo());
		frame_data.submit_fence= vk_device_->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
	}
}

WindowVulkan::~WindowVulkan()
{
	Log::Info("Vulkan deinitialization");

	// Sync before destruction.
	vk_device_->waitIdle();

	if(debug_report_callback_ != VK_NULL_HANDLE)
	{
		if(const auto vkDestroyDebugReportCallbackEXT=
			PFN_vkDestroyDebugReportCallbackEXT(instance_->getProcAddr("vkDestroyDebugReportCallbackEXT")))
			vkDestroyDebugReportCallbackEXT(*instance_, debug_report_callback_, nullptr);
	}
}

vk::CommandBuffer WindowVulkan::BeginFrame()
{
	current_frame_command_buffer_= &command_buffers_[frame_count_ % command_buffers_.size()];
	++frame_count_;

	vk_device_->waitForFences(
		1u, &*current_frame_command_buffer_->submit_fence,
		VK_TRUE,
		std::numeric_limits<uint64_t>::max());

	vk_device_->resetFences(1u, &*current_frame_command_buffer_->submit_fence);

	const vk::CommandBuffer command_buffer= *current_frame_command_buffer_->command_buffer;
	command_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	return command_buffer;
}

void WindowVulkan::EndFrame(const DrawFunction& draw_function)
{
	const vk::CommandBuffer command_buffer= *current_frame_command_buffer_->command_buffer;

	// Get next swapchain image.
	const uint32_t swapchain_image_index=
		vk_device_->acquireNextImageKHR(
			*swapchain_,
			std::numeric_limits<uint64_t>::max(),
			*current_frame_command_buffer_->image_available_semaphore,
			vk::Fence()).value;

	draw_function(*framebuffers_[swapchain_image_index].framebuffer);

	// End command buffer.
	command_buffer.end();

	// Submit command buffer.
	const vk::PipelineStageFlags wait_dst_stage_mask= vk::PipelineStageFlagBits::eColorAttachmentOutput;
	const vk::SubmitInfo submit_info(
		1u, &*current_frame_command_buffer_->image_available_semaphore,
		&wait_dst_stage_mask,
		1u, &command_buffer,
		1u, &*current_frame_command_buffer_->rendering_finished_semaphore);
	queue_.submit(submit_info, *current_frame_command_buffer_->submit_fence);

	// Present queue.
	queue_.presentKHR(
		vk::PresentInfoKHR(
			1u, &*current_frame_command_buffer_->rendering_finished_semaphore,
			1u, &*swapchain_,
			&swapchain_image_index,
			nullptr));
}

vk::Device WindowVulkan::GetVulkanDevice() const
{
	return *vk_device_;
}

vk::Extent2D WindowVulkan::GetViewportSize() const
{
	return viewport_size_;
}

uint32_t WindowVulkan::GetQueueFamilyIndex() const
{
	return queue_family_index_;
}

vk::RenderPass WindowVulkan::GetRenderPass() const
{
	return *render_pass_;
}

vk::PhysicalDeviceMemoryProperties WindowVulkan::GetMemoryProperties() const
{
	return memory_properties_;
}

size_t WindowVulkan::GetNumCommandBuffers() const
{
	return command_buffers_.size();
}

} // namespace HexGPU

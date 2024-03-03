#include "BuildPrismRenderer.hpp"
#include "Assert.hpp"
#include "GlobalDescriptorPool.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"


namespace HexGPU
{

namespace
{

struct DrawUniforms
{
	float view_matrix[16]{};
	int32_t build_pos[4]{}; // component 3 - direction.
};

struct BuildPrismVertex
{
	float position[4]; // component 3 - side index.
};

std::vector<BuildPrismVertex> GenBuildPrismMesh()
{
	const BuildPrismVertex hex_vertices[12]
	{
		{ { 1.0f, 0.0f, 0.0f, 0.0f } },
		{ { 3.0f, 0.0f, 0.0f, 0.0f } },
		{ { 4.0f, 1.0f, 0.0f, 0.0f } },
		{ { 0.0f, 1.0f, 0.0f, 0.0f } },
		{ { 3.0f, 2.0f, 0.0f, 0.0f } },
		{ { 1.0f, 2.0f, 0.0f, 0.0f } },

		{ { 1.0f, 0.0f, 1.0f, 0.0f } },
		{ { 3.0f, 0.0f, 1.0f, 0.0f } },
		{ { 4.0f, 1.0f, 1.0f, 0.0f } },
		{ { 0.0f, 1.0f, 1.0f, 0.0f } },
		{ { 3.0f, 2.0f, 1.0f, 0.0f } },
		{ { 1.0f, 2.0f, 1.0f, 0.0f } },
	};

	// Keep same order as in Direction enum!
	// Sides are facing inside.
	std::vector<BuildPrismVertex> res
	{
		// up
		hex_vertices[ 8], hex_vertices[ 7], hex_vertices[ 6],
		hex_vertices[ 9], hex_vertices[ 8], hex_vertices[ 6],
		hex_vertices[11], hex_vertices[10], hex_vertices[ 9],
		hex_vertices[10], hex_vertices[ 8], hex_vertices[ 9],
		// down
		hex_vertices[ 0], hex_vertices[ 1], hex_vertices[ 2],
		hex_vertices[ 0], hex_vertices[ 2], hex_vertices[ 3],
		hex_vertices[ 3], hex_vertices[ 4], hex_vertices[ 5],
		hex_vertices[ 3], hex_vertices[ 2], hex_vertices[ 4],
		// north
		hex_vertices[ 5], hex_vertices[ 4], hex_vertices[10],
		hex_vertices[ 5], hex_vertices[10], hex_vertices[11],
		// south
		hex_vertices[ 1], hex_vertices[ 0], hex_vertices[ 6],
		hex_vertices[ 1], hex_vertices[ 6], hex_vertices[ 7],
		// north-east
		hex_vertices[ 4], hex_vertices[ 2], hex_vertices[ 8],
		hex_vertices[ 4], hex_vertices[ 8], hex_vertices[10],
		// south-east
		hex_vertices[ 2], hex_vertices[ 1], hex_vertices[ 7],
		hex_vertices[ 2], hex_vertices[ 7], hex_vertices[ 8],
		// north-west
		hex_vertices[ 3], hex_vertices[ 5], hex_vertices[ 9],
		hex_vertices[ 5], hex_vertices[11], hex_vertices[ 9],
		// south-west
		hex_vertices[ 0], hex_vertices[ 3], hex_vertices[ 9],
		hex_vertices[ 0], hex_vertices[ 9], hex_vertices[ 6],
	};

	size_t offset= 0;
	for(size_t i= 0; i < 12; ++i)
		res[i].position[3]= float(Direction::Up);
	offset+= 12;

	for(size_t i= 0; i < 12; ++i)
		res[offset + i].position[3]= float(Direction::Down);
	offset+= 12;

	for(size_t i= 0; i < 6; ++i)
		res[offset + i].position[3]= float(Direction::North);
	offset+= 6;

	for(size_t i= 0; i < 6; ++i)
		res[offset + i].position[3]= float(Direction::South);
	offset+= 6;

	for(size_t i= 0; i < 6; ++i)
		res[offset + i].position[3]= float(Direction::NorthEast);
	offset+= 6;

	for(size_t i= 0; i < 6; ++i)
		res[offset + i].position[3]= float(Direction::SouthEast);
	offset+= 6;

	for(size_t i= 0; i < 6; ++i)
		res[offset + i].position[3]= float(Direction::NorthWest);
	offset+= 6;

	for(size_t i= 0; i < 6; ++i)
		res[offset + i].position[3]= float(Direction::SouthWest);
	offset+= 6;

	HEX_ASSERT(offset == res.size());

	return res;
}

GraphicsPipeline CreateBuildPrismPipeline(
	const vk::Device vk_device,
	const vk::Extent2D viewport_size,
	const vk::RenderPass render_pass)
{
	GraphicsPipeline pipeline;

	pipeline.shader_vert= CreateShader(vk_device, ShaderNames::build_prism_vert);
	pipeline.shader_frag= CreateShader(vk_device, ShaderNames::build_prism_frag);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			0u,
			vk::DescriptorType::eUniformBuffer,
			1u,
			vk::ShaderStageFlagBits::eVertex,
		},
	};

	pipeline.descriptor_set_layout=
		vk_device.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	pipeline.pipeline_layout=
		vk_device.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*pipeline.descriptor_set_layout,
				0u, nullptr));

	const vk::PipelineShaderStageCreateInfo shader_stage_create_info[2]
	{
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eVertex,
			*pipeline.shader_vert,
			"main"
		},
		{
			vk::PipelineShaderStageCreateFlags(),
			vk::ShaderStageFlagBits::eFragment,
			*pipeline.shader_frag,
			"main"
		},
	};

	const vk::VertexInputBindingDescription vertex_input_binding_description(
		0u,
		sizeof(BuildPrismVertex),
		vk::VertexInputRate::eVertex);

	const vk::VertexInputAttributeDescription vertex_input_attribute_description[]
	{
		{0u, 0u, vk::Format::eR32G32B32A32Sfloat, 0u},
	};

	const vk::PipelineVertexInputStateCreateInfo pipiline_vertex_input_state_create_info(
		vk::PipelineVertexInputStateCreateFlags(),
		1u, &vertex_input_binding_description,
		uint32_t(std::size(vertex_input_attribute_description)), vertex_input_attribute_description);

	const vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info(
		vk::PipelineInputAssemblyStateCreateFlags(),
		vk::PrimitiveTopology::eTriangleList);

	const vk::Viewport viewport(0.0f, 0.0f, float(viewport_size.width), float(viewport_size.height), 0.0f, 1.0f);
	const vk::Rect2D scissor(vk::Offset2D(0, 0), viewport_size);

	const vk::PipelineViewportStateCreateInfo pipieline_viewport_state_create_info(
		vk::PipelineViewportStateCreateFlags(),
		1u, &viewport,
		1u, &scissor);

	const vk::PipelineRasterizationStateCreateInfo pipilane_rasterization_state_create_info(
		vk::PipelineRasterizationStateCreateFlags(),
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack, // Use back-face culling.
		vk::FrontFace::eCounterClockwise,
		VK_TRUE, -1.0f, 0.0f, -1.0f, // Depth bias
		1.0f);

	const vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info;

	const vk::PipelineDepthStencilStateCreateInfo pipeline_depth_state_create_info(
		vk::PipelineDepthStencilStateCreateFlags(),
		VK_TRUE,
		VK_TRUE,
		vk::CompareOp::eLess,
		VK_FALSE,
		VK_FALSE,
		vk::StencilOpState(),
		vk::StencilOpState(),
		0.0f,
		1.0f);

	// Use simple alpha-blending.
	const vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(
		VK_TRUE,
		vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
		vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

	const vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info(
		vk::PipelineColorBlendStateCreateFlags(),
		VK_FALSE,
		vk::LogicOp::eCopy,
		1u, &pipeline_color_blend_attachment_state);

	pipeline.pipeline=
		UnwrapPipeline(vk_device.createGraphicsPipelineUnique(
			nullptr,
			vk::GraphicsPipelineCreateInfo(
				vk::PipelineCreateFlags(),
				uint32_t(std::size(shader_stage_create_info)),
				shader_stage_create_info,
				&pipiline_vertex_input_state_create_info,
				&pipeline_input_assembly_state_create_info,
				nullptr,
				&pipieline_viewport_state_create_info,
				&pipilane_rasterization_state_create_info,
				&pipeline_multisample_state_create_info,
				&pipeline_depth_state_create_info,
				&pipeline_color_blend_state_create_info,
				nullptr,
				*pipeline.pipeline_layout,
				render_pass,
				0u)));

	return pipeline;
}

} // namespace

BuildPrismRenderer::BuildPrismRenderer(
	WindowVulkan& window_vulkan,
	WorldProcessor& world_processor,
	const vk::DescriptorPool global_descriptor_pool)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, world_processor_(world_processor)
	, uniform_buffer_(
		window_vulkan,
		sizeof(DrawUniforms),
		vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, pipeline_(CreateBuildPrismPipeline(vk_device_, window_vulkan.GetViewportSize(), window_vulkan.GetRenderPass()))
	, descriptor_set_(CreateDescriptorSet(vk_device_, global_descriptor_pool, *pipeline_.descriptor_set_layout))
{
	// Create vertex buffer.
	{
		const auto build_prism_mesh= GenBuildPrismMesh();
		vertex_buffer_num_vertices_= uint32_t(build_prism_mesh.size());

		const size_t vertex_data_size= vertex_buffer_num_vertices_ * sizeof(BuildPrismVertex);

		vertex_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					vertex_data_size,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vertex_buffer_);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
			{
				memory_allocate_info.memoryTypeIndex= i;
				break;
			}
		}

		vertex_buffer_memory_= vk_device_.allocateMemoryUnique(memory_allocate_info);
		vk_device_.bindBufferMemory(*vertex_buffer_, *vertex_buffer_memory_, 0u);

		void* vertex_data_gpu_size= nullptr;
		vk_device_.mapMemory(*vertex_buffer_memory_, 0u, memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &vertex_data_gpu_size);
		std::memcpy(vertex_data_gpu_size, build_prism_mesh.data(), vertex_data_size);
		vk_device_.unmapMemory(*vertex_buffer_memory_);
	}

	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_uniform_buffer_info(
			uniform_buffer_.GetBuffer(),
			0u,
			sizeof(DrawUniforms));

		vk_device_.updateDescriptorSets(
			{
				{
					descriptor_set_,
					0u,
					0u,
					1u,
					vk::DescriptorType::eUniformBuffer,
					nullptr,
					&descriptor_uniform_buffer_info,
					nullptr
				},
			},
			{});
	}
}

BuildPrismRenderer::~BuildPrismRenderer()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void BuildPrismRenderer::PrepareFrame(const vk::CommandBuffer command_buffer)
{
	// Get build position from player state.
	command_buffer.copyBuffer(
		world_processor_.GetPlayerStateBuffer(),
		uniform_buffer_.GetBuffer(),
		{
			{
				offsetof(WorldProcessor::PlayerState, build_pos),
				offsetof(DrawUniforms, build_pos),
				sizeof(int32_t) * 4
			}
		});

	// Copy view matrix.
	command_buffer.copyBuffer(
		world_processor_.GetPlayerStateBuffer(),
		uniform_buffer_.GetBuffer(),
		{
			{
				offsetof(WorldProcessor::PlayerState, blocks_matrix),
				offsetof(DrawUniforms, view_matrix),
				sizeof(float) * 16
			},
		});

	// Add barrier between uniform buffer memory copy and result usage in shader.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
			queue_family_index_, queue_family_index_,
			uniform_buffer_.GetBuffer(),
			0,
			VK_WHOLE_SIZE);

		command_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eVertexShader,
			vk::DependencyFlags(),
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}
}

void BuildPrismRenderer::Draw(const vk::CommandBuffer command_buffer)
{
	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &*vertex_buffer_, &offsets);
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*pipeline_.pipeline_layout,
		0u,
		{descriptor_set_},
		{});

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_.pipeline);

	command_buffer.draw(vertex_buffer_num_vertices_, 1u, 0u, 0u);
}

} // namespace HexGPU

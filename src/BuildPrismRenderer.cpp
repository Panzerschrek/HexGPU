#include "BuildPrismRenderer.hpp"
#include "Assert.hpp"
#include "BlockType.hpp"
#include "ShaderList.hpp"


namespace HexGPU
{

namespace
{

struct Uniforms
{
	int32_t build_pos[4]; // component 3 - direction.
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

} // namespace

BuildPrismRenderer::BuildPrismRenderer(WindowVulkan& window_vulkan, WorldProcessor& world_processor)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, vk_queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, world_processor_(world_processor)
{
	// Create uniform buffer.
	{
		vk_uniform_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					sizeof(Uniforms),
					vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_uniform_buffer_);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags() &&
				// TODO - avoid making host visible.
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_uniform_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_uniform_buffer_, *vk_uniform_buffer_memory_, 0u);

		void* data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_uniform_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &data_gpu_size);
		std::memset(data_gpu_size, 0, sizeof(Uniforms));
		vk_device_.unmapMemory(*vk_uniform_buffer_memory_);
	}

	// Create vertex buffer.
	{
		const auto build_prism_mesh= GenBuildPrismMesh();
		vertex_buffer_num_vertices_= uint32_t(build_prism_mesh.size());

		const size_t vertex_data_size= vertex_buffer_num_vertices_ * sizeof(BuildPrismVertex);

		vk_vertex_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					vertex_data_size,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_vertex_buffer_);

		const auto memory_properties= window_vulkan.GetMemoryProperties();

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_vertex_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_vertex_buffer_, *vk_vertex_buffer_memory_, 0u);

		void* vertex_data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_vertex_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &vertex_data_gpu_size);
		std::memcpy(vertex_data_gpu_size, build_prism_mesh.data(), vertex_data_size);
		vk_device_.unmapMemory(*vk_vertex_buffer_memory_);
	}

	// Create shaders
	shader_vert_= CreateShader(vk_device_, ShaderNames::build_prism_vert);
	shader_frag_= CreateShader(vk_device_, ShaderNames::build_prism_frag);

	// Create descriptor set layout.
	{
		const vk::DescriptorSetLayoutBinding vk_descriptor_set_layout_bindings[]
		{
			{
				0u,
				vk::DescriptorType::eUniformBuffer,
				1u,
				vk::ShaderStageFlagBits::eVertex,
			},
		};
		vk_decriptor_set_layout_=
			vk_device_.createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(
					vk::DescriptorSetLayoutCreateFlags(),
					uint32_t(std::size(vk_descriptor_set_layout_bindings)), vk_descriptor_set_layout_bindings));
	}

	const vk::PushConstantRange vk_push_constant_range(
		vk::ShaderStageFlagBits::eVertex,
		0u,
		sizeof(m_Mat4));

	// Create pipeline layout
	vk_pipeline_layout_=
		vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u,
				&*vk_decriptor_set_layout_,
				1u,
				&vk_push_constant_range));

	// Create pipeline.
	{
		const vk::PipelineShaderStageCreateInfo vk_shader_stage_create_info[2]
		{
			{
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eVertex,
				*shader_vert_,
				"main"
			},
			{
				vk::PipelineShaderStageCreateFlags(),
				vk::ShaderStageFlagBits::eFragment,
				*shader_frag_,
				"main"
			},
		};

		const vk::VertexInputBindingDescription vk_vertex_input_binding_description(
			0u,
			sizeof(BuildPrismVertex),
			vk::VertexInputRate::eVertex);

		const vk::VertexInputAttributeDescription vk_vertex_input_attribute_description[]
		{
			{0u, 0u, vk::Format::eR32G32B32A32Sfloat, 0u},
		};

		const vk::PipelineVertexInputStateCreateInfo vk_pipiline_vertex_input_state_create_info(
			vk::PipelineVertexInputStateCreateFlags(),
			1u, &vk_vertex_input_binding_description,
			uint32_t(std::size(vk_vertex_input_attribute_description)), vk_vertex_input_attribute_description);

		const vk::PipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info(
			vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleList);

		const vk::Extent2D viewport_size= window_vulkan.GetViewportSize();
		const vk::Viewport vk_viewport(0.0f, 0.0f, float(viewport_size.width), float(viewport_size.height), 0.0f, 1.0f);
		const vk::Rect2D vk_scissor(vk::Offset2D(0, 0), viewport_size);

		const vk::PipelineViewportStateCreateInfo vk_pipieline_viewport_state_create_info(
			vk::PipelineViewportStateCreateFlags(),
			1u, &vk_viewport,
			1u, &vk_scissor);

		const vk::PipelineRasterizationStateCreateInfo vk_pipilane_rasterization_state_create_info(
			vk::PipelineRasterizationStateCreateFlags(),
			VK_FALSE,
			VK_FALSE,
			vk::PolygonMode::eFill,
			vk::CullModeFlagBits::eBack, // Use back-face culling.
			vk::FrontFace::eCounterClockwise,
			VK_TRUE, -1.0f, 0.0f, -1.0f, // Depth bias
			1.0f);

		const vk::PipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info;

		const vk::PipelineDepthStencilStateCreateInfo vk_pipeline_depth_state_create_info(
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
		const vk::PipelineColorBlendAttachmentState vk_pipeline_color_blend_attachment_state(
			VK_TRUE,
			vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
			vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

		const vk::PipelineColorBlendStateCreateInfo vk_pipeline_color_blend_state_create_info(
			vk::PipelineColorBlendStateCreateFlags(),
			VK_FALSE,
			vk::LogicOp::eCopy,
			1u, &vk_pipeline_color_blend_attachment_state);

		vk_pipeline_=
			vk_device_.createGraphicsPipelineUnique(
				nullptr,
				vk::GraphicsPipelineCreateInfo(
					vk::PipelineCreateFlags(),
					uint32_t(std::size(vk_shader_stage_create_info)),
					vk_shader_stage_create_info,
					&vk_pipiline_vertex_input_state_create_info,
					&vk_pipeline_input_assembly_state_create_info,
					nullptr,
					&vk_pipieline_viewport_state_create_info,
					&vk_pipilane_rasterization_state_create_info,
					&vk_pipeline_multisample_state_create_info,
					&vk_pipeline_depth_state_create_info,
					&vk_pipeline_color_blend_state_create_info,
					nullptr,
					*vk_pipeline_layout_,
					window_vulkan.GetRenderPass(),
					0u));
	}

	// Create descriptor set pool.
	const vk::DescriptorPoolSize vk_descriptor_pool_size(vk::DescriptorType::eUniformBuffer, 1u);
	vk_descriptor_pool_=
		vk_device_.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				1u, // max sets.
				1u, &vk_descriptor_pool_size));

	// Create descriptor set.
	vk_descriptor_set_=
		std::move(
		vk_device_.allocateDescriptorSetsUnique(
			vk::DescriptorSetAllocateInfo(
				*vk_descriptor_pool_,
				1u, &*vk_decriptor_set_layout_)).front());

	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_uniform_buffer_info(
			*vk_uniform_buffer_,
			0u,
			sizeof(Uniforms));

		vk_device_.updateDescriptorSets(
			{
				{
					*vk_descriptor_set_,
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
	{
		const vk::BufferCopy copy_region(
			offsetof(WorldProcessor::PlayerState, build_pos),
			offsetof(Uniforms, build_pos),
			sizeof(int32_t) * 4);

		command_buffer.copyBuffer(
			world_processor_.GetPlayerStateBuffer(),
			*vk_uniform_buffer_,
			1u, &copy_region);
	}
	// Add barrier between uniform buffer memory copy and result usage in shader.
	{
		const vk::BufferMemoryBarrier barrier(
			vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
			vk_queue_family_index_, vk_queue_family_index_,
			*vk_uniform_buffer_,
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

void BuildPrismRenderer::Draw(const vk::CommandBuffer command_buffer, const m_Mat4& view_matrix)
{
	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &*vk_vertex_buffer_, &offsets);
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*vk_pipeline_layout_,
		0u,
		1u, &*vk_descriptor_set_,
		0u, nullptr);

	const m_Vec3 scale_vec(0.5f / std::sqrt(3.0f), 0.5f, 1.0f );
	m_Mat4 scale_mat;
	scale_mat.Scale(scale_vec);
	const m_Mat4 final_mat= scale_mat * view_matrix;

	command_buffer.pushConstants(*vk_pipeline_layout_, vk::ShaderStageFlagBits::eVertex, 0, sizeof(view_matrix), &final_mat);

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *vk_pipeline_);

	command_buffer.draw(vertex_buffer_num_vertices_, 1u, 0u, 0u);
}

} // namespace HexGPU

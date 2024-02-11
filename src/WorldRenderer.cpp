#include "WorldRenderer.hpp"
#include "Assert.hpp"
#include "ShaderList.hpp"
#include <cmath>
#include <cstring>


namespace HexGPU
{

namespace
{

struct WorldVertex
{
	float pos[4];
	uint8_t color[4];
	uint8_t reserved[12];
};

// Returns indeces for quads with size - maximum uint16_t vertex index.
std::vector<uint16_t> GetQuadsIndices()
{
	size_t quad_count= 65536 / 6 - 1;
	std::vector<uint16_t> indeces( quad_count * 6 );

	for(uint32_t i= 0, v= 0; i< quad_count * 6; i+= 6, v+= 4)
	{
		indeces[i+0]= uint16_t(v + 0);
		indeces[i+1]= uint16_t(v + 1);
		indeces[i+2]= uint16_t(v + 2);
		indeces[i+3]= uint16_t(v + 0);
		indeces[i+4]= uint16_t(v + 2);
		indeces[i+5]= uint16_t(v + 3);
	}

	return indeces;
}

using QuadVertices= std::array<WorldVertex, 4>;

std::vector<QuadVertices> GenQuads()
{
	std::vector<QuadVertices> quads;

	for(uint32_t x= 0; x < 16; ++x)
	for(uint32_t y= 0; y < 16; ++y)
	{
		const float z= 0.0f;

		QuadVertices vertices;

		vertices[0].pos[0]= float(x);
		vertices[0].pos[1]= float(y);
		vertices[0].pos[2]= z;
		vertices[0].color[0]= uint8_t((x + 1u) * 32);
		vertices[0].color[1]= uint8_t((y + 1u) * 32);
		vertices[0].color[2]= 0;

		vertices[1].pos[0]= float(x + 1u);
		vertices[1].pos[1]= float(y);
		vertices[1].pos[2]= z;
		vertices[1].color[0]= uint8_t(x * 32);
		vertices[1].color[1]= uint8_t((y + 1u) * 32);
		vertices[1].color[2]= 0;

		vertices[2].pos[0]= float(x + 1u);
		vertices[2].pos[1]= float(y + 1u);
		vertices[2].pos[2]= z;
		vertices[2].color[0]= uint8_t(x * 32);
		vertices[2].color[1]= uint8_t(y * 32);
		vertices[2].color[2]= 0;

		vertices[3].pos[0]= float(x);
		vertices[3].pos[1]= float(y + 1u);
		vertices[3].pos[2]= z;
		vertices[3].color[0]= uint8_t((x + 1u) * 32);
		vertices[3].color[1]= uint8_t(y * 32);
		vertices[3].color[2]= 0;

		quads.push_back( std::move(vertices) );
	}

	return quads;
}

} // namespace

WorldRenderer::WorldRenderer(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, viewport_size_(window_vulkan.GetViewportSize())
	, vk_render_pass_(window_vulkan.GetRenderPass())
{
	// Create shaders
	shader_vert_= CreateShader(vk_device_, ShaderNames::triangle_vert);
	shader_frag_= CreateShader(vk_device_, ShaderNames::triangle_frag);
	geometry_gen_shader_= CreateShader(vk_device_, ShaderNames::geometry_gen_comp);

	// Create pipeline layout

	const vk::DescriptorSetLayoutBinding vk_descriptor_set_layout_bindings[]
	{
		{
			0u,
			vk::DescriptorType::eUniformBuffer,
			1u,
			vk::ShaderStageFlagBits::eVertex
		},
	};

	vk_decriptor_set_layout_=
		vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(0), vk_descriptor_set_layout_bindings));

	const vk::PushConstantRange vk_push_constant_range(
		vk::ShaderStageFlagBits::eVertex,
		0u,
		sizeof(m_Mat4));

	vk_pipeline_layout_=
		vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u,
				&*vk_decriptor_set_layout_,
				1u,
				&vk_push_constant_range));

	// Create pipeline.

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
		sizeof(WorldVertex),
		vk::VertexInputRate::eVertex);

	const vk::VertexInputAttributeDescription vk_vertex_input_attribute_description[2]
	{
		{0u, 0u, vk::Format::eR32G32B32Sfloat, 0u},
		{1u, 0u, vk::Format::eR8G8B8A8Unorm, sizeof(float) * 4},
	};

	const vk::PipelineVertexInputStateCreateInfo vk_pipiline_vertex_input_state_create_info(
		vk::PipelineVertexInputStateCreateFlags(),
		1u, &vk_vertex_input_binding_description,
		uint32_t(std::size(vk_vertex_input_attribute_description)), vk_vertex_input_attribute_description);

	const vk::PipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info(
		vk::PipelineInputAssemblyStateCreateFlags(),
		vk::PrimitiveTopology::eTriangleList);

	const vk::Viewport vk_viewport(0.0f, 0.0f, float(viewport_size_.width), float(viewport_size_.height), 0.0f, 1.0f);
	const vk::Rect2D vk_scissor(vk::Offset2D(0, 0), viewport_size_);

	const vk::PipelineViewportStateCreateInfo vk_pipieline_viewport_state_create_info(
		vk::PipelineViewportStateCreateFlags(),
		1u, &vk_viewport,
		1u, &vk_scissor);

	const vk::PipelineRasterizationStateCreateInfo vk_pipilane_rasterization_state_create_info(
		vk::PipelineRasterizationStateCreateFlags(),
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eNone,
		vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f,
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

	const vk::PipelineColorBlendAttachmentState vk_pipeline_color_blend_attachment_state(
		VK_FALSE,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
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
				vk_render_pass_,
				0u));

	const auto memory_properties= window_vulkan.GetMemoryProperties();

	// Create vertex buffer

	{
		const std::vector<QuadVertices> world_vertices= GenQuads();
		num_quads_= world_vertices.size();

		const size_t quads_data_size= world_vertices.size() * sizeof(QuadVertices);

		vk_vertex_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					quads_data_size,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_vertex_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_vertex_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_vertex_buffer_, *vk_vertex_buffer_memory_, 0u);

		void* vertex_data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_vertex_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &vertex_data_gpu_size);
		std::memcpy(vertex_data_gpu_size, world_vertices.data(), quads_data_size);
		vk_device_.unmapMemory(*vk_vertex_buffer_memory_);
	}

	{
		const std::vector<uint16_t> world_indeces= GetQuadsIndices();

		const size_t indices_size= world_indeces.size() * sizeof(uint16_t);

		vk_index_buffer_=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					indices_size,
					vk::BufferUsageFlagBits::eIndexBuffer));

		const vk::MemoryRequirements buffer_memory_requirements= vk_device_.getBufferMemoryRequirements(*vk_index_buffer_);

		vk::MemoryAllocateInfo vk_memory_allocate_info(buffer_memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				vk_memory_allocate_info.memoryTypeIndex= i;
		}

		vk_index_buffer_memory_= vk_device_.allocateMemoryUnique(vk_memory_allocate_info);
		vk_device_.bindBufferMemory(*vk_index_buffer_, *vk_index_buffer_memory_, 0u);

		void* vertex_data_gpu_size= nullptr;
		vk_device_.mapMemory(*vk_index_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, vk::MemoryMapFlags(), &vertex_data_gpu_size);
		std::memcpy(vertex_data_gpu_size, world_indeces.data(), indices_size);
		vk_device_.unmapMemory(*vk_index_buffer_memory_);
	}

	// Create descriptor set pool.
	const vk::DescriptorPoolSize vk_descriptor_pool_size(vk::DescriptorType::eCombinedImageSampler, 1u);
	vk_descriptor_pool_=
		vk_device_.createDescriptorPoolUnique(
			vk::DescriptorPoolCreateInfo(
				vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
				4u, // max sets.
				1u, &vk_descriptor_pool_size));

	// Create descriptor set.
	vk_descriptor_set_=
		std::move(
		vk_device_.allocateDescriptorSetsUnique(
			vk::DescriptorSetAllocateInfo(
				*vk_descriptor_pool_,
				1u, &*vk_decriptor_set_layout_)).front());

	// Create compute shader pipeline.
	{
		const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[1]
		{
			{
				0u,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute,
				nullptr,
			},
		};
		vk_geometry_gen_decriptor_set_layout_= vk_device_.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

		vk_geometry_gen_pipeline_layout_= vk_device_.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*vk_geometry_gen_decriptor_set_layout_,
				0u, nullptr));

		vk_geometry_gen_pipeline_= vk_device_.createComputePipelineUnique(
			nullptr,
			vk::ComputePipelineCreateInfo(
				vk::PipelineCreateFlags(),
				vk::PipelineShaderStageCreateInfo(
					vk::PipelineShaderStageCreateFlags(),
					vk::ShaderStageFlagBits::eCompute,
					*geometry_gen_shader_,
					"main"),
				*vk_geometry_gen_pipeline_layout_));

		// Create descriptor set pool.
		const vk::DescriptorSetLayoutBinding vk_descriptor_set_layout_bindings[]
		{
			{
				0u,
				vk::DescriptorType::eStorageBuffer,
				1u,
				vk::ShaderStageFlagBits::eCompute
			},
		};

		vk_geometry_gen_decriptor_set_layout_=
			vk_device_.createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(
					vk::DescriptorSetLayoutCreateFlags(),
					uint32_t(std::size(vk_descriptor_set_layout_bindings)), vk_descriptor_set_layout_bindings));

		const vk::DescriptorPoolSize vk_descriptor_pool_size(vk::DescriptorType::eStorageBuffer, 1u);
		vk_geometry_gen_descriptor_pool_=
			vk_device_.createDescriptorPoolUnique(
				vk::DescriptorPoolCreateInfo(
					vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
					4u, // max sets.
					1u, &vk_descriptor_pool_size));

		vk_geometry_gen_descriptor_set_=
			std::move(
				vk_device_.allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(
						*vk_geometry_gen_descriptor_pool_,
						1u, &*vk_geometry_gen_decriptor_set_layout_)).front());

		const vk::DescriptorBufferInfo descriptor_buffer_info(
			*vk_vertex_buffer_,
			0u,
			num_quads_ * sizeof(QuadVertices));

		vk_device_.updateDescriptorSets(
			{
				{
					*vk_geometry_gen_descriptor_set_,
					0u,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_buffer_info,
					nullptr
				},
			},
			{});
	}
}

WorldRenderer::~WorldRenderer()
{
	// Sync before destruction.
	vk_device_.waitIdle();
}

void WorldRenderer::PrepareFrame(const vk::CommandBuffer command_buffer)
{
	command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *vk_geometry_gen_pipeline_);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*vk_geometry_gen_pipeline_layout_,
		0u,
		1u, &*vk_geometry_gen_descriptor_set_,
		0u, nullptr);

	command_buffer.dispatch(16, 16, 1);

}

void WorldRenderer::Draw(const vk::CommandBuffer command_buffer, const m_Mat4& view_matrix)
{
	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &*vk_vertex_buffer_, &offsets);
	command_buffer.bindIndexBuffer(*vk_index_buffer_, 0u, vk::IndexType::eUint16);
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*vk_pipeline_layout_,
		0u,
		1u, &*vk_descriptor_set_,
		0u, nullptr);

	command_buffer.pushConstants(*vk_pipeline_layout_, vk::ShaderStageFlagBits::eVertex, 0, sizeof(view_matrix), &view_matrix);

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *vk_pipeline_);
	command_buffer.drawIndexed(uint32_t(num_quads_) * 6u, 1u, 0u, 0u, 0u);
}

} // namespace HexGPU

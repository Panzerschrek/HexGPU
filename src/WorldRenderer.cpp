#include "WorldRenderer.hpp"
#include "Assert.hpp"
#include "Constants.hpp"
#include "GlobalDescriptorPool.hpp"
#include "ShaderList.hpp"
#include "VulkanUtils.hpp"
#include <cmath>


namespace HexGPU
{

namespace
{

namespace DrawIndirectBufferBuildShaderBindings
{
	const ShaderBindingIndex chunk_draw_info_buffer= 0;
	const ShaderBindingIndex draw_indirect_buffer= 1;
	const ShaderBindingIndex water_draw_indirect_buffer= 2;
	const ShaderBindingIndex player_state_buffer= 3;
	const ShaderBindingIndex fire_draw_indirect_buffer= 4;
}

namespace DrawShaderBindings
{
	const ShaderBindingIndex uniform_buffer= 0;
	const ShaderBindingIndex sampler= 1;
}

namespace WaterDrawShaderBindings
{
	const ShaderBindingIndex uniform_buffer= 0;
	const ShaderBindingIndex sampler= 1;
}

namespace FireDrawShaderBindings
{
	const ShaderBindingIndex uniform_buffer= 0;
	const ShaderBindingIndex sampler= 1;
}

struct DrawIndirectBufferBuildUniforms
{
	int32_t world_size_chunks[2]{};
	int32_t world_offset_chunks[2]{};
};

struct WorldShaderUniforms
{
	float view_matrix[16]{};
	float sky_light_color[4]{};
};

struct WaterPushConstantsUniforms
{
	float water_phase= 0.0f;
};

Buffer CreateAndFillQuadsIndexBuffer(WindowVulkan& window_vulkan, GPUDataUploader& gpu_data_uploader)
{
	// Each quad contains 4 unique vertices.
	// So, whe have maximum number of quads equal to maximal possible index devided by 4.
	// Add for sequrity extra -1.
	constexpr size_t quad_count= 65536 / 4 - 1;
	uint16_t indices[quad_count * 6];

	for(uint32_t i= 0, v= 0; i< quad_count * 6; i+= 6, v+= 4)
	{
		indices[i+0]= uint16_t(v + 0);
		indices[i+1]= uint16_t(v + 1);
		indices[i+2]= uint16_t(v + 2);
		indices[i+3]= uint16_t(v + 0);
		indices[i+4]= uint16_t(v + 2);
		indices[i+5]= uint16_t(v + 3);
	}

	Buffer buffer(
		window_vulkan,
		sizeof(indices),
		vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);

	gpu_data_uploader.UploadData(static_cast<const void*>(indices), sizeof(indices), buffer.GetBuffer(), 0);

	return buffer;
}

using QuadVertices= std::array<WorldVertex, 4>;

ComputePipeline CreateDrawIndirectBufferBuildPipeline(const vk::Device vk_device)
{
	ComputePipeline pipeline;

	pipeline.shader= CreateShader(vk_device, ShaderNames::world_draw_indirect_buffer_build_comp);

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			DrawIndirectBufferBuildShaderBindings::chunk_draw_info_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			DrawIndirectBufferBuildShaderBindings::draw_indirect_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			DrawIndirectBufferBuildShaderBindings::water_draw_indirect_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			DrawIndirectBufferBuildShaderBindings::player_state_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
		{
			DrawIndirectBufferBuildShaderBindings::fire_draw_indirect_buffer,
			vk::DescriptorType::eStorageBuffer,
			1u,
			vk::ShaderStageFlagBits::eCompute,
			nullptr,
		},
	};

	pipeline.descriptor_set_layout= vk_device.createDescriptorSetLayoutUnique(
		vk::DescriptorSetLayoutCreateInfo(
			vk::DescriptorSetLayoutCreateFlags(),
			uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eCompute,
		0u,
		sizeof(DrawIndirectBufferBuildUniforms));

	pipeline.pipeline_layout= vk_device.createPipelineLayoutUnique(
		vk::PipelineLayoutCreateInfo(
			vk::PipelineLayoutCreateFlags(),
			1u, &*pipeline.descriptor_set_layout,
			1u, &push_constant_range));

	pipeline.pipeline= CreateComputePipeline(vk_device, *pipeline.shader, *pipeline.pipeline_layout);

	return pipeline;
}

} // namespace

WorldRenderer::WorldRenderer(
	WindowVulkan& window_vulkan,
	GPUDataUploader& gpu_data_uploader,
	const WorldProcessor& world_processor,
	const vk::DescriptorPool global_descriptor_pool)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_family_index_(window_vulkan.GetQueueFamilyIndex())
	, world_processor_(world_processor)
	, world_size_(world_processor.GetWorldSize())
	, geometry_generator_(window_vulkan, world_processor, global_descriptor_pool)
	, textures_generator_(window_vulkan, global_descriptor_pool)
	, draw_indirect_buffer_(
		window_vulkan,
		world_size_[0] * world_size_[1] * uint32_t(sizeof(vk::DrawIndexedIndirectCommand)),
		vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer)
	, water_draw_indirect_buffer_(
		window_vulkan,
		world_size_[0] * world_size_[1] * uint32_t(sizeof(vk::DrawIndexedIndirectCommand)),
		vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer)
	, fire_draw_indirect_buffer_(
		window_vulkan,
		world_size_[0] * world_size_[1] * uint32_t(sizeof(vk::DrawIndexedIndirectCommand)),
		vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer)
	, uniform_buffer_(
		window_vulkan,
		sizeof(WorldShaderUniforms),
		vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst)
	, draw_indirect_buffer_build_pipeline_(CreateDrawIndirectBufferBuildPipeline(vk_device_))
	, draw_indirect_buffer_build_descriptor_set_(
		CreateDescriptorSet(
			vk_device_,
			global_descriptor_pool,
			*draw_indirect_buffer_build_pipeline_.descriptor_set_layout))
	, draw_pipeline_(
		CreateWorldDrawPipeline(vk_device_, window_vulkan.GetViewportSize(), window_vulkan.GetRenderPass()))
	, descriptor_set_(CreateDescriptorSet(vk_device_, global_descriptor_pool, *draw_pipeline_.descriptor_set_layout))
	, water_draw_pipeline_(
		CreateWorldWaterDrawPipeline(vk_device_, window_vulkan.GetViewportSize(), window_vulkan.GetRenderPass()))
	, water_descriptor_set_(CreateDescriptorSet(vk_device_, global_descriptor_pool, *water_draw_pipeline_.descriptor_set_layout))
	, fire_draw_pipeline_(CreateFireDrawPipeline(vk_device_, window_vulkan.GetViewportSize(), window_vulkan.GetRenderPass()))
	, fire_descriptor_set_(CreateDescriptorSet(vk_device_, global_descriptor_pool, *fire_draw_pipeline_.descriptor_set_layout))
	, index_buffer_(CreateAndFillQuadsIndexBuffer(window_vulkan, gpu_data_uploader))
{
	// Update descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_chunk_draw_info_buffer_info(
			geometry_generator_.GetChunkDrawInfoBuffer(),
			0u,
			geometry_generator_.GetChunkDrawInfoBufferSize());

		const vk::DescriptorBufferInfo descriptor_draw_indirect_buffer_info(
			draw_indirect_buffer_.GetBuffer(),
			0u,
			draw_indirect_buffer_.GetSize());

		const vk::DescriptorBufferInfo descriptor_water_draw_indirect_buffer_info(
			water_draw_indirect_buffer_.GetBuffer(),
			0u,
			water_draw_indirect_buffer_.GetSize());

		const vk::DescriptorBufferInfo descriptor_player_state_buffer_info(
			world_processor.GetPlayerStateBuffer(),
			0u,
			sizeof(WorldProcessor::PlayerState));

		const vk::DescriptorBufferInfo descriptor_fire_draw_indirect_buffer_info(
			fire_draw_indirect_buffer_.GetBuffer(),
			0u,
			fire_draw_indirect_buffer_.GetSize());

		vk_device_.updateDescriptorSets(
			{
				{
					draw_indirect_buffer_build_descriptor_set_,
					DrawIndirectBufferBuildShaderBindings::chunk_draw_info_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_chunk_draw_info_buffer_info,
					nullptr
				},
				{
					draw_indirect_buffer_build_descriptor_set_,
					DrawIndirectBufferBuildShaderBindings::draw_indirect_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_draw_indirect_buffer_info,
					nullptr
				},
				{
					draw_indirect_buffer_build_descriptor_set_,
					DrawIndirectBufferBuildShaderBindings::water_draw_indirect_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_water_draw_indirect_buffer_info,
					nullptr
				},
				{
					draw_indirect_buffer_build_descriptor_set_,
					DrawIndirectBufferBuildShaderBindings::player_state_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_player_state_buffer_info,
					nullptr
				},
				{
					draw_indirect_buffer_build_descriptor_set_,
					DrawIndirectBufferBuildShaderBindings::fire_draw_indirect_buffer,
					0u,
					1u,
					vk::DescriptorType::eStorageBuffer,
					nullptr,
					&descriptor_fire_draw_indirect_buffer_info,
					nullptr
				},
			},
			{});
	}

	// Update draw descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_uniform_buffer_info(
			uniform_buffer_.GetBuffer(),
			0u,
			sizeof(WorldShaderUniforms));

		const vk::DescriptorImageInfo descriptor_tex_info(
			vk::Sampler(),
			textures_generator_.GetImageView(),
			vk::ImageLayout::eShaderReadOnlyOptimal);

		vk_device_.updateDescriptorSets(
			{
				{
					descriptor_set_,
					DrawShaderBindings::uniform_buffer,
					0u,
					1u,
					vk::DescriptorType::eUniformBuffer,
					nullptr,
					&descriptor_uniform_buffer_info,
					nullptr
				},
				{
					descriptor_set_,
					DrawShaderBindings::sampler,
					0u,
					1u,
					vk::DescriptorType::eCombinedImageSampler,
					&descriptor_tex_info,
					nullptr,
					nullptr
				},
			},
			{});
	}

	// Update water draw descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_uniform_buffer_info(
			uniform_buffer_.GetBuffer(),
			0u,
			sizeof(WorldShaderUniforms));

		const vk::DescriptorImageInfo descriptor_tex_info(
			vk::Sampler(),
			textures_generator_.GetWaterImageView(),
			vk::ImageLayout::eShaderReadOnlyOptimal);

		vk_device_.updateDescriptorSets(
			{
				{
					water_descriptor_set_,
					WaterDrawShaderBindings::uniform_buffer,
					0u,
					1u,
					vk::DescriptorType::eUniformBuffer,
					nullptr,
					&descriptor_uniform_buffer_info,
					nullptr
				},
				{
					water_descriptor_set_,
					WaterDrawShaderBindings::sampler,
					0u,
					1u,
					vk::DescriptorType::eCombinedImageSampler,
					&descriptor_tex_info,
					nullptr,
					nullptr
				},
			},
			{});
	}

	// Update fire draw descriptor set.
	{
		const vk::DescriptorBufferInfo descriptor_uniform_buffer_info(
			uniform_buffer_.GetBuffer(),
			0u,
			sizeof(WorldShaderUniforms));

		const vk::DescriptorImageInfo descriptor_tex_info(
			vk::Sampler(),
			textures_generator_.GetWaterImageView(),
			vk::ImageLayout::eShaderReadOnlyOptimal);

		vk_device_.updateDescriptorSets(
			{
				{
					fire_descriptor_set_,
					FireDrawShaderBindings::uniform_buffer,
					0u,
					1u,
					vk::DescriptorType::eUniformBuffer,
					nullptr,
					&descriptor_uniform_buffer_info,
					nullptr
				},
				{
					fire_descriptor_set_,
					FireDrawShaderBindings::sampler,
					0u,
					1u,
					vk::DescriptorType::eCombinedImageSampler,
					&descriptor_tex_info,
					nullptr,
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

void WorldRenderer::PrepareFrame(TaskOrganizer& task_organizer)
{
	textures_generator_.PrepareFrame(task_organizer);
	geometry_generator_.Update(task_organizer);
	BuildDrawIndirectBuffer(task_organizer);
	CopyViewMatrix(task_organizer);
}

void WorldRenderer::CollectFrameInputs(TaskOrganizer::GraphicsTaskParams& out_task_params)
{
	out_task_params.indirect_draw_buffers.push_back(draw_indirect_buffer_.GetBuffer());
	out_task_params.indirect_draw_buffers.push_back(water_draw_indirect_buffer_.GetBuffer());
	out_task_params.indirect_draw_buffers.push_back(fire_draw_indirect_buffer_.GetBuffer());
	out_task_params.index_buffers.push_back(index_buffer_.GetBuffer());
	out_task_params.vertex_buffers.push_back(geometry_generator_.GetVertexBuffer());
	out_task_params.uniform_buffers.push_back(uniform_buffer_.GetBuffer());
	out_task_params.input_images.push_back(textures_generator_.GetImageInfo());
}

void WorldRenderer::DrawOpaque(const vk::CommandBuffer command_buffer)
{
	const vk::Buffer vertex_buffer= geometry_generator_.GetVertexBuffer();

	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &vertex_buffer, &offsets);
	command_buffer.bindIndexBuffer(index_buffer_.GetBuffer(), 0u, vk::IndexType::eUint16);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*draw_pipeline_.pipeline_layout,
		0u,
		{descriptor_set_},
		{});

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *draw_pipeline_.pipeline);

	command_buffer.drawIndexedIndirect(
		draw_indirect_buffer_.GetBuffer(),
		0,
		world_size_[0] * world_size_[1],
		sizeof(vk::DrawIndexedIndirectCommand));

	{
		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			*fire_draw_pipeline_.pipeline_layout,
			0u,
			{fire_descriptor_set_},
			{});

		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *fire_draw_pipeline_.pipeline);

		command_buffer.drawIndexedIndirect(
			fire_draw_indirect_buffer_.GetBuffer(),
			0,
			world_size_[0] * world_size_[1],
			sizeof(vk::DrawIndexedIndirectCommand));
	}
}

void WorldRenderer::DrawTransparent(const vk::CommandBuffer command_buffer, const float time_s)
{
	const vk::Buffer vertex_buffer= geometry_generator_.GetVertexBuffer();

	const vk::DeviceSize offsets= 0u;
	command_buffer.bindVertexBuffers(0u, 1u, &vertex_buffer, &offsets);
	command_buffer.bindIndexBuffer(index_buffer_.GetBuffer(), 0u, vk::IndexType::eUint16);

	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*water_draw_pipeline_.pipeline_layout,
		0u,
		{water_descriptor_set_},
		{});

	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *water_draw_pipeline_.pipeline);

	WaterPushConstantsUniforms uniforms;
	uniforms.water_phase= time_s;

	command_buffer.pushConstants(
		*water_draw_pipeline_.pipeline_layout,
		vk::ShaderStageFlagBits::eFragment,
		0,
		sizeof(WaterPushConstantsUniforms), static_cast<const void*>(&uniforms));

	command_buffer.drawIndexedIndirect(
		water_draw_indirect_buffer_.GetBuffer(),
		0,
		world_size_[0] * world_size_[1],
		sizeof(vk::DrawIndexedIndirectCommand));
}

WorldRenderer::WorldDrawPipeline WorldRenderer::CreateWorldDrawPipeline(
	const vk::Device vk_device,
	const vk::Extent2D viewport_size,
	const vk::RenderPass render_pass)
{
	WorldDrawPipeline pipeline;

	// Create shaders
	pipeline.shader_vert= CreateShader(vk_device, ShaderNames::world_vert);
	pipeline.shader_frag= CreateShader(vk_device, ShaderNames::world_frag);

	// Create texture sampler
	pipeline.texture_sampler=
		vk_device.createSamplerUnique(
			vk::SamplerCreateInfo(
				vk::SamplerCreateFlags(),
				vk::Filter::eLinear,
				vk::Filter::eLinear,
				vk::SamplerMipmapMode::eLinear,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				0.0f,
				VK_FALSE, // anisotropy
				1.0f, // anisotropy level
				VK_FALSE,
				vk::CompareOp::eNever,
				0.0f,
				100.0f,
				vk::BorderColor::eFloatTransparentBlack,
				VK_FALSE));

	// Create descriptor set layout.
	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			DrawShaderBindings::uniform_buffer,
			vk::DescriptorType::eUniformBuffer,
			1u,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		},
		{
			DrawShaderBindings::sampler,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*pipeline.texture_sampler,
		},
	};

	pipeline.descriptor_set_layout=
		vk_device.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	// Create pipeline layout
	pipeline.pipeline_layout=
		vk_device.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*pipeline.descriptor_set_layout,
				0u, nullptr));

	// Create pipeline.

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
		sizeof(WorldVertex),
		vk::VertexInputRate::eVertex);

	const vk::VertexInputAttributeDescription vertex_input_attribute_description[]
	{
		{0u, 0u, vk::Format::eR16G16B16A16Sscaled, 0u},
		{1u, 0u, vk::Format::eR16G16B16A16Sint, sizeof(int16_t) * 4},
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
		VK_FALSE, 0.0f, 0.0f, 0.0f,
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

	const vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(
		VK_FALSE,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
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

WorldRenderer::WorldDrawPipeline WorldRenderer::CreateWorldWaterDrawPipeline(
	const vk::Device vk_device,
	const vk::Extent2D viewport_size,
	const vk::RenderPass render_pass)
{
	WorldDrawPipeline pipeline;

	pipeline.shader_vert= CreateShader(vk_device, ShaderNames::water_vert);
	pipeline.shader_frag= CreateShader(vk_device, ShaderNames::water_frag);

	pipeline.texture_sampler=
		vk_device.createSamplerUnique(
			vk::SamplerCreateInfo(
				vk::SamplerCreateFlags(),
				vk::Filter::eLinear,
				vk::Filter::eLinear,
				vk::SamplerMipmapMode::eLinear,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				0.0f,
				VK_FALSE, // anisotropy
				1.0f, // anisotropy level
				VK_FALSE,
				vk::CompareOp::eNever,
				0.0f,
				100.0f,
				vk::BorderColor::eFloatTransparentBlack,
				VK_FALSE));

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			WaterDrawShaderBindings::uniform_buffer,
			vk::DescriptorType::eUniformBuffer,
			1u,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		},
		{
			WaterDrawShaderBindings::sampler,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*pipeline.texture_sampler,
		},
	};

	pipeline.descriptor_set_layout=
		vk_device.createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(
				vk::DescriptorSetLayoutCreateFlags(),
				uint32_t(std::size(descriptor_set_layout_bindings)), descriptor_set_layout_bindings));

	const vk::PushConstantRange push_constant_range(
		vk::ShaderStageFlagBits::eFragment,
		0u,
		sizeof(WaterPushConstantsUniforms));

	pipeline.pipeline_layout=
		vk_device.createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(
				vk::PipelineLayoutCreateFlags(),
				1u, &*pipeline.descriptor_set_layout,
				1u, &push_constant_range));

	const vk::PipelineShaderStageCreateInfo shader_stage_create_info[]
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
		sizeof(WorldVertex),
		vk::VertexInputRate::eVertex);

	const vk::VertexInputAttributeDescription vertex_input_attribute_description[]
	{
		{0u, 0u, vk::Format::eR16G16B16A16Sscaled, 0u},
		{1u, 0u, vk::Format::eR16G16B16A16Sint, sizeof(int16_t) * 4},
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
		vk::CullModeFlagBits::eNone, // Use twosided polygons.
		vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f,
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

WorldRenderer::WorldDrawPipeline WorldRenderer::CreateFireDrawPipeline(
	const vk::Device vk_device,
	const vk::Extent2D viewport_size,
	const vk::RenderPass render_pass)
{
	WorldDrawPipeline pipeline;

	pipeline.shader_vert= CreateShader(vk_device, ShaderNames::fire_vert);
	pipeline.shader_frag= CreateShader(vk_device, ShaderNames::fire_frag);

	pipeline.texture_sampler=
		vk_device.createSamplerUnique(
			vk::SamplerCreateInfo(
				vk::SamplerCreateFlags(),
				vk::Filter::eLinear,
				vk::Filter::eLinear,
				vk::SamplerMipmapMode::eLinear,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				vk::SamplerAddressMode::eRepeat,
				0.0f,
				VK_FALSE, // anisotropy
				1.0f, // anisotropy level
				VK_FALSE,
				vk::CompareOp::eNever,
				0.0f,
				100.0f,
				vk::BorderColor::eFloatTransparentBlack,
				VK_FALSE));

	const vk::DescriptorSetLayoutBinding descriptor_set_layout_bindings[]
	{
		{
			FireDrawShaderBindings::uniform_buffer,
			vk::DescriptorType::eUniformBuffer,
			1u,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		},
		{
			FireDrawShaderBindings::sampler,
			vk::DescriptorType::eCombinedImageSampler,
			1u,
			vk::ShaderStageFlagBits::eFragment,
			&*pipeline.texture_sampler,
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

	const vk::PipelineShaderStageCreateInfo shader_stage_create_info[]
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

	// TODO - use special vertex format for fire.
	const vk::VertexInputBindingDescription vertex_input_binding_description(
		0u,
		sizeof(WorldVertex),
		vk::VertexInputRate::eVertex);

	const vk::VertexInputAttributeDescription vertex_input_attribute_description[]
	{
		{0u, 0u, vk::Format::eR16G16B16A16Sscaled, 0u},
		{1u, 0u, vk::Format::eR16G16B16A16Sint, sizeof(int16_t) * 4},
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
		vk::CullModeFlagBits::eNone, // Use twosided polygons.
		vk::FrontFace::eCounterClockwise,
		VK_FALSE, 0.0f, 0.0f, 0.0f,
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

	const vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(
		VK_FALSE,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
		vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
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

void WorldRenderer::CopyViewMatrix(TaskOrganizer& task_organizer)
{
	TaskOrganizer::TransferTaskParams task;
	task.input_buffers.push_back(world_processor_.GetPlayerStateBuffer());
	task.input_buffers.push_back(world_processor_.GetWorldGlobalStateBuffer());
	task.output_buffers.push_back(uniform_buffer_.GetBuffer());

	const auto task_func=
		[this](const vk::CommandBuffer command_buffer)
		{
			// Copy view matrix.
			command_buffer.copyBuffer(
				world_processor_.GetPlayerStateBuffer(),
				uniform_buffer_.GetBuffer(),
				{
					{
						offsetof(WorldProcessor::PlayerState, blocks_matrix),
						offsetof(WorldShaderUniforms, view_matrix),
						sizeof(float) * 16
					}
				});

			// Copy sky light color.
			command_buffer.copyBuffer(
				world_processor_.GetWorldGlobalStateBuffer(),
				uniform_buffer_.GetBuffer(),
				{
					{
						offsetof(WorldProcessor::WorldGlobalState, sky_light_color),
						offsetof(WorldShaderUniforms, sky_light_color),
						sizeof(float) * 4
					}
				});
		};

	task_organizer.ExecuteTask(task, task_func);
}

void WorldRenderer::BuildDrawIndirectBuffer(TaskOrganizer& task_organizer)
{
	TaskOrganizer::ComputeTaskParams task;
	task.input_storage_buffers.push_back(geometry_generator_.GetChunkDrawInfoBuffer());
	task.input_storage_buffers.push_back(world_processor_.GetPlayerStateBuffer());
	task.output_storage_buffers.push_back(draw_indirect_buffer_.GetBuffer());
	task.output_storage_buffers.push_back(water_draw_indirect_buffer_.GetBuffer());
	task.output_storage_buffers.push_back(fire_draw_indirect_buffer_.GetBuffer());

	const auto task_func=
		[this](const vk::CommandBuffer command_buffer)
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *draw_indirect_buffer_build_pipeline_.pipeline);

			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				*draw_indirect_buffer_build_pipeline_.pipeline_layout,
				0u,
				{draw_indirect_buffer_build_descriptor_set_},
				{});

			DrawIndirectBufferBuildUniforms uniforms;
			uniforms.world_size_chunks[0]= int32_t(world_size_[0]);
			uniforms.world_size_chunks[1]= int32_t(world_size_[1]);

			const auto world_offset= world_processor_.GetWorldOffset();
			uniforms.world_offset_chunks[0]= world_offset[0];
			uniforms.world_offset_chunks[1]= world_offset[1];

			command_buffer.pushConstants(
				*draw_indirect_buffer_build_pipeline_.pipeline_layout,
				vk::ShaderStageFlagBits::eCompute,
				0,
				sizeof(DrawIndirectBufferBuildUniforms),
				&uniforms);

			// Dispatch a thread for each chunk.
			command_buffer.dispatch(world_size_[0], world_size_[1], 1);
		};

	task_organizer.ExecuteTask(task, task_func);
}

} // namespace HexGPU

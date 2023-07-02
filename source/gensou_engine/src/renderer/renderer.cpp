#include "renderer/renderer.h"

#include "renderer/device.h"
#include "renderer/geometry/lines.h"
#include "renderer/image.h"
#include "renderer/texture.h"
#include "renderer/pipeline.h"

#include "renderer/swapchain.h"
#include "renderer/validation_layers.h"
#include "renderer/ui_renderer.h"

#include "core/runtime.h"
#include "core/time.h"
#include "core/cmd_queue.h"

#include "scene/components.h"
#include "scene/sprite.h"

#include "core/engine_events.h"
#include <vulkan/vulkan_core.h>

namespace {

	/* texture id sent through push constant */
	struct vertex
	{
		vertex() = default;
		vertex(const glm::vec4& vertexColor) : color(vertexColor) {}

		glm::vec3 position{0.0f, 0.0f, 0.0f};
		glm::vec2 uv{0.0f, 0.0f};
		glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};
}

namespace gs {

	static std::shared_ptr<sprite> s_white_texture;

	static constexpr uint64_t s_frame_vertex_buffer_size = (uint64_t)MiB >> 1ULL; // half MiB

	renderer*	renderer::s_instance				= nullptr;
	uint32_t	renderer::s_blur_downscale_factor	= 1;
	bool		renderer::s_enable_post_process		= true;

	std::future<void> renderer::s_render_complete_future;


	void renderer::init()
	{
		create_pipeline_cache();

		s_instance = new renderer();
		engine_events::viewport_resize.subscribe(&renderer::on_resize);

		ui_renderer::init(s_instance->m_ui_renderpass, 0);
	}

	void renderer::terminate()
	{
		engine_events::terminate_renderer.broadcast();

		if (s_instance)
		{
			delete s_instance;
			s_instance = nullptr;
		}

		s_white_texture.reset();

		ui_renderer::terminate();

		texture::destroy_all_samplers();

		destroy_pipeline_cache();
	}

	void renderer::submit_quad(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float squash, bool mirrorTexture)
	{
		uint32_t texId = s_instance->m_texture_descriptors[rt::current_frame()].get_texture_id(inTexture);
		s_instance->submit_quad_internal(texId, uv, stride, size, color, transform, squash, mirrorTexture);
	}

	void renderer::submit_quad(const glm::vec2& size, const glm::mat4& transform, const glm::vec4& color)
	{
		s_instance->submit_quad_internal(texture_batch_descriptor::get_white_texture_id(), glm::vec2(0.125f), glm::vec2(0.75f), size, color, transform, 1.0f, false);
	}

	void renderer::submit_line(const glm::vec2& edgeRange, const glm::vec3& p1Pos, const glm::vec4& p1Color, const glm::vec3& p2Pos, const glm::vec4& p2Color)
	{
		s_instance->m_lines.submit(edgeRange, p1Pos, p1Color, p2Pos, p2Color);
	}

	void renderer::submit_line_range(const line_vertex* start, size_t count, const glm::vec2& edgeRange)
	{
		s_instance->m_lines.submit_range(start, count, edgeRange);
	}

	void renderer::submit_cube(const glm::vec4& color, const glm::mat4& transform)
	{
		s_instance->m_cubes.submit(color, transform);
	}

	void renderer::override_white_texture(std::shared_ptr<texture> inTexture, const glm::vec2& uv, const glm::vec2& stride)
	{
		s_white_texture->tex = inTexture;
		s_white_texture->uv = uv;
		s_white_texture->stride = stride;
	}

	void renderer::set_clear_value(const glm::vec4& color)
	{
		for(auto& f : s_instance->m_framebuffers)
			f.set_clear_value(0, { color.r, color.g, color.b, color.a});
	}

	renderer::renderer()
		: m_vertex_buffer_capacity(s_frame_vertex_buffer_size * MAX_FRAMES_IN_FLIGHT),
		  m_vertex_buffer(m_vertex_buffer_capacity, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, nullptr, 0),
		  m_index_buffer(2048ULL * sizeof(uint16_t) * 6ULL, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, nullptr, 0),
		  m_camera_ubo(sizeof(glm::mat4) * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, nullptr, 0),
		  m_vertices((uint64_t)MiB >> 4ULL),
		  m_pre_render_cmds((uint64_t)MiB >> 2ULL)
	{
		BENCHMARK("renderer constructor")
		
		m_working_draw_calls.reserve(32ULL);

		for(auto& drawCall : m_draw_calls)
			drawCall.reserve(32ULL);

		std::vector<uint16_t> indices(2048ULL * 6ULL);
		for (uint16_t i = 0, offset = 0; i < indices.size(); i += 6, offset += 4)
		{
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;
		}

		m_index_buffer.write(indices.data(), indices.size() * sizeof(uint16_t), 0ULL);

		auto colorFormat = VK_FORMAT_R8G8B8A8_SRGB;
		auto depthFormat = device::get_depth_format(16, false);

		/* ---------------- RENDERPASSES ---------------- */
		{
			VkSubpassDescription subpass{};
			VkSubpassDependency dependencies[2] = { VkSubpassDependency{} };
			VkAttachmentDescription attachments[2] = { VkAttachmentDescription{} };

			attachments[0].flags = 0x0;
			attachments[0].format = colorFormat;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; /* needs to be stored to be loaded in the ui pass */
			attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			attachments[1].flags = 0x0;
			attachments[1].format = depthFormat;
			attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; /* transient attachment */
			attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference colorReference{};
			colorReference.attachment = 0;
			colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference depthReference{};
			depthReference.attachment = 1;
			depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorReference;
			subpass.pDepthStencilAttachment = &depthReference;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = 0;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			/* self-dependency for the layout change */
			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassCreateInfo{};
			renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassCreateInfo.attachmentCount = 2;
			renderPassCreateInfo.pAttachments = attachments;
			renderPassCreateInfo.subpassCount = 1;
			renderPassCreateInfo.pSubpasses = &subpass;
			renderPassCreateInfo.dependencyCount = 2;
			renderPassCreateInfo.pDependencies = dependencies;

			/* scene pass */
			{
				VkResult result = vkCreateRenderPass(device::get_logical(), &renderPassCreateInfo, nullptr, &m_renderpass);
				if (result != VK_SUCCESS)
					engine_events::vulkan_result_error.broadcast(result, "Could not create scene renderpass");
			}

			/* ui pass */
			{
				attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; /* we ll render an overlayer, should not clear */
				attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; /* needs to be stored to be sampled in the screen pass */
				attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; /* needs to be the exact layout in other to avoid undefined behaviour */
				attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; /* we'll sampled it during the screen pass */
				subpass.pDepthStencilAttachment = nullptr;
				renderPassCreateInfo.attachmentCount = 1;

				VkResult result = vkCreateRenderPass(device::get_logical(), &renderPassCreateInfo, nullptr, &m_ui_renderpass);
				if (result != VK_SUCCESS)
					engine_events::vulkan_result_error.broadcast(result, "Could not create ui renderpass");
			}

		} /* renderpasses */

		/* ---------------- FRAMEBUFFERS ---------------- */
		{
			VkImageUsageFlags colorUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			VkImageUsageFlags depthUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			if(device::supports_lazy_allocation())
				depthUsage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

			for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				if(colorFormat == VK_FORMAT_R8G8B8A8_UNORM)
					LOG_ENGINE(warn, "framebuffer[%u].colorFormat == VK_FORMAT_R8G8B8A8_UNORM", i);
					
				m_framebuffers[i].set_attachment(0, colorUsage, colorFormat, rt::viewport());
				m_framebuffers[i].set_attachment(1, depthUsage, depthFormat, rt::viewport());
				m_ui_framebuffers[i].set_attachment(0, m_framebuffers[i].get_attachment(0));

				m_framebuffers[i].set_clear_value(0, { 0.0f, 0.0f, 0.0f, 1.0f });
				m_framebuffers[i].set_clear_value(1, { 1.0f, 0.0f });

				m_framebuffers[i].set_clear_value_count(2);
				m_ui_framebuffers[i].set_clear_value_count(1);

				m_framebuffers[i].create(m_renderpass);
				m_ui_framebuffers[i].create(m_ui_renderpass);
			}
		} /* framebuffers */

		/* internal sampler */
		m_sampler = texture::get_sampler(sampler_filter::linear, sampler_wrap::mirror);

		/* descriptor sets */
		{
			VkDescriptorBufferInfo cameraDescriptorInfo{};
			cameraDescriptorInfo.buffer = m_camera_ubo.get();
			cameraDescriptorInfo.range = sizeof(glm::mat4);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.sampler = m_sampler;

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				cameraDescriptorInfo.offset = cameraDescriptorInfo.range * i;

				m_camera_descriptors[i].create(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
				m_camera_descriptors[i].update(0, &cameraDescriptorInfo, 1, 0);

				imageInfo.imageView = m_framebuffers[i].get_attachment(0)->get_image_view();
				m_screen_texture_descriptors[i].create(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
				m_screen_texture_descriptors[i].update(0, &imageInfo, 1, 0);
			}
		} /* descriptors */
	
		/*  pipelines */
		{
			/* main scene pipeline */
			{
				m_texture_pipeline = std::make_shared<graphics_pipeline>();

				/* always recompile shaders in debug mode */
				#if defined(APP_DEBUG) && !defined(APP_ANDROID)
				m_texture_pipeline->push_shader_src("quad.vert.glsl", true);
				m_texture_pipeline->push_shader_src("quad.frag.glsl", true);
				#else
				m_texture_pipeline->push_shader_spv("engine_res/shaders/spir-v/quad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
				m_texture_pipeline->push_shader_spv("engine_res/shaders/spir-v/quad.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
				#endif

				graphics_pipeline_properties pipelineProperties{};
				pipelineProperties.depthTest = true;
				pipelineProperties.width = runtime::viewport().width;
				pipelineProperties.height = runtime::viewport().height;
				pipelineProperties.culling = INVERT_VIEWPORT ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT;
				pipelineProperties.blending = true;
				pipelineProperties.renderPass = m_renderpass;
				pipelineProperties.subpassIndex = 0;

				/* vertex input info */
				{
					static constexpr VkVertexInputAttributeDescription vertexDescription[] =
					{
						{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, position) },
						{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vertex, uv) },
						{ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, color) },
					};

					static constexpr VkVertexInputBindingDescription vertexBindingDescription{ 0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX };

					static constexpr VkPipelineVertexInputStateCreateInfo quadVertexInputState
					{
						VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
						nullptr, 0x0,
						1,&vertexBindingDescription,
						3, vertexDescription
					};

					pipelineProperties.vertexInputInfo = quadVertexInputState;
				}

				auto cameraLayout = m_camera_descriptors[0].get_layout();
				auto textureLayout = m_texture_descriptors[0].get_descriptor().get_layout();

				VkPushConstantRange pushRange{};
				pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; //VK_SHADER_STAGE_VERTEX_BIT ;
				pushRange.offset = 0ULL;
				pushRange.size = sizeof(uint32_t);

				m_texture_pipeline->create_pipeline_layout({ cameraLayout, textureLayout }, { pushRange });
				m_texture_pipeline->create_pipeline(pipelineProperties);
			}

			/* line pipeline */
			{
				m_line_pipeline = std::make_shared<graphics_pipeline>();

				#if defined(APP_DEBUG) && !defined(APP_ANDROID)
				m_line_pipeline->push_shader_src("line.vert.glsl", true);
				m_line_pipeline->push_shader_src("line.frag.glsl", true);
				#else
				m_line_pipeline->push_shader_spv("engine_res/shaders/spir-v/line.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
				m_line_pipeline->push_shader_spv("engine_res/shaders/spir-v/line.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
				#endif

				graphics_pipeline_properties pipelineProperties{};
				pipelineProperties.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
				pipelineProperties.depthTest = true;
				pipelineProperties.width = runtime::viewport().width;
				pipelineProperties.height = runtime::viewport().height;
				pipelineProperties.culling = VK_CULL_MODE_NONE;
				pipelineProperties.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
				pipelineProperties.blending = true;
				pipelineProperties.renderPass = m_renderpass;
				pipelineProperties.subpassIndex = 0;
				pipelineProperties.vertexInputInfo = line_geometry::get_state_input_info();
				
				auto cameraLayout = m_camera_descriptors[0].get_layout();

				VkPushConstantRange pushRange{};
				pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; //VK_SHADER_STAGE_VERTEX_BIT ;
				pushRange.offset = 0ULL;
				pushRange.size = sizeof(glm::vec2);

				m_line_pipeline->create_pipeline_layout({ cameraLayout }, { pushRange });
				m_line_pipeline->create_pipeline(pipelineProperties);
			}

			/* cube pipeline */
			{
				m_cube_pipeline = std::make_shared<graphics_pipeline>();

				#if defined(APP_DEBUG) && !defined(APP_ANDROID)
				m_cube_pipeline->push_shader_src("cube.vert.glsl", true);
				m_cube_pipeline->push_shader_src("cube.frag.glsl", true);
				#else
				m_cube_pipeline->push_shader_spv("engine_res/shaders/spir-v/cube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
				m_cube_pipeline->push_shader_spv("engine_res/shaders/spir-v/cube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
				#endif

				graphics_pipeline_properties pipelineProperties{};
				pipelineProperties.depthTest = true;
				pipelineProperties.width = runtime::viewport().width;
				pipelineProperties.height = runtime::viewport().height;
				pipelineProperties.culling = VK_CULL_MODE_NONE; //VK_FRONT_FACE_CLOCKWISE
				pipelineProperties.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //VK_FRONT_FACE_COUNTER_CLOCKWISE
				pipelineProperties.blending = true;
				pipelineProperties.renderPass = m_renderpass;
				pipelineProperties.subpassIndex = 0;
				pipelineProperties.vertexInputInfo = cube_geometry::get_state_input_info();
				
				auto cameraLayout = m_camera_descriptors[0].get_layout();

				m_cube_pipeline->create_pipeline_layout({ cameraLayout });
				m_cube_pipeline->create_pipeline(pipelineProperties);
			}

		} /* pipelines */

		/* blur data */
		{
			VkFormat format = device::get_storage_image_format();
			extent2d blurExtent = { rt::viewport().width / s_blur_downscale_factor, rt::viewport().height / s_blur_downscale_factor };

			/* for layout transition */
			image2d::image_info layoutTransitionInfo[MAX_FRAMES_IN_FLIGHT] = { image2d::image_info{} };

			for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				auto image_0 = std::make_shared<image2d>(VkImageUsageFlags(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT), blurExtent, format);
				auto image_1 = std::make_shared<image2d>(VkImageUsageFlags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT), blurExtent, format);

				m_blur_images[i * 2] = image_0;
				m_blur_images[i * 2 + 1] = image_1;

				auto& horizontalDescriptor = m_blur_descriptors[i * 2];
				auto& verticalDescriptor = m_blur_descriptors[i * 2 + 1];

				horizontalDescriptor.create({
					{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
					{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }
				});

				verticalDescriptor.create({
					{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
					{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }
				});

				/* for sampling (read) */
				VkDescriptorImageInfo samplingImageInfo[2];
				samplingImageInfo[0] = { m_sampler, image_0->get_image_view(), VK_IMAGE_LAYOUT_GENERAL };
				samplingImageInfo[1] = { m_sampler, image_1->get_image_view(), VK_IMAGE_LAYOUT_GENERAL };

				/* for storage (write) */
				VkDescriptorImageInfo storageImageInfo[2];
				storageImageInfo[0] = { VK_NULL_HANDLE, image_0->get_image_view(), VK_IMAGE_LAYOUT_GENERAL };
				storageImageInfo[1] = { VK_NULL_HANDLE, image_1->get_image_view(), VK_IMAGE_LAYOUT_GENERAL };

				horizontalDescriptor.update(0, &storageImageInfo[0], 1, 0);
				horizontalDescriptor.update(1, &samplingImageInfo[1], 1, 0);
				verticalDescriptor.update(0, &storageImageInfo[1], 1, 0);
				verticalDescriptor.update(1, &samplingImageInfo[0], 1, 0);

				/* even index images will be the ones worked on and will always have layout_general */
				layoutTransitionInfo[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				layoutTransitionInfo[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
				layoutTransitionInfo[i].srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // 0x0
				layoutTransitionInfo[i].dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				layoutTransitionInfo[i].srcAccess = VK_ACCESS_MEMORY_READ_BIT; //0x0
				layoutTransitionInfo[i].dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
				layoutTransitionInfo[i].image = m_blur_images[i * 2]->get_image();
				layoutTransitionInfo[i].extent = m_blur_images[i * 2]->get_extent();

				m_blur_textures[i] = texture::create(m_blur_images[i * 2 + 1]);
			}
			/* TODO: try false for waitForFences */
			image2d::transition_layout(layoutTransitionInfo, MAX_FRAMES_IN_FLIGHT, true);

			m_blur_pipeline = std::make_shared<compute_pipeline>();

			#if defined(APP_DEBUG) && !defined(APP_ANDROID)
			m_blur_pipeline->push_shader_src("gaussian_blur.comp.glsl", true);
			#else
			m_blur_pipeline->push_shader_spv("engine_res/shaders/spir-v/gaussian_blur.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
			#endif

			VkPushConstantRange pushConstantRange{};
			pushConstantRange.offset = 0;
			pushConstantRange.size = sizeof(blur_push_constant);
			pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			m_blur_pipeline->create_pipeline_layout({ m_blur_descriptors[0].get_layout() }, { pushConstantRange });
			m_blur_pipeline->create_pipeline(8, 8, 1);
		}

		LOG_ENGINE(trace, "finished renderer constructor");
	}

	renderer::~renderer()
	{
		LOG_ENGINE(trace, "renderer destructor called");

		if (m_renderpass)
			vkDestroyRenderPass(device::get_logical(), m_renderpass, nullptr);

		if (m_ui_renderpass)
			vkDestroyRenderPass(device::get_logical(), m_ui_renderpass, nullptr);
	}

	void renderer::submit_quad_internal(uint32_t textureId, const glm::vec2& uv, const glm::vec2& stride, const glm::vec2& size, const glm::vec4& color, const glm::mat4& transform, float squash, bool mirrorTexture)
	{
		const float right	= size.x / 2;
		const float left	= -right;
		const float up		= size.y / 2;
		const float down	= -up * squash;

		const glm::mat4 baseQuad =
		{
			left,	down,	0.0f, 1.0f,
			right,	down,	0.0f, 1.0f,
			right,	up,		0.0f, 1.0f,
			left,	up,		0.0f, 1.0f
		};

		glm::mat4 position = transform * baseQuad;

		uint32_t offset = m_quad_count * sizeof(vertex) * 4ull;

		auto newColor = glm::vec4(revert_gamma_correction(glm::vec3(color)), color.a);

		auto vertex0 = m_vertices.emplace<vertex>(offset + sizeof(vertex) * 0ull, newColor);
		auto vertex1 = m_vertices.emplace<vertex>(offset + sizeof(vertex) * 1ull, newColor);
		auto vertex2 = m_vertices.emplace<vertex>(offset + sizeof(vertex) * 2ull, newColor);
		auto vertex3 = m_vertices.emplace<vertex>(offset + sizeof(vertex) * 3ull, newColor);

		vertex0->position = glm::vec3(position[0]);	// top left
		vertex1->position = glm::vec3(position[1]);	// bottom right
		vertex2->position = glm::vec3(position[2]);	// top right
		vertex3->position = glm::vec3(position[3]);	// bottom left

		const float uvX = mirrorTexture ? 1.0f - uv.x : uv.x;

		vertex0->uv = { uvX,			uv.y			};	// top left 
		vertex1->uv = { uvX + stride.x,	uv.y,			};	// bottom right
		vertex2->uv = { uvX + stride.x,	uv.y + stride.y	};	// top right
		vertex3->uv = { uvX,			uv.y + stride.y	};	// bottom left

		if (m_working_draw_calls.empty())
		{
			m_working_draw_calls.emplace_back(std::make_pair(1ul, textureId));
		}
		else if(textureId == m_working_draw_calls.back().second)
		{
			m_working_draw_calls.back().first++;
		}
		else
		{
			m_working_draw_calls.emplace_back(std::make_pair(1ul, textureId));
		}

		m_quad_count++;
	}

	void renderer::render_internal(std::shared_ptr<swapchain> sc)
	{
		assert(sc);

		/*----------------prepare render data-------------------------------------*/
		size_t offsetIntoBuffer = s_frame_vertex_buffer_size * runtime::current_frame();
		size_t vertexDataSize = m_quad_count * sizeof(vertex) * 4ULL;
		size_t uiVertexOffset = offsetIntoBuffer + vertexDataSize;

		bool hasUi = false;
		bool hasBlur = false;

		if(ui_renderer::quad_count())
		{
			hasBlur = ui_renderer::using_blur();
			hasUi = true;
		}

		command_manager::reset_general_pools();

		auto frame = runtime::current_frame();
		auto quads = m_quad_count;
		auto blurArea = ui_renderer::blur_area();
		auto& drawCalls = m_draw_calls[frame];
		auto& uiDrawCalls = ui_renderer::get_draw_calls(frame);

		/*-------wait before touching render resources--------------------*/
		wait_render_cmds();
		m_draw_calls[frame] = m_working_draw_calls;
		auto& lineDrawCalls = m_lines.get_draw_calls(frame);

		/* can only be executed after wait_for_fences of this frame has returned */
		{
			m_pre_render_cmds.dequeue_all();

			if(quads)
				m_vertex_buffer.write(m_vertices.data(), vertexDataSize, offsetIntoBuffer);

			if(hasUi)
				m_vertex_buffer.write(ui_renderer::get_vertices(), ui_renderer::get_vertices_size(), uiVertexOffset);

			m_lines.start_frame();
			m_cubes.start_frame();
		}

		system::submit_render_cmd(frame,
		[this, frame, sc, hasUi, hasBlur,
					uiVertexOffset, blurArea, &drawCalls, &uiDrawCalls, &lineDrawCalls, 
						quads, lines = m_lines.count, cubes = m_cubes.count]() mutable
		{
			BENCHMARK("RENDERER | submit_render_cmd");

			auto fbSize = m_framebuffers[frame].get_extent();
			VkRect2D rect{};
			rect.extent = { fbSize.width, fbSize.height };
			rect.offset = { 0, 0 };

			VkViewport viewport{ 0.0f, 0.0f, (float)fbSize.width, (float)fbSize.height, 0.0f, 1.0f };

			if (INVERT_VIEWPORT)
			{
				viewport.y = (float)fbSize.height;
				viewport.height = -((float)fbSize.height);
			}

			auto cmd = command_manager::get_render_cmd_buffer(frame);

			VkCommandBufferBeginInfo commandBufferBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0x0, nullptr };
			vkBeginCommandBuffer(cmd, &commandBufferBeginInfo);

			VkRenderPassBeginInfo renderPassBeginInfo{};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = m_renderpass;
			renderPassBeginInfo.framebuffer = m_framebuffers[frame].get();
			renderPassBeginInfo.renderArea = rect;
			renderPassBeginInfo.clearValueCount = m_framebuffers[frame].get_clear_value_count();
			renderPassBeginInfo.pClearValues = m_framebuffers[frame].get_clear_value_data();

			/* main scene renderpass */
			{
				vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdSetViewport(cmd, 0, 1, &viewport);
				vkCmdSetScissor(cmd, 0, 1, &rect);

				if(lines)
				{
					VkBuffer vertexBuffers[] = { m_lines.vertex_buffer.get() };
					/* start at the current frame offset */
					uint64_t vertexOffsets[] = { 0 };
					uint64_t instanceOffsets[] = { m_lines.current_offset };

					vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, vertexOffsets);

					vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_line_pipeline->get());

					VkDescriptorSet cameraSets[] = { m_camera_descriptors[frame].get() };
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_line_pipeline->get_layout(), 0, 1, cameraSets, 0, nullptr);

					int32_t lineOffset = 0;
					for (auto [lineCount, edge] : lineDrawCalls)
					{
						vkCmdPushConstants(cmd, m_line_pipeline->get_layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec2), &edge);
						vkCmdDraw(cmd, lineCount * 2ULL, 1, lineOffset * 2L, 0);

						lineOffset += lineCount;
					}
				}

				if (cubes)
				{
					VkBuffer vertexBuffers[] = { m_cubes.vertex_buffer.get() };
					VkBuffer instanceBuffers[] = { m_cubes.instance_buffer.get() };
					/* start at the current frame offset */
					uint64_t vertexOffsets[] = { 0 };
					uint64_t instanceOffsets[] = { m_cubes.current_offset };

					vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, vertexOffsets);
					vkCmdBindVertexBuffers(cmd, 1, 1, instanceBuffers, instanceOffsets);

					vkCmdBindIndexBuffer(cmd, m_cubes.index_buffer.get(), 0, VK_INDEX_TYPE_UINT16);

					vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cube_pipeline->get());

					VkDescriptorSet cameraSets[] = { m_camera_descriptors[frame].get() };
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cube_pipeline->get_layout(), 0, 1, cameraSets, 0, nullptr);

					vkCmdDrawIndexed(cmd, cube_geometry::indices_count(), cubes, 0, 0, 0);					
				}

				if (quads)
				{
					VkBuffer buffers[] = { m_vertex_buffer.get() };
					/* start at the current frame offset */
					uint64_t bufferOffsets[] = { s_frame_vertex_buffer_size * frame };

					vkCmdBindVertexBuffers(cmd, 0, 1, buffers, bufferOffsets);
					vkCmdBindIndexBuffer(cmd, m_index_buffer.get(), 0, VK_INDEX_TYPE_UINT16);

					vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_texture_pipeline->get());

					VkDescriptorSet cameraSets[] = { m_camera_descriptors[frame].get() };
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_texture_pipeline->get_layout(), 0, 1, cameraSets, 0, nullptr);

					VkDescriptorSet textureSets[] = { m_texture_descriptors[frame].get_descriptor().get() };
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_texture_pipeline->get_layout(), 1, 1, textureSets, 0, nullptr);

					int32_t quadOffset = 0;
					for (auto [quadCount, texIndex] : drawCalls)
					{
						vkCmdPushConstants(cmd, m_texture_pipeline->get_layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &texIndex);
						vkCmdDrawIndexed(cmd, quadCount * 6UL, 1, 0, quadOffset * 4L, 0);
						quadOffset += quadCount;
					}
				}

				vkCmdEndRenderPass(cmd);
			}

			/* post-process (compute shader) */
			if (s_enable_post_process && hasBlur)
			{
				BENCHMARK("COMPUTE | blur");
				blur(cmd, frame, m_framebuffers[frame].get_attachment(0), blurArea);
			}

			/* ui pass */
			if (hasUi)
			{
				renderPassBeginInfo.renderPass = m_ui_renderpass;
				renderPassBeginInfo.framebuffer = m_ui_framebuffers[frame].get();
				renderPassBeginInfo.clearValueCount = m_ui_framebuffers[frame].get_clear_value_count();
				renderPassBeginInfo.pClearValues = m_ui_framebuffers[frame].get_clear_value_data();

				vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdSetViewport(cmd, 0, 1, &viewport);
				vkCmdSetScissor(cmd, 0, 1, &rect);

				VkBuffer buffers[] = { m_vertex_buffer.get() };
				uint64_t offsets[] = { uiVertexOffset };

				vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
				vkCmdBindIndexBuffer(cmd, m_index_buffer.get(), 0, VK_INDEX_TYPE_UINT16);

				auto uiPipeline = ui_renderer::get_pipeline();
				auto& cameraDescriptor = ui_renderer::get_camera_descriptor_set();
				auto& textureDescriptor = ui_renderer::get_texture_descriptor_set(frame);

				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline->get());

				VkDescriptorSet cameraSets[] = { cameraDescriptor.get() };
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline->get_layout(), 0, 1, cameraSets, 0, nullptr);

				VkDescriptorSet textureSets[] = { textureDescriptor.get() };
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipeline->get_layout(), 1, 1, textureSets, 0, nullptr);

				int32_t indexOffset = 0;
				for (auto [quadCount, texIndex] : uiDrawCalls)
				{
					vkCmdPushConstants(cmd, uiPipeline->get_layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &texIndex);
					vkCmdDrawIndexed(cmd, quadCount * 6UL, 1, 0, indexOffset * 4L, 0);
					indexOffset += quadCount;
				}

				vkCmdEndRenderPass(cmd);
			}

			/* screen pass (swapchain target) */
			{
				auto imgIndex = sc->acquire_next_image();

				const auto& fb = sc->get_framebuffer(imgIndex);
				auto swapchainPipeline = sc->get_pipeline();
				extent2d swapchainSize = sc->get_image_extent();

				renderPassBeginInfo.renderPass = sc->get_renderpass();
				renderPassBeginInfo.framebuffer = fb.get();
				renderPassBeginInfo.clearValueCount = fb.get_clear_value_count();
				renderPassBeginInfo.pClearValues = fb.get_clear_value_data();

				rect.extent = { swapchainSize.width, swapchainSize.height };

				vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdSetViewport(cmd, 0, 1, &viewport);
				vkCmdSetScissor(cmd, 0, 1, &rect);

				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, swapchainPipeline->get());

				VkDescriptorSet screenTextureDescriptors[] = { m_screen_texture_descriptors[frame].get() };
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, swapchainPipeline->get_layout(), 0, 1, screenTextureDescriptors, 0, nullptr);

				vkCmdDraw(cmd, 3, 1, 0, 0);
				vkCmdEndRenderPass(cmd);
			}

			/* record */
			{
				BENCHMARK("vkEndCommandBuffer");
				VkResult recordCmdResult = vkEndCommandBuffer(cmd);
				INTERNAL_ASSERT_VKRESULT(recordCmdResult, "failed to record command buffer");
			}

			sc->present(frame);
		});

		s_render_complete_future = system::execute_render_cmds(frame);
		auto nextFrame = runtime::next_frame();

		/* clear working resources for the next frame */
		m_working_draw_calls.clear();
		m_vertices.reset();
		m_quad_count = 0;

		m_lines.end_frame();
		m_cubes.end_frame();
		ui_renderer::end_frame(nextFrame);
	}

	void renderer::on_resize_internal(uint32_t x, uint32_t y)
	{
		LOG_ENGINE(trace, "renderer::on_resize_internal");

		reset_render_cmds_internal(false);

		for (size_t i = 0; i < m_framebuffers.size(); i++)
		{
			m_framebuffers[i].resize(x, y);
			m_ui_framebuffers[i].resize(x, y);

			m_framebuffers[i].set_clear_value_count(2);
			m_ui_framebuffers[i].set_clear_value_count(1);
		}

		LOG_ENGINE(trace, "resized framebuffers");

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.sampler = m_sampler;

		for (uint32_t i = 0; i < m_screen_texture_descriptors.size(); i++)
		{
			imageInfo.imageView = m_framebuffers[i].get_attachment(0)->get_image_view();
			m_screen_texture_descriptors[i].update(0, &imageInfo, 1, 0);
		}

		image2d::image_info layoutTransitionInfo[MAX_FRAMES_IN_FLIGHT] = { image2d::image_info{} };

		/* blur data */
		for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			auto image_0 = m_blur_images[i * 2];
			auto image_1 = m_blur_images[i * 2 + 1];

			image_0->resize(x / s_blur_downscale_factor, y / s_blur_downscale_factor);
			image_1->resize(x / s_blur_downscale_factor, y / s_blur_downscale_factor);

			auto& horizontalDescriptor = m_blur_descriptors[i * 2];
			auto& verticalDescriptor = m_blur_descriptors[i * 2 + 1];

			/* for sampling (read) */
			VkDescriptorImageInfo samplingImageInfo[2];
			samplingImageInfo[0] = { m_sampler, image_0->get_image_view(), VK_IMAGE_LAYOUT_GENERAL };
			samplingImageInfo[1] = { m_sampler, image_1->get_image_view(), VK_IMAGE_LAYOUT_GENERAL };

			/* for storage (write) */
			VkDescriptorImageInfo storageImageInfo[2];
			storageImageInfo[0] = { VK_NULL_HANDLE, image_0->get_image_view(), VK_IMAGE_LAYOUT_GENERAL };
			storageImageInfo[1] = { VK_NULL_HANDLE, image_1->get_image_view(), VK_IMAGE_LAYOUT_GENERAL };

			horizontalDescriptor.update(0, &storageImageInfo[0], 1, 0);
			horizontalDescriptor.update(1, &samplingImageInfo[1], 1, 0);
			verticalDescriptor.update(0, &storageImageInfo[1], 1, 0);
			verticalDescriptor.update(1, &samplingImageInfo[0], 1, 0);

			/* even index images will be the ones worked on and will always have layout_general */
			layoutTransitionInfo[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			layoutTransitionInfo[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
			layoutTransitionInfo[i].srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			layoutTransitionInfo[i].dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			layoutTransitionInfo[i].srcAccess = VK_ACCESS_MEMORY_READ_BIT;
			layoutTransitionInfo[i].dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
			layoutTransitionInfo[i].image = m_blur_images[i * 2]->get_image();
			layoutTransitionInfo[i].extent = m_blur_images[i * 2]->get_extent();

			m_blur_textures[i]->set_image(m_blur_images[i * 2 + 1]);
		}
		image2d::transition_layout(layoutTransitionInfo, MAX_FRAMES_IN_FLIGHT);

		LOG_ENGINE(trace, "finished renderer resize");
	}

	void renderer::blur(VkCommandBuffer cmd, uint32_t frame, std::shared_ptr<image2d> attachment, quad_area blurArea, uint32_t blurCount)
	{
		if (blurArea.size_x + blurArea.size_y == 0)
			return;

		/* add 10 pixels in each direction to account for neighbouring pixels */
		{
			blurArea.x = std::max(blurArea.x - (10.0f * s_blur_downscale_factor), 0.0f);
			blurArea.y = std::max(blurArea.y - (10.0f * s_blur_downscale_factor), 0.0f);

			blurArea.size_x = std::min(blurArea.size_x + (20.0f * s_blur_downscale_factor), (float)attachment->get_width());
			blurArea.size_y = std::min(blurArea.size_y + (20.0f * s_blur_downscale_factor), (float)attachment->get_height());
		}

		/* blitt fragment shader output onto image_1 which will be blured then sampled in the next render pass
		 * image_0 will serve as the write image for the horizontal pass, which will in turn be sampled on the vertical pass
		 */

		auto image_0 = m_blur_images[frame * 2];
		auto image_1 = m_blur_images[frame * 2 + 1];

		auto& horizontalDescriptor = m_blur_descriptors[frame * 2];
		auto& verticalDescriptor = m_blur_descriptors[frame * 2 + 1];

		/* blitt */
		{
			image2d::image_info dstImage{}, srcImage{};
			dstImage.image = image_1->get_image();
			dstImage.extent = image_1->get_extent();
			dstImage.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; /* undefined because we don't care for image_1's current content as it will be overridden */
			dstImage.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			dstImage.srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dstImage.dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			srcImage.image = attachment->get_image();
			srcImage.extent = attachment->get_extent();

			image2d::copy_image(cmd, srcImage, dstImage);
		}

		VkDescriptorSet descriptorSets[] = { horizontalDescriptor.get(), verticalDescriptor.get() };
		uint32_t offsets[] = { 0 };

		/* calculate work group sizes */
		uint32_t workGroupX, workGroupY;
		{
			uint32_t localSizeX = m_blur_pipeline->local_size_x();
			uint32_t localSizeY = m_blur_pipeline->local_size_y();

			uint32_t blurX = (uint32_t)blurArea.size_x / s_blur_downscale_factor;
			uint32_t blurY = (uint32_t)blurArea.size_y / s_blur_downscale_factor;

			uint32_t imageX = attachment->get_width();
			uint32_t imageY = attachment->get_height();

			workGroupX = (imageX / localSizeX) + /*leftover*/std::min(imageX % localSizeX, 1u);
			workGroupY = (imageY / localSizeY) + /*leftover*/std::min(imageY % localSizeY, 1u);

			workGroupX = std::min(blurX / localSizeX + std::min(blurX % localSizeX, 1u), workGroupX);
			workGroupY = std::min(blurY / localSizeY + std::min(blurY % localSizeY, 1u), workGroupY);
		}

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.pNext = nullptr;
		barrier.image = image_0->get_image();
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseMipLevel = 0;

		barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		blur_push_constant pushData{};
		pushData.x_offset = blurArea.x / s_blur_downscale_factor;
		pushData.y_offset = blurArea.y / s_blur_downscale_factor;
		pushData.horizontal_pass = 0UL;

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline->get());

		/* dispatch compute pipeline twice (one for the horizontal pass and another for the vertical one) */
		/* we'll ignore the blur pass count, as even a single pass is already very expensive on mobile */
		{
			/* HORIZONTAL PASS [sample from image1, write on image_0] */
			pushData.horizontal_pass = 1;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline->get_layout(), 0, 1, &descriptorSets[0], 0, offsets);
			vkCmdPushConstants(cmd, m_blur_pipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(blur_push_constant), &pushData);
			vkCmdDispatch(cmd, workGroupX, workGroupY, 1);

			barrier.image = image_0->get_image();
			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			/* VERTICAL PASS [sample from image0, write on image_1] */
			pushData.horizontal_pass = 0;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_blur_pipeline->get_layout(), 0, 1, &descriptorSets[1], 0, offsets);
			vkCmdPushConstants(cmd, m_blur_pipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(blur_push_constant), &pushData);
			vkCmdDispatch(cmd, workGroupX, workGroupY, 1);

			barrier.image = image_1->get_image();
			barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);
		}
	}

	void renderer::reset_render_cmds_internal(bool resetWhiteTexture)
	{
		for(auto& drawCall : m_draw_calls)
			drawCall.clear();

		m_vertices.reset();
		m_quad_count = 0;

		for (auto& texDescriptor : m_texture_descriptors)
			texDescriptor.clear();

		ui_renderer::reset_cmds(resetWhiteTexture);
	}

	void renderer::update_view_projection(const glm::mat4& viewProjection, uint32_t frame)
	{
		submit_pre_render_cmd([viewProjection, frame]()
		{
			size_t offset = sizeof(glm::mat4) * frame;
			s_instance->m_camera_ubo.write(&viewProjection, sizeof(glm::mat4), offset);
		});
	}

	uint32_t renderer::get_total_quad_count()
	{
		return s_instance->m_quad_count + ui_renderer::quad_count();
	}
}
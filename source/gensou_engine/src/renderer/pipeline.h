#pragma once

#include "core/core.h"

#include <vulkan/vulkan.h>

namespace gs {

	//-------------------------------------------------------------------------------
	// BASE PIPELINE
	//-------------------------------------------------------------------------------

	extern void create_pipeline_cache();
	extern void destroy_pipeline_cache();

	class base_pipeline
	{
	public:
		virtual ~base_pipeline();

		/* size in bytes */
		void push_shader(const dword* pSourceCode, size_t size, VkShaderStageFlagBits stage);
		void push_shader(const std::vector<dword>& sourceCode, VkShaderStageFlagBits stage);
		void push_shader_spv(const std::string& shaderPath, VkShaderStageFlagBits stage);
		void push_shader_src(const std::string& path, bool recompile = false);

		void create_pipeline_layout(std::initializer_list<VkDescriptorSetLayout> layouts = {}, std::initializer_list<VkPushConstantRange> ranges = {});

		VkPipeline get() const { return m_pipeline; }
		VkPipelineLayout get_layout() const { return m_pipeline_layout; }

	protected:
		base_pipeline();

		VkPipeline m_pipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;

		std::unordered_map<VkShaderStageFlagBits, VkShaderModule> m_shader_modules;
	};

	//-------------------------------------------------------------------------------
	// COMPUTE PIPELINE
	//-------------------------------------------------------------------------------

	class compute_pipeline : public base_pipeline
	{
	public:
		void create_pipeline(uint32_t localSizeX, uint32_t localSizeY, uint32_t localSizeZ);

		uint32_t local_size_x() const { return m_local_size_x; }
		uint32_t local_size_y() const { return m_local_size_y; }
		uint32_t local_size_z() const { return m_local_size_z; }

	private:
		uint32_t m_local_size_x = 1, m_local_size_y = 1, m_local_size_z = 1;
	};

	//-------------------------------------------------------------------------------
	// GRAPHICS PIPELINE
	//-------------------------------------------------------------------------------

	struct graphics_pipeline_properties
	{
		uint32_t width = 0, height = 0;
		float lineWidth = 1.0f;
		VkCullModeFlagBits culling = VK_CULL_MODE_FRONT_BIT;
		VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // VK_FRONT_FACE_CLOCKWISE
		VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		uint32_t subpassIndex = 0;

		VkPipelineVertexInputStateCreateInfo vertexInputInfo
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			nullptr, 0x0,
			0, nullptr,
			0, nullptr
		};

		VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		bool depthTest = true, stencilTest = false, depthWriteEnable = true;
		bool wireFrame = false, dynamicViewport = true, blending = true;
		bool multiSample = false, sampleShading = false, alphaToCoverageEnable = false;
		float minSampleShading = 1.0f;
	};

	class graphics_pipeline : public base_pipeline
	{
	public:
		void create_pipeline(const graphics_pipeline_properties& properties, VkPipelineColorBlendAttachmentState* attachmentBlending = nullptr, uint32_t attachmentBlendingCount = 0);

	private:
		graphics_pipeline_properties m_properties{};
	};

}
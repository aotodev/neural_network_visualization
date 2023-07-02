#include "renderer/pipeline.h"
#include "renderer/device.h"
#include "renderer/validation_layers.h"

#include "core/log.h"
#include "core/system.h"
#include "core/runtime.h"
#include "core/engine_events.h"
#include "core/misc.h"

namespace {

	struct pipeline_cache_gensou_header
	{
		uint32_t size;				/* *vkGetPipelineCacheData.pDataSize */
		dword hash;					/* hash of pipeline cache data, including the header */
		uint32_t driver_version;	/* VkPhysicalDeviceProperties::driverVersion */
	};

}

namespace gs {

	static VkPipelineCache s_pipeline_cache = VK_NULL_HANDLE;

	static void serialize_pipeline_cache()
	{
		byte* writeData = nullptr;
		size_t writeSize = 0;

		byte* cacheData = nullptr;
		size_t cacheSize = 0;

		vkGetPipelineCacheData(device::get_logical(), s_pipeline_cache, &cacheSize, nullptr);
		writeData = new byte[sizeof(pipeline_cache_gensou_header) + cacheSize];
		cacheData = writeData + sizeof(pipeline_cache_gensou_header);

		vkGetPipelineCacheData(device::get_logical(), s_pipeline_cache, &cacheSize, cacheData);
		writeSize = cacheSize + sizeof(pipeline_cache_gensou_header);

		auto gsHeader = (pipeline_cache_gensou_header*)(writeData);
		gsHeader->size = cacheSize;
		gsHeader->hash = get_hashcode_from_binary(cacheData, cacheSize);
		gsHeader->driver_version = device::driver_version();

		dword pipelineHash = gsHeader->hash;

		std::string outPath(system::make_path_from_internal_data("pipeline_cache"));
		std::ofstream fout(outPath, std::ios::binary);

		fout.write(reinterpret_cast<char*>(writeData), writeSize);

		if (writeData)
			delete[] writeData;

		LOG_ENGINE(trace, "serialized pipeline cache with hash %u in path '%s'", pipelineHash, outPath.c_str());
	}

	/* makes sure the data is complete and compatible */
	/* this step is particularly important on mobile platforms and a must on Android */
	/* if any of the following checks fail we should create a new pipeline cache from scratch */
	static bool check_pipeline_cache(const byte* data)
	{
		const byte* tempData = data;
		auto gsHeader = (pipeline_cache_gensou_header*)tempData;
		auto vulkanHeader = (VkPipelineCacheHeaderVersionOne*)(tempData + sizeof(pipeline_cache_gensou_header));

		if (vulkanHeader->headerSize != 32)
		{
			LOG_ENGINE(warn, "pipeline cache invalid, invalid header");
			return false;
		}

		if (vulkanHeader->headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
		{
			LOG_ENGINE(warn, "pipeline cache invalid, invalid vulkan header version");
			return false;
		}

		if(vulkanHeader->vendorID != device::vendor_id())
		{
			LOG_ENGINE(warn, "pipeline cache invalid, invalid vendor id");
			return false;
		}

		if(vulkanHeader->deviceID != device::device_id())
		{
			LOG_ENGINE(warn, "pipeline cache invalid, invalid device id");
			return false;
		}

		if (memcmp(vulkanHeader->pipelineCacheUUID, device::pipeline_cache_uuid(), VK_UUID_SIZE * sizeof(byte)) != 0)
		{
			LOG_ENGINE(warn, "pipeline cache invalid, invalid pipeline cache UUID");
			return false;
		}

		if(gsHeader->driver_version != device::driver_version())
		{
			LOG_ENGINE(warn, "pipeline cache invalid, not same driver version");
			return false;
		}

		/* check data integrity */
		auto pipelineHash = get_hashcode_from_binary((byte*)vulkanHeader, gsHeader->size);
		if(gsHeader->hash != pipelineHash)
		{
			LOG_ENGINE(warn, "pipeline cache invalid, hash code not the same (loaded = %u, generated = %u). data probably corrupt", gsHeader->hash, pipelineHash);
			return false;
		}

		LOG_ENGINE(info, "pipeline cache found and valid");
		return true;
	}

	void create_pipeline_cache()
	{
		VkPipelineCacheCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0x0;
		createInfo.initialDataSize = 0;
		createInfo.pInitialData = nullptr;

		auto rawData = system::load_internal_file("pipeline_cache");
	
		if(!rawData.empty() && check_pipeline_cache(rawData.data()))
		{
			createInfo.initialDataSize = ((pipeline_cache_gensou_header*)rawData.data())->size;
			createInfo.pInitialData = rawData.data() + sizeof(pipeline_cache_gensou_header);

			LOG_ENGINE(trace, "reusing pipeline cache loaded from memory");
		}

		auto result = vkCreatePipelineCache(device::get_logical(), &createInfo, nullptr, &s_pipeline_cache);
		if (result != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(result, "failed to create pipeline cache");
	}

	void destroy_pipeline_cache()
	{
		if(s_pipeline_cache != VK_NULL_HANDLE)
		{
			serialize_pipeline_cache();
			vkDestroyPipelineCache(device::get_logical(), s_pipeline_cache, nullptr);
			LOG_ENGINE(trace, "destroyed pipeline cache");
		}
	}

	//-------------------------------------------------------------------------------
	// BASE PIPELINE
	//-------------------------------------------------------------------------------

	base_pipeline::base_pipeline()
	{
		m_shader_modules.insert({ VK_SHADER_STAGE_VERTEX_BIT, VK_NULL_HANDLE });
		m_shader_modules.insert({ VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE });
		m_shader_modules.insert({ VK_SHADER_STAGE_COMPUTE_BIT, VK_NULL_HANDLE });
	}

	base_pipeline::~base_pipeline()
	{
		if (m_pipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(device::get_logical(), m_pipeline, nullptr);

		for (auto [stage, shader] : m_shader_modules)
		{
			if (shader != VK_NULL_HANDLE)
				vkDestroyShaderModule(device::get_logical(), shader, nullptr);
		}

		if (m_pipeline_layout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(device::get_logical(), m_pipeline_layout, nullptr);
	}

	void base_pipeline::push_shader(const dword* pSourceCode, size_t size, VkShaderStageFlagBits stage)
	{
		auto& shader = m_shader_modules.at(stage);
		if (shader != VK_NULL_HANDLE)
			vkDestroyShaderModule(device::get_logical(), shader, nullptr);

		VkShaderModuleCreateInfo createShaderModuleInfo{};
		createShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createShaderModuleInfo.codeSize = size;
		createShaderModuleInfo.pCode = pSourceCode;

		VkResult shaderModuleResult = vkCreateShaderModule(device::get_logical(), &createShaderModuleInfo, nullptr, &shader);
		if (shaderModuleResult != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(shaderModuleResult, "Could not create Shader Module");

		LOG_ENGINE(trace, "Created Shader Module");
	}

	void base_pipeline::push_shader(const std::vector<dword>& sourceCode, VkShaderStageFlagBits stage)
	{
		push_shader(sourceCode.data(), sourceCode.size() * sizeof(uint32_t), stage);
	}

	void base_pipeline::push_shader_spv(const std::string& shaderPath, VkShaderStageFlagBits stage)
	{
		auto shader = system::load_spv_file(shaderPath);

		if (shader.empty())
		{
			LOG_ENGINE(error, "could not open %s.spv shader file", shaderPath.c_str());
		}
		else
		{
			push_shader(reinterpret_cast<const dword*>(shader.data()), shader.size() * sizeof(byte), stage);
		}
	}

	void base_pipeline::push_shader_src(const std::string& shaderName, bool recompile /*= false*/)
	{
		std::string absPath(SHADERS_DIR);

		std::string localName, type;
		std::stringstream stream(shaderName), finalName;
		std::getline(stream, localName, '.');
		std::getline(stream, type, '.');

		VkShaderStageFlagBits stage;

		if (type == "vert")
		{
			stage = VK_SHADER_STAGE_VERTEX_BIT;
		}
		else if (type == "frag")
		{
			stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		else if (type == "comp")
		{
			stage = VK_SHADER_STAGE_COMPUTE_BIT;
		}
		else
		{
			LOG_ENGINE(error, "shader type missing from shader name. It should be '.vert' or '.frag' or '.comp', instead was '%s'", type.c_str());
			return;
		}

		finalName << localName << "." << type << ".spv";
		std::string spvPath(absPath + std::string("/spir-v/") + finalName.str());

		if (!std::filesystem::exists(spvPath) || recompile)
		{
			std::stringstream compileCmd;

			#ifdef VULKAN_GLSL_1_2
			/* --target-env vulkan1.2 to generate SPIR-V 1.3 or higher */
			compileCmd << "glslangValidator --target-env vulkan1.2 " << absPath << "/src/" << shaderName << " -o " << spvPath;
			#else
			compileCmd << "glslangValidator -V " << absPath << "/src/" << shaderName << " -o " << spvPath;
			#endif

			std::system(compileCmd.str().c_str());

			/* optimize */
			if (std::filesystem::exists(spvPath))
			{
				std::stringstream optimizeCmd;
				optimizeCmd << "spirv-opt " << spvPath << " -o " << spvPath;
				std::system(optimizeCmd.str().c_str());
			}
			else
			{
				LOG_ENGINE(error, "Failed to compile shader %s", shaderName.c_str());
				return;
			}

			LOG_ENGINE(trace, "compiled shader from source");
		}

		push_shader_spv(spvPath, stage);
	}

	void base_pipeline::create_pipeline_layout(std::initializer_list<VkDescriptorSetLayout> layouts, std::initializer_list<VkPushConstantRange> ranges)
	{
		std::vector<VkDescriptorSetLayout> descriptorLayouts(layouts);
		std::vector<VkPushConstantRange> pushConstants(ranges);

		VkPipelineLayoutCreateInfo createLayoutInfo = {};
		createLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		createLayoutInfo.setLayoutCount = (uint32_t)descriptorLayouts.size();
		createLayoutInfo.pSetLayouts = descriptorLayouts.data();
		createLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstants.size();
		createLayoutInfo.pPushConstantRanges = pushConstants.data();

		VkResult creatPipelineLayoutResult = vkCreatePipelineLayout(device::get_logical(), &createLayoutInfo, nullptr, &m_pipeline_layout);
		if (creatPipelineLayoutResult != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(creatPipelineLayoutResult, "Could not create Pipeline Layout");
	}

	//-------------------------------------------------------------------------------
	// COMPUTE PIPELINE
	//-------------------------------------------------------------------------------

	void compute_pipeline::create_pipeline(uint32_t localSizeX, uint32_t localSizeY, uint32_t localSizeZ)
	{
		m_local_size_x = localSizeX;
		m_local_size_y = localSizeY;
		m_local_size_z = localSizeZ;

		auto shader = m_shader_modules.at(VK_SHADER_STAGE_COMPUTE_BIT);
		assert(shader != VK_NULL_HANDLE);

		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.module = shader;
		shaderStageInfo.pName = "main";
		shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;

		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = m_pipeline_layout;
		computePipelineCreateInfo.flags = 0;
		computePipelineCreateInfo.stage = shaderStageInfo;

		VkResult createPipelineResult = vkCreateComputePipelines(device::get_logical(), s_pipeline_cache, 1, &computePipelineCreateInfo, nullptr, &m_pipeline);
		if (createPipelineResult != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(createPipelineResult, "failed to create compute pipeline");
	}

	//-------------------------------------------------------------------------------
	// GRAPHICS PIPELINE
	//-------------------------------------------------------------------------------

	void graphics_pipeline::create_pipeline(const graphics_pipeline_properties& properties, VkPipelineColorBlendAttachmentState* attachmentBlending, uint32_t attachmentBlendingCount)
	{
		m_properties = properties;
		assert(m_properties.renderPass != VK_NULL_HANDLE);

		auto vertexShader = m_shader_modules.at(VK_SHADER_STAGE_VERTEX_BIT);
		auto fragmentShader = m_shader_modules.at(VK_SHADER_STAGE_FRAGMENT_BIT);

		assert(vertexShader);
		assert(fragmentShader);

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

		pipelineInfo.pVertexInputState = &m_properties.vertexInputInfo;

		VkPipelineShaderStageCreateInfo shaderStageInfos[2] = { VkPipelineShaderStageCreateInfo{} };
		shaderStageInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfos[0].module = vertexShader;
		shaderStageInfos[0].pName = "main";
		shaderStageInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

		shaderStageInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfos[1].module = fragmentShader;
		shaderStageInfos[1].pName = "main";
		shaderStageInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStageInfos;

		pipelineInfo.layout = m_pipeline_layout;
		pipelineInfo.renderPass = m_properties.renderPass;
		pipelineInfo.subpass = m_properties.subpassIndex;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyInfo.topology = m_properties.topology;
		inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;

		VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
		rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizerInfo.depthClampEnable = VK_FALSE;
		rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizerInfo.polygonMode = m_properties.wireFrame ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
		rasterizerInfo.lineWidth = m_properties.lineWidth;
		rasterizerInfo.cullMode = m_properties.culling;
		rasterizerInfo.frontFace = m_properties.frontFace;
		rasterizerInfo.depthBiasEnable = VK_FALSE;
		rasterizerInfo.depthBiasConstantFactor = 0.0f;
		rasterizerInfo.depthBiasClamp = 0.0f;
		rasterizerInfo.depthBiasSlopeFactor = 0.0f;
		pipelineInfo.pRasterizationState = &rasterizerInfo;

		VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
		multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisamplingInfo.sampleShadingEnable = m_properties.sampleShading;
		multisamplingInfo.rasterizationSamples = m_properties.multiSample ? (VkSampleCountFlagBits)runtime::multisample_count() : VK_SAMPLE_COUNT_1_BIT;
		multisamplingInfo.minSampleShading = m_properties.minSampleShading; 
		multisamplingInfo.pSampleMask = nullptr;
		multisamplingInfo.alphaToCoverageEnable = m_properties.alphaToCoverageEnable ? VK_TRUE : VK_FALSE;
		multisamplingInfo.alphaToOneEnable = VK_FALSE;
		pipelineInfo.pMultisampleState = &multisamplingInfo;

		VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo{};
		depthStencilStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilStateInfo.depthTestEnable = VK_TRUE;
		depthStencilStateInfo.depthWriteEnable = m_properties.depthWriteEnable ? VK_TRUE : VK_FALSE;
		depthStencilStateInfo.depthCompareOp = m_properties.depthCompareOp;
		depthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilStateInfo.minDepthBounds = 0.0f;
		depthStencilStateInfo.maxDepthBounds = 1.0f;
		depthStencilStateInfo.stencilTestEnable = m_properties.stencilTest ? VK_TRUE : VK_FALSE;
		depthStencilStateInfo.front.failOp = VK_STENCIL_OP_KEEP;
		depthStencilStateInfo.front.passOp = VK_STENCIL_OP_KEEP;
		depthStencilStateInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depthStencilStateInfo.back = depthStencilStateInfo.front;
		pipelineInfo.pDepthStencilState = m_properties.depthTest ? &depthStencilStateInfo : nullptr;

		VkPipelineColorBlendAttachmentState colorBlendAttachmentInfo{};
		colorBlendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachmentInfo.blendEnable = m_properties.blending ? VK_TRUE : VK_FALSE;
		colorBlendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; /* finalColor.rgb */
		colorBlendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; /* finalColor.rgb */
		colorBlendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; /* finalColor.a */
		colorBlendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; /* finalColor.a */
		colorBlendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlendingInfo{};
		colorBlendingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendingInfo.logicOpEnable = VK_FALSE;
		colorBlendingInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendingInfo.attachmentCount = attachmentBlendingCount > 0 ? attachmentBlendingCount : 1;
		colorBlendingInfo.pAttachments = attachmentBlendingCount > 0 ? attachmentBlending : &colorBlendAttachmentInfo;
		colorBlendingInfo.blendConstants[0] = 0.0f;
		colorBlendingInfo.blendConstants[1] = 0.0f;
		colorBlendingInfo.blendConstants[2] = 0.0f;
		colorBlendingInfo.blendConstants[3] = 0.0f;
		pipelineInfo.pColorBlendState = &colorBlendingInfo;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = m_properties.width;
		viewport.height = m_properties.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { m_properties.width, m_properties.height };

		VkPipelineViewportStateCreateInfo viewportStateInfo{};
		viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateInfo.viewportCount = 1;
		viewportStateInfo.pViewports = m_properties.dynamicViewport ? nullptr : &viewport;
		viewportStateInfo.scissorCount = 1;
		viewportStateInfo.pScissors = m_properties.dynamicViewport ? nullptr : &scissor;
		pipelineInfo.pViewportState = &viewportStateInfo;

		std::vector<VkDynamicState> dynamicStates;
		if (m_properties.dynamicViewport)
		{
			dynamicStates.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
			dynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);
		}
		if (m_properties.wireFrame)
			dynamicStates.emplace_back(VK_DYNAMIC_STATE_LINE_WIDTH);

		VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
		dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicStateInfo.pDynamicStates = dynamicStates.empty() ? nullptr : dynamicStates.data();
		pipelineInfo.pDynamicState = dynamicStates.empty() ? nullptr : &dynamicStateInfo;

		VkResult creatPipelineResult = vkCreateGraphicsPipelines(device::get_logical(), s_pipeline_cache, 1, &pipelineInfo, nullptr, &m_pipeline);
		if (creatPipelineResult != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(creatPipelineResult, "ould not create graphics pipeline");
	}

}
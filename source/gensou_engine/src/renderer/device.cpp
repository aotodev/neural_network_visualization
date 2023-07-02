#include "renderer/device.h"
#include "renderer/validation_layers.h"

#include "core/engine_events.h"
#include "core/runtime.h"

#include "core/log.h"
#include "core/time.h"
#include "core/window.h"
#include "core/system.h"
#include <vulkan/vulkan_core.h>

extern const char* get_platform_surface_ext();

namespace gs {

	VkInstance				device::s_instance = VK_NULL_HANDLE;
	VkDevice				device::s_logical_device = VK_NULL_HANDLE;
	VkPhysicalDevice		device::s_physical_device = VK_NULL_HANDLE;

	VkQueue					device::s_graphics_queue = VK_NULL_HANDLE;
	VkQueue					device::s_compute_queue = VK_NULL_HANDLE;
	VkQueue					device::s_transfer_queue = VK_NULL_HANDLE;
	VkQueue					device::s_present_queue = VK_NULL_HANDLE;

	uint32_t				device::s_graphics_family_index = 0;
	uint32_t				device::s_compute_family_index = 0;
	uint32_t				device::s_transfer_family_index = 0;
	uint32_t				device::s_present_family_index = 0;
	uint32_t				device::s_application_api_version = 0;
	uint32_t				device::s_device_api_version = 0;

	uint32_t				device::s_vendor_id = 0;
	uint32_t				device::s_device_id = 0;
	uint32_t				device::s_driver_version = 0;

	uint8_t					device::s_pipeline_cache_uuid[VK_UUID_SIZE] = { 0 };

	std::string				device::s_device_name;

	bool					device::s_compute_queue_shared_with_graphics = false;
	bool					device::s_transfer_queue_shared_with_graphics = false;
	bool					device::s_transfer_queue_shared_with_compute = false;
	bool					device::s_integrated = false;
	bool					device::s_supports_buffer_device_address = false;
	bool					device::s_supports_lazy_allocation	= false;

	std::mutex				device::s_graphics_queue_mutex;
	std::mutex				device::s_compute_queue_mutex;
	std::mutex				device::s_transfer_queue_mutex;

	VkSampleCountFlagBits	device::s_max_supported_multisample_count = VK_SAMPLE_COUNT_1_BIT;

	size_t					device::s_min_storage_buffer_offset_alignment	= 0x0;
	size_t					device::s_min_uniform_buffer_offset_alignment	= 0x0;
	size_t					device::s_max_descriptor_samplers				= 0x0;
	size_t					device::s_max_descriptor_sampled_images			= 0x0;
	float					device::s_max_sampler_anisotropy				= 0x0;
	float 					device::s_line_width_range[2]					= { 1.0f, 1.0f };
	bool					device::s_supports_astc							= false;

	void device::init(VkPhysicalDeviceFeatures* inDeviceFeatures /*= nullptr*/)
	{
		LOG_ENGINE(info, "initing vulkan device");
		BENCHMARK("init vulkan device");

		init_instance();
		select_physical_device();
		init_logical_device(inDeviceFeatures);
	}

	void device::init_instance()
	{
		std::vector<const char*> instanceExtensions;
		instanceExtensions.reserve(4);

		instanceExtensions.push_back("VK_KHR_surface");
		instanceExtensions.push_back(get_platform_surface_ext());

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = nullptr;
		appInfo.pApplicationName = GAME_NAME;
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "GensouEngine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 2);

		#ifdef APP_ANDROID
		s_application_api_version = VK_API_VERSION_1_1;
		#else
		s_application_api_version = VK_API_VERSION_1_3;
		#endif

		appInfo.apiVersion = s_application_api_version;

		VkInstanceCreateInfo instanceCreateInfo{};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pNext = nullptr;
		instanceCreateInfo.pApplicationInfo = &appInfo;
		instanceCreateInfo.enabledLayerCount = 0;

#if defined(APP_DEBUG)

		instanceExtensions.push_back("VK_EXT_debug_utils");

		const char* enabledlayers[] = { "VK_LAYER_KHRONOS_validation" };
		VkDebugUtilsMessengerCreateInfoEXT validationLayersCreateInfo{};

		validationLayersCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		validationLayersCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		validationLayersCreateInfo.messageType = /*VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		validationLayersCreateInfo.pfnUserCallback = ValidationLayersCallback;

		instanceCreateInfo.enabledLayerCount = 1;
		instanceCreateInfo.ppEnabledLayerNames = enabledlayers;
		instanceCreateInfo.pNext = &validationLayersCreateInfo;
#endif

		for (const auto& ext : instanceExtensions)
			LOG_ENGINE(trace, "extension: %s", ext);

		instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
		VkResult createInstanceResult = vkCreateInstance(&instanceCreateInfo, nullptr, &s_instance);

		if (createInstanceResult != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(createInstanceResult, "Could not create a Vulkan Instance");

		uint32_t APIversion = VK_API_VERSION_1_0;
		if(auto FN_vkEnumerateInstanceVersion = PFN_vkEnumerateInstanceVersion(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion")))
    		FN_vkEnumerateInstanceVersion(&APIversion );

		LOG_ENGINE(trace, "created Vulkan instance witn API version: %u.%u.%u", ((APIversion >> 22) & 0x7FU), ((APIversion >> 12) & 0x3FFU), (APIversion & 0xFFFU));
	}

	void device::select_physical_device()
	{
		uint32_t availablePhysicalDevicesCount;
		vkEnumeratePhysicalDevices(s_instance, &availablePhysicalDevicesCount, nullptr);

		if (availablePhysicalDevicesCount == 1)
		{
			vkEnumeratePhysicalDevices(s_instance, &availablePhysicalDevicesCount, &s_physical_device);
		}
		else if (availablePhysicalDevicesCount > 1)
		{
			std::vector<VkPhysicalDevice> physicalDevicesList(availablePhysicalDevicesCount, VK_NULL_HANDLE);
			vkEnumeratePhysicalDevices(s_instance, &availablePhysicalDevicesCount, physicalDevicesList.data());

			//TODO select most suitable device, for now we'll just pick the first one
			s_physical_device = physicalDevicesList[0];
		}
		else
		{
			LOG_ENGINE(critical, "no Vulkan compatible physical device found");
		}

		VkPhysicalDeviceMemoryProperties devicemeoryProperties;
	}

	void device::init_logical_device(VkPhysicalDeviceFeatures* inDeviceFeatures /*= nullptr*/)
	{
		//--------------------------------------------------------------------------
		// QUERY DEVICE PROPERTIES
		//--------------------------------------------------------------------------

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(s_physical_device, &deviceProperties);

		s_min_storage_buffer_offset_alignment = deviceProperties.limits.minStorageBufferOffsetAlignment;
		s_min_uniform_buffer_offset_alignment = deviceProperties.limits.minUniformBufferOffsetAlignment;
		s_max_descriptor_samplers = deviceProperties.limits.maxPerStageDescriptorSamplers;
		s_max_descriptor_sampled_images = deviceProperties.limits.maxPerStageDescriptorSampledImages;
		s_max_sampler_anisotropy = deviceProperties.limits.maxSamplerAnisotropy;
		
		s_line_width_range[0] = deviceProperties.limits.lineWidthRange[0];
		s_line_width_range[1] = deviceProperties.limits.lineWidthRange[1];

		VkSampleCountFlags counts = deviceProperties.limits.framebufferColorSampleCounts & deviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & VK_SAMPLE_COUNT_64_BIT)
			s_max_supported_multisample_count = VK_SAMPLE_COUNT_64_BIT;
		else if (counts & VK_SAMPLE_COUNT_32_BIT)
			s_max_supported_multisample_count = VK_SAMPLE_COUNT_32_BIT;
		else if (counts & VK_SAMPLE_COUNT_16_BIT)
			s_max_supported_multisample_count = VK_SAMPLE_COUNT_16_BIT;
		else if (counts & VK_SAMPLE_COUNT_8_BIT)
			s_max_supported_multisample_count = VK_SAMPLE_COUNT_8_BIT;
		else if (counts & VK_SAMPLE_COUNT_4_BIT)
			s_max_supported_multisample_count = VK_SAMPLE_COUNT_4_BIT;
		else if (counts & VK_SAMPLE_COUNT_2_BIT)
			s_max_supported_multisample_count = VK_SAMPLE_COUNT_2_BIT;

		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_D32_SFLOAT_S8_UINT, &formatProperties);

		if(s_integrated = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
		    LOG_ENGINE(info, "device is integrated");

		s_device_name = deviceProperties.deviceName;
		s_device_api_version = deviceProperties.apiVersion;
		s_vendor_id = deviceProperties.vendorID;
		s_device_id = deviceProperties.deviceID;
		s_driver_version = deviceProperties.driverVersion;
		memcpy(s_pipeline_cache_uuid, deviceProperties.pipelineCacheUUID, VK_UUID_SIZE * sizeof(uint8_t));

		/* check memory types */
		VkPhysicalDeviceMemoryProperties memProperties{};
		vkGetPhysicalDeviceMemoryProperties(s_physical_device, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
			{
				LOG_ENGINE(info, "has lazily allocated support");
				s_supports_lazy_allocation = true;
				break;
			}
		}
		if(!s_supports_lazy_allocation)
			LOG_ENGINE(info, "does not have lazily allocated support");

		//--------------------------------------------------------------------------
		// ENABLE NEEDED FEATURES
		//--------------------------------------------------------------------------

		VkPhysicalDeviceFeatures deviceEnabledFeatures{};
		constexpr size_t fSize = sizeof(VkPhysicalDeviceFeatures);
		if (inDeviceFeatures)
			deviceEnabledFeatures = *inDeviceFeatures;

		/* required core features
		 * features garanteed by the android baseline
		 * see: https://github.com/KhronosGroup/Vulkan-Profiles/blob/master/profiles/VP_ANDROID_baseline_2021.json
		 */

		//deviceEnabledFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
		//deviceEnabledFeatures.shaderStorageImageArrayDynamicIndexing = VK_TRUE;
		//deviceEnabledFeatures.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
		//deviceEnabledFeatures.wideLines = VK_TRUE;

		#if USE_OIT
		deviceEnabledFeatures.independentBlend = VK_TRUE;
		#endif

		#if USE_MULTISAMPLE
		deviceEnabledFeatures.sampleRateShading = VK_TRUE;
		#endif

		#ifdef APP_ANDROID
		deviceEnabledFeatures.textureCompressionASTC_LDR = VK_TRUE;
		#endif

		#if ENABLE_ANISOTROPY
		if (s_max_sampler_anisotropy > 1.0f)
		{
			deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
			LOG_ENGINE(trace, "Max Sampler Anisotropy: %.1f", s_max_sampler_anisotropy);
		}
		#endif

#ifdef VULKAN_GLSL_1_2 		//1.2 features
		VkPhysicalDeviceVulkan12Features vulkan12EnabledFeatures{};
		vulkan12EnabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vulkan12EnabledFeatures.bufferDeviceAddress = VK_TRUE;
		vulkan12EnabledFeatures.descriptorIndexing = VK_TRUE;
		vulkan12EnabledFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

		VkPhysicalDeviceVulkan12Features temp_vulkan12Features{};
		temp_vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

		VkPhysicalDeviceFeatures2 deviceFeatures2{};
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures2.pNext = &temp_vulkan12Features;

		vkGetPhysicalDeviceFeatures2(s_physical_device, &deviceFeatures2);

		if (temp_vulkan12Features.bufferDeviceAddress == VK_TRUE)
		{
			s_supports_buffer_device_address = VK_TRUE;
			LOG_ENGINE(trace, "bufferDeviceAddress feature supported");
		}
		else
		{
			LOG_ENGINE(warn, "bufferDeviceAddress feature not supported");
		}
#endif
		VkPhysicalDeviceFeatures hasFeatures{};
		VkPhysicalDeviceFeatures* pHasFeatures = &hasFeatures;
		vkGetPhysicalDeviceFeatures(s_physical_device, pHasFeatures);

		if (pHasFeatures->shaderSampledImageArrayDynamicIndexing == VK_FALSE)
		{
			LOG_ENGINE(critical, "This device's vulkan driver does not support dynamic indexing into a sampler2DArray [shaderSampledImageArrayDynamicIndexing]");
			system::error_msg("REQUIRED FEATURE NOT FOUND\n[shaderSampledImageArrayDynamicIndexing]\nThis device's vulkan driver does not support dynamic indexing into a sampler2DArray");
			exit(-1);
		}

		//if(pHasFeatures->wideLines == VK_FALSE)
		//{
			//LOG_ENGINE(critical, "This device's vulkan driver does not support WIDE LINES");
			//system::error_msg("REQUIRED FEATURE NOT FOUND\n[wideLines]\nThis device's vulkan driver does not wide lines");
			//exit(-1);
		//}

		#ifdef APP_ANDROID
		if (pHasFeatures->textureCompressionASTC_LDR == VK_FALSE)
		{
			LOG_ENGINE(critical, "This device's vulkan driver does not support ASTC [textureCompressionASTC_LDR]");
			system::error_msg("REQUIRED FEATURE NOT FOUND\n[textureCompressionASTC_LDR]\nThis device's vulkan driver does not support ASTC");
			exit(-1);
		}
		else
			s_supports_astc = true;
		#endif

		#if USE_OIT
		if (pHasFeatures->independentBlend == VK_FALSE)
		{
			LOG_ENGINE(critical, "This device's vulkan driver does not support ASTC [independentBlend]");
			system::error_msg("REQUIRED FEATURE NOT FOUND\n[independentBlend]\nThis device's vulkan driver does not support independent blend");
			exit(-1);
		}
		#endif

		#if USE_MULTISAMPLE
		if (pHasFeatures->sampleRateShading == VK_FALSE)
		{
			LOG_ENGINE(critical, "This device's vulkan driver does not support multisampling [sampleRateShading]");
			system::error_msg("REQUIRED FEATURE NOT FOUND\n[sampleRateShading]\nThis device's vulkan driver does not support sample rate shading");
			exit(-1);
		}
		#endif

		VkBool32* ref = reinterpret_cast<VkBool32*>(&deviceEnabledFeatures);
		VkBool32* features = reinterpret_cast<VkBool32*>(pHasFeatures);

		for (size_t i = 0; i < (sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)); ++i)
		{
			if (*(ref + i))
				if (!(*(features + i)))
				{
					LOG_ENGINE(error, "requested device feature not found");
					system::error_msg("requested device feature not found");
				}
		}

		//--------------------------------------------------------------------------
		// PREPARE QUEUES INFO
		//--------------------------------------------------------------------------

		uint32_t queueFamiliesCount;
		vkGetPhysicalDeviceQueueFamilyProperties(s_physical_device, &queueFamiliesCount, nullptr);
		std::vector<VkQueueFamilyProperties> qfPropertiesVector(queueFamiliesCount);
		vkGetPhysicalDeviceQueueFamilyProperties(s_physical_device, &queueFamiliesCount, qfPropertiesVector.data());

		uint32_t graphicsQueueCount = 0, computeQueueCount = 0, transferQueueCount = 0;
		std::vector<uint32_t> familyIndices;
		std::vector<VkDeviceQueueCreateInfo> createQueueInfo;
		std::vector<float> queuePriorityArray(3, 1.0f);

		for (uint32_t i = 0; i < qfPropertiesVector.size(); ++i)
		{
			if (qfPropertiesVector[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				++graphicsQueueCount;
				if (graphicsQueueCount == 1)
				{
					s_graphics_family_index = i;
					familyIndices.push_back(i);

					createQueueInfo.emplace_back();
					createQueueInfo.back().sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					createQueueInfo.back().pNext = nullptr;
					createQueueInfo.back().flags = 0;
					createQueueInfo.back().queueFamilyIndex = s_graphics_family_index;
					createQueueInfo.back().queueCount = 1;
					createQueueInfo.back().pQueuePriorities = queuePriorityArray.data();
				}
			}
			else if (qfPropertiesVector[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				++computeQueueCount;
				if (computeQueueCount == 1)
				{
					s_compute_family_index = i;
					familyIndices.push_back(i);

					createQueueInfo.emplace_back();
					createQueueInfo.back().sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					createQueueInfo.back().pNext = nullptr;
					createQueueInfo.back().flags = 0;
					createQueueInfo.back().queueFamilyIndex = s_compute_family_index;
					createQueueInfo.back().queueCount = 1;
					createQueueInfo.back().pQueuePriorities = queuePriorityArray.data();
				}
			}
			else if (qfPropertiesVector[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				++transferQueueCount;
				if (transferQueueCount == 1)
				{
					s_transfer_family_index = i;
					familyIndices.push_back(i);

					createQueueInfo.emplace_back();
					createQueueInfo.back().sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					createQueueInfo.back().pNext = nullptr;
					createQueueInfo.back().flags = 0;
					createQueueInfo.back().queueFamilyIndex = s_transfer_family_index;
					createQueueInfo.back().queueCount = 1;
					createQueueInfo.back().pQueuePriorities = queuePriorityArray.data();
				}
			}
		}

		LOG_ENGINE(trace, "queue families:\n\tgraphics count:\t%u\n\tcompute count:\t%u\n\ttransfer count:\t%u", graphicsQueueCount, computeQueueCount, transferQueueCount);

		if (graphicsQueueCount == 0)
		{
			LOG_ENGINE(critical, "No graphics queue found");
			system::error_msg("No graphics queue found, impossible to render");
			exit(-1);

		}

		if (computeQueueCount == 0)
		{
			s_compute_family_index = s_graphics_family_index;
			if (graphicsQueueCount > 1)
				createQueueInfo[0].queueCount += 1;
		}

		if (transferQueueCount == 0)
		{
			if (computeQueueCount == 0)
			{
				s_transfer_family_index = s_graphics_family_index;
				createQueueInfo[0].queueCount += 1;
			}
			else if (graphicsQueueCount > 1)
			{
				s_transfer_family_index = s_graphics_family_index;
				createQueueInfo[0].queueCount += 1;
			}
			else
			{
				s_transfer_family_index = s_compute_family_index;
				createQueueInfo[1].queueCount += 1;
			}
		}

		//--------------------------------------------------------------------------
		// CREATE LOGICAL DEVICE
		//--------------------------------------------------------------------------

		std::array<const char*, 8> deviceExtArray;
		uint32_t extCount = 1;
		deviceExtArray[0] = "VK_KHR_swapchain";

		uint32_t apiMajor = (s_device_api_version >> 22) & 0x7FU;
		uint32_t apiMinor = (s_device_api_version >> 12) & 0x3FFU;
		uint32_t apiPatch = s_device_api_version & 0xFFFU;

		//if (apiMinor < 1ul)
		if (false)
		{
			// minor is 0. needs some extensions. check if they are available (should be if driver follows android vulkan baseline)

			uint32_t deviceExtCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &deviceExtCount, nullptr);
			std::vector<VkExtensionProperties> extensions(extCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &deviceExtCount, extensions.data());
			uint32_t desiredExtCount = 3, actualExtCount = 0;
			for (const auto& extension : extensions)
			{
				if(strcmp(extension.extensionName, "VK_KHR_maintenance1") == 0)
				{
					deviceExtArray[extCount] = "VK_KHR_maintenance1";
					extCount++;
					actualExtCount++;
					continue;
				}

				if(strcmp(extension.extensionName, "VK_KHR_maintenance2") == 0)
				{
					deviceExtArray[extCount] = "VK_KHR_maintenance2";
					extCount++;
					actualExtCount++;
					continue;
				}

				if(strcmp(extension.extensionName, "VK_KHR_maintenance3") == 0)
				{
					deviceExtArray[extCount] = "VK_KHR_maintenance3";
					extCount++;
					actualExtCount++;
					continue;
				}

				if(actualExtCount == desiredExtCount)
					break;
			}

		}

		VkDeviceCreateInfo logicalDeviceCreateInfo{};
		logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
#ifdef VULKAN_GLSL_1_2
		logicalDeviceCreateInfo.pNext = s_supports_buffer_device_address ? &vulkan12EnabledFeatures : nullptr;
		/* 	if (supports descriptor_indexing) 
			{
				deviceExtArray[extCount] = VK_EXT_descriptor_indexing;
				extCount++;
	    	}
		 */
#else
		logicalDeviceCreateInfo.enabledExtensionCount = extCount;
		logicalDeviceCreateInfo.pNext = nullptr;
#endif
		logicalDeviceCreateInfo.queueCreateInfoCount = (uint32_t)createQueueInfo.size();
		logicalDeviceCreateInfo.pQueueCreateInfos = createQueueInfo.data();
		logicalDeviceCreateInfo.ppEnabledExtensionNames = deviceExtArray.data();
		logicalDeviceCreateInfo.pEnabledFeatures = &deviceEnabledFeatures;

		LOG_ENGINE(info, "extension count == %u", extCount);

		for(uint32_t i = 0; i < extCount; i++)
			LOG_ENGINE(trace, "device_ext[%u] == %s", i, deviceExtArray[i]);

		VkResult createDeviceResult = vkCreateDevice(s_physical_device, &logicalDeviceCreateInfo, nullptr, &s_logical_device);

		if (createDeviceResult != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(createDeviceResult, "Could not create a Vulkan Logical Device");

		LOG_ENGINE(info, "Created logical device using physical device with API version %u.%u.%u and name:", apiMajor, apiMinor, apiPatch);
        LOG_ENGINE(info, "%s", s_device_name.c_str());

		//--------------------------------------------------------------------------
		// GET DEVICE QUEUES
		//--------------------------------------------------------------------------

		vkGetDeviceQueue(s_logical_device, s_graphics_family_index, 0, &s_graphics_queue);

		if (computeQueueCount == 0)
		{
			if (graphicsQueueCount > 1)
			{
				vkGetDeviceQueue(s_logical_device, s_graphics_family_index, 1, &s_compute_queue);
				LOG_ENGINE(trace, "Compute queue using a dedicated graphics queue");
			}
			else
			{
				vkGetDeviceQueue(s_logical_device, s_graphics_family_index, 0, &s_compute_queue);
				s_compute_queue_shared_with_graphics = true;
				LOG_ENGINE(trace, "Compute queue sharing a graphics queue with graphics");
			}
		}
		else
		{
			vkGetDeviceQueue(s_logical_device, s_compute_family_index, 0, &s_compute_queue);
			LOG_ENGINE(trace, "Compute queue using a dedicated compute queue");
		}

		if (transferQueueCount == 0)
		{
			if (computeQueueCount == 0) // 1st try getting a surogate graphics queue
			{
				if (graphicsQueueCount > 2)
				{
					vkGetDeviceQueue(s_logical_device, s_graphics_family_index, 2, &s_transfer_queue);
					LOG_ENGINE(trace, "Transfer queue using a dedicated graphics queue");
				}
				else if (graphicsQueueCount > 1)
				{
					vkGetDeviceQueue(s_logical_device, s_graphics_family_index, 1, &s_transfer_queue);
					s_transfer_queue_shared_with_compute = true;
					LOG_ENGINE(trace, "Transfer queue sharing a graphics queue with compute");
				}
				else
				{
					vkGetDeviceQueue(s_logical_device, s_graphics_family_index, 0, &s_transfer_queue);
					s_transfer_queue_shared_with_graphics = true;
					s_transfer_queue_shared_with_compute = true;
					LOG_ENGINE(trace, "Graphics, compute and transfer queues all share the same graphics queue");
				}
			}
			else if (graphicsQueueCount > 1)
			{
				vkGetDeviceQueue(s_logical_device, s_graphics_family_index, 1, &s_transfer_queue);
				LOG_ENGINE(trace, "Transfer queue using a dedicated graphics queue");
			}
			else if (computeQueueCount > 1) // 2nd try getting a surrogate compute queue
			{
				vkGetDeviceQueue(s_logical_device, s_compute_family_index, 1, &s_transfer_queue);
				LOG_ENGINE(trace, "Transfer queue using a dedicated compute queue");
			}
			else
			{
				vkGetDeviceQueue(s_logical_device, s_compute_family_index, 0, &s_transfer_queue);
				s_transfer_queue_shared_with_compute = true;
				LOG_ENGINE(trace, "Transfer and compute queues sharing the same compute queue");
			}
		}
		else
		{
			vkGetDeviceQueue(s_logical_device, s_transfer_family_index, 0, &s_transfer_queue);
			LOG_ENGINE(trace, "Transfer queue using a dedicated transfer queue");
		}
		LOG_ENGINE(trace, "finished getting device queues");
	}

	void device::terminate()
	{
		/* make sure all operations which depend on this device have properly finished before destroying it */
		vkDeviceWaitIdle(s_logical_device);

		if (s_logical_device != VK_NULL_HANDLE)
			vkDestroyDevice(s_logical_device, nullptr);

		if (s_instance != VK_NULL_HANDLE)
			vkDestroyInstance(s_instance, nullptr);

		LOG_ENGINE(warn, "Terminated Vulkan");
	}


	VkQueue device::get_queue_by_index(uint32_t index)
	{
		if (index == s_graphics_family_index)
			return s_graphics_queue;
		else if (index == s_compute_family_index)
			return s_compute_queue;
		else if (index == s_transfer_family_index)
			return s_transfer_queue;
		else
			return VK_NULL_HANDLE;
	}

	VkBool32 device::select_present_queue(VkSurfaceKHR inSurface)
	{
		VkBool32 canPresent = VK_FALSE;

		vkGetPhysicalDeviceSurfaceSupportKHR(s_physical_device, s_graphics_family_index, inSurface, &canPresent);
		if (canPresent)
		{
			s_present_queue = s_graphics_queue;
			s_present_family_index = s_graphics_family_index;
			return VK_TRUE;
		}

		vkGetPhysicalDeviceSurfaceSupportKHR(s_physical_device, s_compute_family_index, inSurface, &canPresent);
		if (canPresent)
		{
			s_present_queue = s_compute_queue;
			s_present_family_index = s_compute_family_index;
			return VK_TRUE;
		}

		vkGetPhysicalDeviceSurfaceSupportKHR(s_physical_device, s_transfer_family_index, inSurface, &canPresent);
		if (canPresent)
		{
			s_present_queue = s_transfer_queue;
			s_present_family_index = s_transfer_family_index;
			return VK_TRUE;
		}

		return VK_FALSE;
	}

	bool device::format_supports_blitt(VkFormat format)
	{
		VkFormatProperties formatProperties;

		vkGetPhysicalDeviceFormatProperties(s_physical_device, format, &formatProperties);
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT && formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)
			return true;

		return false;
	}

	bool device::format_supports_src_blitt(VkFormat format)
	{
		VkFormatProperties formatProperties;

		vkGetPhysicalDeviceFormatProperties(s_physical_device, format, &formatProperties);
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)
			return true;

		return false;
	}

	bool device::format_supports_dst_blitt(VkFormat format)
	{
		VkFormatProperties formatProperties;

		vkGetPhysicalDeviceFormatProperties(s_physical_device, format, &formatProperties);
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)
			return true;

		return false;
	}

	bool device::supports_astc_format() { return s_supports_astc; }

	VkFormat device::get_hdr_attachment_blend_format(VkFormat preferedFormat /*=VK_FORMAT_UNDEFINED*/)
	{
		VkFormatProperties formatProperties;

		if (preferedFormat != VK_FORMAT_UNDEFINED)
		{
			vkGetPhysicalDeviceFormatProperties(s_physical_device, preferedFormat, &formatProperties);
			if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
				return preferedFormat;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R32G32B32A32_SFLOAT, &formatProperties); //VK_FORMAT_B8G8R8A8_SRGB
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
		{
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R16G16B16A16_SFLOAT, &formatProperties); //VK_FORMAT_B8G8R8A8_SRGB
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
		{
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		}

		LOG_ENGINE(warn, "This device does not have any format which supports HDR color attachment with blend");
		return VK_FORMAT_UNDEFINED;
	}

	VkFormat device::get_hdr_linear_sample_format(VkFormat preferedFormat /*= VK_FORMAT_UNDEFINED*/)
	{
		VkFormatProperties formatProperties;

		if (preferedFormat != VK_FORMAT_UNDEFINED)
		{
			vkGetPhysicalDeviceFormatProperties(s_physical_device, preferedFormat, &formatProperties);
			if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
				return preferedFormat;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R32G32B32A32_SFLOAT, &formatProperties); //VK_FORMAT_B8G8R8A8_SRGB
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
		{
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R16G16B16A16_SFLOAT, &formatProperties); //VK_FORMAT_B8G8R8A8_SRGB
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
		{
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		}

		LOG_ENGINE(warn, "This device does not have any format which supports Sampled Image with linear filter, returning a LDR one");
		return VK_FORMAT_R8G8B8A8_SRGB;
	}

	VkFormat device::get_hdr_linear_sample_blitt_format(VkFormat preferedFormat /*= VK_FORMAT_UNDEFINED*/)
	{
		VkFormatProperties formatProperties;

		if (preferedFormat != VK_FORMAT_UNDEFINED)
		{
			vkGetPhysicalDeviceFormatProperties(s_physical_device, preferedFormat, &formatProperties);
			if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT &&
				formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT &&
				formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)
				return preferedFormat;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R32G32B32A32_SFLOAT, &formatProperties); //VK_FORMAT_B8G8R8A8_SRGB
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT &&
			formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT &&
			formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)
		{
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R16G16B16A16_SFLOAT, &formatProperties); //VK_FORMAT_B8G8R8A8_SRGB
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT &&
			formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT &&
			formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)
		{
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		}

		LOG_ENGINE(warn, "This device does not have any HDR format which supports Sampled Image with linear filter, returning a LDR one");
		return VK_FORMAT_R8G8B8A8_SRGB;
	}

	VkFormat device::get_color_blitt_format(VkFormat preferedFormat /*= VK_FORMAT_UNDEFINED*/)
	{
		VkFormatProperties formatProperties;

		if (preferedFormat != VK_FORMAT_UNDEFINED)
		{
			vkGetPhysicalDeviceFormatProperties(s_physical_device, preferedFormat, &formatProperties);
			if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT &&
				formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)
				return preferedFormat;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R8G8B8A8_SRGB, &formatProperties); //VK_FORMAT_B8G8R8A8_SRGB
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT &&
			formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)
		{
			return VK_FORMAT_R8G8B8A8_SRGB;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties); //VK_FORMAT_B8G8R8A8_UNORM
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT &&
			formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)
		{
			LOG_ENGINE(warn, "no sRGB format with blitt support was found, returning a linear one");
			return VK_FORMAT_R8G8B8A8_UNORM;
		}

		return VK_FORMAT_UNDEFINED;
	}

	VkFormat device::get_storage_image_format(VkFormat preferedFormat /*=VK_FORMAT_UNDEFINED*/)
	{
		VkFormatProperties formatProperties;

		if (preferedFormat != VK_FORMAT_UNDEFINED)
		{
			vkGetPhysicalDeviceFormatProperties(s_physical_device, preferedFormat, &formatProperties);
			if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
				return preferedFormat;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R8G8B8A8_SRGB, &formatProperties); //VK_FORMAT_B8G8R8A8_SRGB
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
		{
			return VK_FORMAT_R8G8B8A8_SRGB;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R16G16B16A16_SFLOAT, &formatProperties); //VK_FORMAT_B8G8R8A8_SRGB
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
		{
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		}

		vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties); //VK_FORMAT_B8G8R8A8_UNORM
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
		{
			return VK_FORMAT_R8G8B8A8_UNORM;
		}

		return VK_FORMAT_UNDEFINED;
	}

	VkFormat device::get_depth_format(uint8_t precision, bool stencilRequired)
	{
		VkFormatProperties properties;

		if (precision <= 16)
		{
			if (!stencilRequired)
			{
				vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_D16_UNORM, &properties);
				if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				{
					LOG_ENGINE(info, "Using VK_FORMAT_D16_UNORM depth format");
					return VK_FORMAT_D16_UNORM;
				}
			}

			vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_D16_UNORM_S8_UINT, &properties);
			if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				return VK_FORMAT_D16_UNORM_S8_UINT;

		}
		else if (precision <= 24)
		{
			vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_D24_UNORM_S8_UINT, &properties);
			if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				return VK_FORMAT_D24_UNORM_S8_UINT;
		}
		else
		{
			if (!stencilRequired)
			{
				vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_D32_SFLOAT, &properties);
				if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
					return VK_FORMAT_D32_SFLOAT;
			}

			vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_D32_SFLOAT_S8_UINT, &properties);
			if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				return VK_FORMAT_D32_SFLOAT_S8_UINT;
		}

		/* if we reached this point it means we did not find a format with stencil */
		if (stencilRequired)
		{
			LOG_ENGINE(critical, "no stencil Buffer support, trying to find depth only support");

			if (precision <= 16)
			{
				vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_D16_UNORM, &properties);
				if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
					return VK_FORMAT_D16_UNORM;
			}
			else
			{
				vkGetPhysicalDeviceFormatProperties(s_physical_device, VK_FORMAT_D32_SFLOAT, &properties);
				if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
					return VK_FORMAT_D32_SFLOAT;
			}
		}

		LOG_ENGINE(critical, "no Depth Buffer support");
		return VK_FORMAT_UNDEFINED;
	}

	void device::set_multisample_count(uint32_t desiredCount)
	{
		runtime::s_desired_multisample_count = desiredCount;
		runtime::s_multisample_count = std::min(runtime::s_desired_multisample_count, (uint32_t)s_max_supported_multisample_count);
	}

}
#pragma once

#include "core/log.h"
#include "core/core.h"

#include <vulkan/vulkan.h>


static const char* get_vulkan_result_as_string(VkResult result)
{
	switch (result)
	{
	case VK_SUCCESS:
		return "VK_SUCCESS";
	case VK_NOT_READY:
		return "VK_NOT_READY";
	case VK_TIMEOUT:
		return "VK_TIMEOUT";
	case VK_EVENT_SET:
		return "VK_EVENT_SET";
	case VK_EVENT_RESET:
		return "VK_EVENT_RESET";
	case VK_INCOMPLETE:
		return "VK_INCOMPLETE";
	case VK_ERROR_OUT_OF_HOST_MEMORY:
		return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY:
		return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED:
		return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST:
		return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED:
		return "VK_ERROR_MEMORY_MAP_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT:
		return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT:
		return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT:
		return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER:
		return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_TOO_MANY_OBJECTS:
		return "VK_ERROR_TOO_MANY_OBJECTS";
	case VK_ERROR_FORMAT_NOT_SUPPORTED:
		return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_FRAGMENTED_POOL:
		return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_UNKNOWN:
		return "VK_ERROR_UNKNOWN";
	case VK_ERROR_OUT_OF_POOL_MEMORY:
		return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE:
		return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case VK_ERROR_FRAGMENTATION:
		return "VK_ERROR_FRAGMENTATION";
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
		return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
	case VK_ERROR_SURFACE_LOST_KHR:
		return "VK_ERROR_SURFACE_LOST_KHR";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
		return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case VK_SUBOPTIMAL_KHR:
		return "VK_SUBOPTIMAL_KHR";
	case VK_ERROR_OUT_OF_DATE_KHR:
		return "VK_ERROR_OUT_OF_DATE_KHR";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
		return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case VK_ERROR_VALIDATION_FAILED_EXT:
		return "VK_ERROR_VALIDATION_FAILED_EXT";
	case VK_ERROR_INVALID_SHADER_NV:
		return "VK_ERROR_INVALID_SHADER_NV";
	//case VK_ERROR_INCOMPATIBLE_VERSION_KHR:
		//return "VK_ERROR_INCOMPATIBLE_VERSION_KHR";
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
		return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
	case VK_ERROR_NOT_PERMITTED_EXT:
		return "VK_ERROR_NOT_PERMITTED_EXT";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
		return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
	case VK_THREAD_IDLE_KHR:
		return "VK_THREAD_IDLE_KHR";
	case VK_THREAD_DONE_KHR:
		return "VK_THREAD_DONE_KHR";
	case VK_OPERATION_DEFERRED_KHR:
		return "VK_OPERATION_DEFERRED_KHR";
	case VK_OPERATION_NOT_DEFERRED_KHR:
		return "VK_OPERATION_NOT_DEFERRED_KHR";
	case VK_PIPELINE_COMPILE_REQUIRED_EXT:
		return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
	case VK_RESULT_MAX_ENUM:
		return "VK_RESULT_MAX_ENUM";
	default:
		return "invalid vkResult enum";
	}
}

#ifdef APP_DEBUG
	#if defined(APP_WINDOWS)
		#define INTERNAL_DEBUGBREAK() __debugbreak()
	#elif defined(APP_LINUX)
		#include <signal.h>
		#define INTERNAL_DEBUGBREAK() raise(SIGTRAP)
	#else
		//#error "No debugbreak support"
		#define INTERNAL_DEBUGBREAK()
	#endif

#define INTERNAL_ASSERT(condition, message, ...) { if (!(condition)) { LOG_ENGINE(critical, "Assertion %s failed in: %s at line %d with message: %s", #condition, __FILE__, __LINE__, message); INTERNAL_DEBUGBREAK(); } }
#define INTERNAL_ASSERT_VKRESULT(result, message, ...) { if (result != VK_SUCCESS) { LOG_ENGINE(critical, "VkResult Assertion failed in: %s, at line: %d, with VkResult == %s and message: %s", __FILE__, __LINE__, get_vulkan_result_as_string(result), message); } }
#else
	#define INTERNAL_DEBUGBREAK()
	#define INTERNAL_ASSERT(condition, message, ...)
	#define INTERNAL_ASSERT_VKRESULT(result, message, ...)
#endif


static void CheckVulkanError(VkResult result)
{
	if (result != VK_SUCCESS)
	{
		LOG_ENGINE(critical, "VkResult Assertion failed in: %s, at line: %d, with VkResult == %s ", __FILE__, __LINE__, get_vulkan_result_as_string(result));
	}
}

static inline VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayersCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::stringstream types;

	if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
		types << "(general)";
	if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
		types << "(validation)";
	if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
		types << "(performance)";

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		LOG_ENGINE(critical, "Vulkan ERROR at file %s and line %d", __FILE__, __LINE__);
		LOG_ENGINE(error, "Validation Layer [severity error] [type(s) %s]: %s ", types.str().c_str(), pCallbackData->pMessage);
		//INTERNAL_DEBUGBREAK();
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		LOG_ENGINE(warn, "Validation Layer [severity warning] [type(s) %s]: %s", types.str().c_str(), pCallbackData->pMessage);
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		LOG_ENGINE(info, "Validation Layer [severity info] [type(s) %s]: %s", types.str().c_str(), pCallbackData->pMessage);
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		LOG_ENGINE(trace, "Validation Layer [severity verbose] [type(s) %s]: %s", types.str().c_str(), pCallbackData->pMessage);
	else
		LOG_ENGINE(trace, "Validation Layer [severity unknown] [type(s) %s]: %s", types.str().c_str(), pCallbackData->pMessage);

	return VK_FALSE;
}
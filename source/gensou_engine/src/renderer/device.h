#pragma once

#include "core/core.h"
#include "core/log.h"

#include <vulkan/vulkan.h>

namespace gs {

	class device
	{
	public:
		static void init(VkPhysicalDeviceFeatures* inDeviceFeatures = nullptr);

		static VkInstance get_instance() { return s_instance; }
		static VkDevice get_logical() { return s_logical_device; }
		static VkPhysicalDevice get_physical() { return s_physical_device; }

		static VkQueue get_queue(queue_family queueFamily)
		{
			switch (queueFamily)
			{
			case queue_family::graphics: return s_graphics_queue;
			case queue_family::compute: return s_compute_queue;
			case queue_family::transfer: return s_transfer_queue;
			case queue_family::present: return s_present_queue;
			default: return VK_NULL_HANDLE;
			}
		}

		static VkQueue get_graphics_queue() { return s_graphics_queue; }
		static VkQueue get_compute_queue() { return s_compute_queue; }
		static VkQueue get_transfer_queue() { return s_transfer_queue; }
		static VkQueue get_present_queue() { return s_present_queue; }

		static uint32_t get_graphics_family_index() { return s_graphics_family_index; }
		static uint32_t get_compute_family_index() { return s_compute_family_index; }
		static uint32_t get_transfer_family_index() { return s_transfer_family_index; }
		static uint32_t get_present_family_index() { return s_present_family_index; }

		static bool is_compute_queue_same_as_graphics() { return s_compute_queue_shared_with_graphics; }
		static bool is_transfer_queue_same_as_graphics() { return s_transfer_queue_shared_with_graphics; }
		static bool is_transfer_queue_same_as_compute() { return s_transfer_queue_shared_with_compute; }

		static std::mutex& get_graphics_queue_mutex() { return s_graphics_queue_mutex; }
		static std::mutex& get_compute_queue_mutex() { return s_compute_queue_shared_with_graphics ? s_graphics_queue_mutex : s_compute_queue_mutex; }
		static std::mutex& get_transfer_queue_mutex()
		{
			if (s_transfer_queue_shared_with_graphics) return s_graphics_queue_mutex;
			if (s_transfer_queue_shared_with_compute) return get_compute_queue_mutex();

			return s_transfer_queue_mutex;
		}

		static std::mutex& get_queue_mutex(queue_family queueFamily)
		{
			switch (queueFamily)
			{
			case queue_family::graphics: return get_graphics_queue_mutex();
			case queue_family::compute: return get_compute_queue_mutex();
			case queue_family::transfer: return get_transfer_queue_mutex();
			case queue_family::present: return get_graphics_queue_mutex();
			default: assert(false);
			}
		}

		static bool is_integrated() { return s_integrated; }
		static bool supports_lazy_allocation() { return s_supports_lazy_allocation; }

		static VkQueue get_queue_by_index(uint32_t index);
		static VkBool32 select_present_queue(VkSurfaceKHR inSurface);

		static size_t min_storage_buffer_offset_alignment() { return s_min_storage_buffer_offset_alignment; }
		static size_t min_uniform_buffer_offset_alignment() { return s_min_uniform_buffer_offset_alignment; }

		static size_t max_descriptor_samplers() { return s_max_descriptor_samplers; }
		static size_t max_descriptor_sampled_images() { return s_max_descriptor_sampled_images; }
		static float max_sampler_anisotropy() { return s_max_sampler_anisotropy; }

		static VkBool32 supports_buffer_device_address() { return s_supports_buffer_device_address; }

		static const std::string& get_device_name() { return s_device_name; }
		static uint32_t get_device_api_version() { return s_device_api_version; }
		static uint32_t get_application_api_version() { return s_application_api_version; }

		static uint32_t vendor_id() { return s_vendor_id; }
		static uint32_t device_id() { return s_device_id; }
		static uint32_t driver_version() { return s_driver_version; }
		static uint8_t* pipeline_cache_uuid() { return s_pipeline_cache_uuid; }

		static bool format_supports_blitt(VkFormat format);
		static bool format_supports_src_blitt(VkFormat format);
		static bool format_supports_dst_blitt(VkFormat format);
		static bool supports_astc_format();

		static float min_line_width() { return s_line_width_range[0]; }
		static float max_line_width() { return s_line_width_range[1]; }

		static VkFormat get_hdr_attachment_blend_format(VkFormat preferedFormat = VK_FORMAT_UNDEFINED);
		static VkFormat get_hdr_linear_sample_format(VkFormat preferedFormat = VK_FORMAT_UNDEFINED);
		static VkFormat get_hdr_linear_sample_blitt_format(VkFormat preferedFormat = VK_FORMAT_UNDEFINED);
		static VkFormat get_color_blitt_format(VkFormat preferedFormat = VK_FORMAT_UNDEFINED);
		static VkFormat get_storage_image_format(VkFormat preferedFormat = VK_FORMAT_UNDEFINED);
		static VkFormat get_depth_format(uint8_t precision = 24, bool stencilRequired = false);

		static void set_multisample_count(uint32_t desiredCount);

		static void terminate();

	private:
		static void init_instance();
		static void select_physical_device();
		static void init_logical_device(VkPhysicalDeviceFeatures* inDeviceFeatures = nullptr);

	private:
		static VkInstance s_instance;
		static VkDevice s_logical_device;
		static VkPhysicalDevice s_physical_device;

		static VkQueue s_graphics_queue, s_compute_queue, s_transfer_queue, s_present_queue;
		static uint32_t s_graphics_family_index, s_compute_family_index, s_transfer_family_index, s_present_family_index;
		static bool s_compute_queue_shared_with_graphics, s_transfer_queue_shared_with_graphics, s_transfer_queue_shared_with_compute;
		static std::mutex s_graphics_queue_mutex, s_compute_queue_mutex, s_transfer_queue_mutex;

		static bool s_integrated, s_supports_buffer_device_address, s_supports_lazy_allocation;
		static uint32_t s_application_api_version, s_device_api_version;
		static std::string s_device_name;

		/* import for checks when deserializing the pipeline cache */
		static uint32_t s_vendor_id;
    	static uint32_t s_device_id;
    	static uint32_t s_driver_version;
    	static uint8_t s_pipeline_cache_uuid[VK_UUID_SIZE];

		static VkSampleCountFlagBits s_max_supported_multisample_count;
		static size_t s_min_storage_buffer_offset_alignment;
		static size_t s_min_uniform_buffer_offset_alignment;
		static size_t s_max_descriptor_samplers;
		static size_t s_max_descriptor_sampled_images;
		static float s_max_sampler_anisotropy;
		static float s_line_width_range[2];
		static bool s_supports_astc;
	};

}
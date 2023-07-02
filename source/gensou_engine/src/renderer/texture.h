#pragma once

#include "core/core.h"
#include "core/misc.h"
#include "core/uuid.h"
#include "core/engine_events.h"

#include "renderer/device.h"
#include "renderer/image.h"
#include "renderer/memory_manager.h"

#include <vulkan/vulkan.h>

namespace gs {

	struct gensou_file;

	enum class sampler_filter { linear = 0, nearest = 1, cubic = 2 };
	enum class sampler_wrap { repeat = 4, mirror = 8, clamp_edge = 16, clamp_border = 32 };

	struct sampler_info
	{
		sampler_filter filter = sampler_filter::linear;

		struct
		{
			sampler_wrap u = sampler_wrap::clamp_border;
			sampler_wrap v = sampler_wrap::clamp_border;
			
		} wrap;
	};

	class texture
	{
		static std::unordered_map<uuid, std::weak_ptr<texture>> s_textures_atlas;
		static std::unordered_map<uint32_t, VkSampler> s_sampler_atlas;

	public:
		/* may return null if any of the steps fail. always check the return value */
		static std::shared_ptr<texture> create(const std::string& path, bool mips = false, bool flipOnLoad = INVERT_VIEWPORT, sampler_info samplerInfo = {});
		static std::shared_ptr<texture> create(std::shared_ptr<image2d> image, sampler_info samplerInfo = {});

		/* raw data loaded from a compressed file (png, jpg, etc) */
		static std::shared_ptr<texture> create_from_memory(const byte* data, size_t size, bool mips, sampler_info samplerInfo = {});

		/* pixel data ready to be uploaded to a VkImage object */
		static std::shared_ptr<texture> create_from_pixels(const byte* pixels, size_t size, extent2d extent, bool mips = false, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB, sampler_info samlerInfo = {});

		static VkSampler get_sampler(sampler_filter filter, sampler_wrap wrap);
		static VkSampler get_sampler(sampler_filter filter, sampler_wrap wrap_u, sampler_wrap wrap_v);

		static void destroy_all_samplers();

	private:
		static std::shared_ptr<texture> create_from_astc(std::shared_ptr<gensou_file> file, bool mips, sampler_info samplerInfo = {});
		static std::shared_ptr<texture> create_from_ktx (std::shared_ptr<gensou_file> file, bool mips, sampler_info samplerInfo = {});
		static std::shared_ptr<texture> create_from_ktx2(std::shared_ptr<gensou_file> file, bool mips, sampler_info samplerInfo = {});

		texture(const byte* pixels, size_t size, extent2d extent, bool mips, VkFormat format, sampler_info samplerInfo = {});
		texture(std::shared_ptr<image2d> image, sampler_info samplerInfo = {});

	public:
		~texture();

		uuid get_image_id() const { return m_image->get_id(); }

		/* returns image2d class */
		std::shared_ptr<image2d> get_image2d() { return m_image; }

		void set_image(std::shared_ptr<image2d> inImage) { m_image = inImage; }

		/* returns underlying vulkan image */
		VkImage get_image() const { return m_image->get_image(); }
		VkImageView get_image_view() const { return m_image->get_image_view(); }

		uint32_t get_width() const { return m_image->get_extent().width; }
		uint32_t get_height() const { return m_image->get_extent().height; }
		extent2d get_extent() const { return m_image->get_extent(); }

		float get_aspect_ratio() const { return float(m_image->get_extent().width) / float(m_image->get_extent().height); }

		VkSampler sampler() const { return m_sampler; }

		const std::string& get_path() const { return m_path; }

	private:
		std::shared_ptr<image2d> m_image;
		VkSampler m_sampler = VK_NULL_HANDLE;

		std::string m_path;
	};

	class texture_cube
	{
	public:
		texture_cube() = default;
		texture_cube(const std::string& path, bool isFolder, bool flipOnLoad = INVERT_VIEWPORT, sampler_info samplerInfo = {});
		~texture_cube();

		void create(const std::string& cubemapFolder, bool flipOnLoad = INVERT_VIEWPORT, sampler_info samplerInfo = {});
		void create_single(const std::string& path, bool flipOnLoad = INVERT_VIEWPORT, sampler_info samplerInfo = {});

		VkSampler sampler() const { return m_sampler; }
		std::shared_ptr<image_cube> get_image_cube() const { return m_image_cube; }

	private:
		std::shared_ptr<image_cube> m_image_cube;
		VkSampler m_sampler = VK_NULL_HANDLE;

		std::string m_path;
	};

}
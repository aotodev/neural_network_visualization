#include "renderer/texture.h"

#include "renderer/command_manager.h"
#include "renderer/validation_layers.h"

#include "core/log.h"
#include "core/runtime.h"
#include "core/system.h"
#include "core/engine_events.h"

#include <stb_image.h>

#include <ktx.h>
#include <ktxvulkan.h>
#include <KHR/khr_df.h>

namespace gs {

	std::unordered_map<uuid, std::weak_ptr<texture>> texture::s_textures_atlas;
	std::unordered_map<uint32_t, VkSampler> texture::s_sampler_atlas;

	static bool is_ktx1(const byte* data)
	{
		return	data[0] == 0xAB && data[1] == 0x4B && data[2]  == 0x54 && data[3]  == 0x58 &&
				data[4] == 0x20 && data[5] == 0x31 && data[6]  == 0x31 && data[7]  == 0xBB &&
				data[8] == 0x0D && data[9] == 0x0A && data[10] == 0x1A && data[11] == 0x0A;
	}

	static bool is_ktx2(const byte* data)
	{
		return	data[0] == 0xAB && data[1] == 0x4B && data[2]  == 0x54 && data[3]  == 0x58 &&
				data[4] == 0x20 && data[5] == 0x32 && data[6]  == 0x30 && data[7]  == 0xBB &&
				data[8] == 0x0D && data[9] == 0x0A && data[10] == 0x1A && data[11] == 0x0A;
	}

	/* header declaration */
	typedef struct
	{
		byte magic[4];
		byte blockdim_x;
		byte blockdim_y;
		byte blockdim_z;
		byte xsize[3];
		byte ysize[3];
		byte zsize[3];
	} astc_header;

	static bool is_astc(const byte* astcData)
	{
		auto data = (astc_header*)astcData;
		return data->magic[0] == 0x13 && data->magic[1] == 0xAB && data->magic[2] == 0xA1 && data->magic[3] == 0x5C;
	}

	static VkFormat get_astc_format(int32_t blockX, int32_t blockY)
	{
		if (blockX == 4 && blockY == 4)
			return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;

		if (blockX == 5)
		{
			if (blockY == 4)
				return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
			if (blockY == 5)
				return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
		}

		if (blockX == 6)
		{
			if (blockY == 5)
				return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
			if (blockY == 6)
				return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
		}

		if (blockX == 8)
		{
			if (blockY == 5)
				return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
			if (blockY == 6)
				return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
			if (blockY == 8)
				return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
		}

		if (blockX == 10)
		{
			if (blockY == 5)
				return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
			if (blockY == 6)
				return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
			if (blockY == 8)
				return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
			if (blockY == 10)
				return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
		}

		if (blockX == 12)
		{
			if (blockY == 5)
				return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
			if (blockY == 6)
				return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
		}

		LOG_ENGINE(warn, "Invalid ASCT block size, returnnig ASTC_4x4_SRGB");
		return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
	}

	static VkFormat get_astc_format(const byte* astcData)
	{
		if (!is_astc(astcData))
		{
			LOG_ENGINE(error, "Not a ASTC file, returing format undefined");
			return VK_FORMAT_UNDEFINED;
		}

		astc_header* data = (astc_header*)astcData;

		/* Number of blocks in the x, y and z direction. */
		uint32_t xblocks = 0;
		uint32_t yblocks = 0;
		uint32_t zblocks = 0;

		/* Number of bytes for each dimension. */
		uint32_t xsize = 0;
		uint32_t ysize = 0;
		uint32_t zsize = 0;

		/* Merge x,y,z-sizes from 3 chars into one integer value. */
		xsize = data->xsize[0] + (data->xsize[1] << 8) + (data->xsize[2] << 16);
		ysize = data->ysize[0] + (data->ysize[1] << 8) + (data->ysize[2] << 16);
		zsize = data->zsize[0] + (data->zsize[1] << 8) + (data->zsize[2] << 16);
		/* Compute number of blocks in each direction. */
		xblocks = (xsize + data->blockdim_x - 1) / data->blockdim_x;
		yblocks = (ysize + data->blockdim_y - 1) / data->blockdim_y;
		zblocks = (zsize + data->blockdim_z - 1) / data->blockdim_z;

		return get_astc_format(xblocks, yblocks);
	}

	std::shared_ptr<texture> texture::create_from_astc(std::shared_ptr<gensou_file> file, bool mips, sampler_info samplerInfo)
	{
		std::shared_ptr<texture> outTexture;

		if (!device::supports_astc_format())
		{
			// TODO: uncompress uast/ASTC
			LOG_ENGINE(error, "Tried to load an ASTC compressed texture, but this device does not support it");
			return outTexture;
		}

		/* From ARM-SOFTWARE */

		/* Number of blocks in the x, y and z direction. */
		uint32_t xblocks = 0, yblocks = 0, zblocks = 0;

		/* Number of bytes for each dimension. */
		uint32_t xsize = 0, ysize = 0, zsize = 0;

		astc_header* astcData = (astc_header*)file->data();

		/* Merge x,y,z-sizes from 3 chars into one integer value. */
		xsize = astcData->xsize[0] + (astcData->xsize[1] << 8) + (astcData->xsize[2] << 16);
		ysize = astcData->ysize[0] + (astcData->ysize[1] << 8) + (astcData->ysize[2] << 16);
		zsize = astcData->zsize[0] + (astcData->zsize[1] << 8) + (astcData->zsize[2] << 16);

		/* Compute number of blocks in each direction. */
		xblocks = (xsize + astcData->blockdim_x - 1) / astcData->blockdim_x;
		yblocks = (ysize + astcData->blockdim_y - 1) / astcData->blockdim_y;
		zblocks = (zsize + astcData->blockdim_z - 1) / astcData->blockdim_z;

		/* Each block is encoded on 16 bytes, so calculate total compressed image data size. */
		size_t totalSize = (size_t)(xblocks * yblocks * zblocks << 4);

		outTexture = std::shared_ptr<texture>(new texture(file->data(), totalSize, extent2d(xsize, ysize), mips, get_astc_format(xblocks, yblocks), samplerInfo));

        return outTexture;
	}

	std::shared_ptr<texture> texture::create(const std::string& path, bool mips, bool flipOnLoad, sampler_info samplerInfo)
	{
		if (auto id = system::get_cached_id_from_file(path))
		{
			const auto mapIterator = s_textures_atlas.find(id);
			if (mapIterator != s_textures_atlas.end())
			{
				auto textureWeakPtr = mapIterator->second;
				if (!textureWeakPtr.expired())
				{
					LOG_ENGINE(trace, "texture with path '%s' found", path.c_str());
					return std::shared_ptr<texture>(textureWeakPtr);
				}
			}
		}

		std::shared_ptr<texture> outTexture;
		auto data = system::load_file(path);

		if (!data)
			return outTexture;

		if (is_astc(data->data()))
		{
			LOG_ENGINE(trace, "Loaded ASTC texture");
			outTexture = create_from_astc(data, mips, samplerInfo);
		}
		else if(is_ktx1(data->data()))
		{
			LOG_ENGINE(trace, "Loaded KTX texture");
			outTexture = create_from_ktx(data, mips, samplerInfo);
		}
		else if(is_ktx2(data->data()))
		{
			LOG_ENGINE(trace, "Loaded KTX2 texture");
			outTexture = create_from_ktx2(data, mips, samplerInfo);
		}
		else /* other (png, jpeg, etc)  */
		{
			outTexture = create_from_memory(data->data(), data->size(), mips, samplerInfo);
		}

		if (outTexture)
		{
			outTexture->m_image->m_id = data->id();
			LOG_ENGINE(trace, "adding texture from path [%s] and id 0x%xll to the textures atlas", path.c_str(), outTexture->get_image_id());

			s_textures_atlas[outTexture->get_image_id()] = std::weak_ptr<texture>(outTexture);
			outTexture->m_path = path;
		}
		else
		{
			LOG_ENGINE(error, "Failed to load texture from path '%s', returning an empty texture", path.c_str());
		}

		return outTexture;
	}

	std::shared_ptr<texture> texture::create(std::shared_ptr<image2d> image, sampler_info samplerInfo)
	{
		return std::shared_ptr<texture>(new texture(image, samplerInfo));
	}

	std::shared_ptr<texture> texture::create_from_memory(const byte* data, size_t size, bool mips, sampler_info samplerInfo)
	{
		int width, height, channels;
		byte* pixels = nullptr;
		size_t imageSize = 0;
		VkFormat format = VK_FORMAT_UNDEFINED;

		if (stbi_is_hdr_from_memory(data, (int)size))
		{
			LOG_ENGINE(trace, "HDR texture");

			if (mips)
			{
				format = device::get_hdr_linear_sample_blitt_format();

				if (format == VK_FORMAT_UNDEFINED || format == VK_FORMAT_R8G8B8A8_UNORM)
				{
					LOG_ENGINE(warn, "no HDR format suport with blitt found. It will not be possible to generate mips for this texture");
					format = device::get_hdr_linear_sample_format();
				}
			}
			else
			{
				format = device::get_hdr_linear_sample_format();
			}

			/* supports HDR */
			if (format == VK_FORMAT_R32G32B32A32_SFLOAT || format == VK_FORMAT_R16G16B16A16_SFLOAT) 
			{
				pixels = (byte*)stbi_loadf_from_memory(data, (int)size, &width, &height, &channels, 4);
				imageSize = width * height * 4 * sizeof(float);
			}
			else
			{
				LOG_ENGINE(warn, "Loaded an HDR texture, but no HDR sampled format is supported by this device");

				pixels = stbi_load_from_memory(data, (int)size, &width, &height, &channels, 4);
				imageSize = (size_t)width * (size_t)height * 4ull;
				format = mips ? device::get_color_blitt_format(VK_FORMAT_R8G8B8A8_SRGB) : VK_FORMAT_R8G8B8A8_SRGB;
			}
		}
		else
		{
			pixels = stbi_load_from_memory(data, (int)size, &width, &height, &channels, 4);
			imageSize = (size_t)width * (size_t)height * 4ull;
			format = mips ? device::get_color_blitt_format(VK_FORMAT_R8G8B8A8_SRGB) : VK_FORMAT_R8G8B8A8_SRGB;
		}

		std::shared_ptr<texture> outTexture = create_from_pixels(pixels, imageSize, extent2d(width, height), mips, format, samplerInfo);
		stbi_image_free(pixels);

		return outTexture;
	}

	std::shared_ptr<texture> texture::create_from_pixels(const byte* pixels, size_t size, extent2d extent, bool mips, VkFormat format, sampler_info samplerInfo)
	{
		return std::shared_ptr<texture>(new texture(pixels, size, extent, mips, format, samplerInfo));
	}

	std::shared_ptr<texture> texture::create_from_ktx(std::shared_ptr<gensou_file> file, bool mips, sampler_info samplerInfo)
	{
		std::shared_ptr<texture> outTexture;
		ktxTexture* kTexture = nullptr;

		ktxResult result = ktxTexture_CreateFromMemory(static_cast<const ktx_uint8_t*>(file->data()), file->size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);
		if (result != KTX_SUCCESS)
		{
			LOG_ENGINE(error, "failed to load KTX with the following result: '%s'", ktxErrorString(result));
			return outTexture;
		}

		auto format = ktxTexture_GetVkFormat(kTexture);

		auto image = std::make_shared<image2d>(kTexture, format == VK_FORMAT_UNDEFINED ? VK_FORMAT_R8G8B8A8_SRGB : format, mips);
		outTexture = std::shared_ptr<texture>(new texture(image, samplerInfo));

		if(kTexture)
			ktxTexture_Destroy(kTexture);

		return outTexture;
	}

	std::shared_ptr<texture> texture::create_from_ktx2(std::shared_ptr<gensou_file> file, bool mips, sampler_info samplerInfo)
	{
		std::shared_ptr<texture> outTexture;

		{
			ktxTexture2* k2Texture = nullptr;

			ktxResult result = ktxTexture2_CreateFromMemory(static_cast<const ktx_uint8_t*>(file->data()), file->size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &k2Texture);
			if (result != KTX_SUCCESS)
			{
				LOG_ENGINE(error, "failed to load KTX with the following result: '%s'", ktxErrorString(result));
				return outTexture;
			}

			bool astc = device::supports_astc_format() && (bool)USE_ASTC;
			if (k2Texture->isCompressed)
			{
				LOG_ENGINE(trace, "transcoding ktx2 texture");

				auto transcodeResult = ktxTexture2_TranscodeBasis(k2Texture, astc ? KTX_TTF_ASTC_4x4_RGBA : KTX_TTF_BC7_RGBA, KTX_TF_HIGH_QUALITY);
				assert(transcodeResult == KTX_SUCCESS);
			}

			VkFormat kFormat = ktxTexture2_GetVkFormat(k2Texture);

			auto image = std::make_shared<image2d>(ktxTexture(k2Texture), kFormat, mips);
			outTexture = std::shared_ptr<texture>(new texture(image, samplerInfo));

			if(k2Texture)
				ktxTexture_Destroy(ktxTexture(k2Texture));

		}

		return outTexture;
	}

	texture::texture(const byte* pixels, size_t size, extent2d extent, bool mips, VkFormat format, sampler_info samplerInfo)
		: m_image(std::make_shared<image2d>(pixels, size, extent, format, mips))
	{
		if(!m_sampler)
			m_sampler = get_sampler(samplerInfo.filter, samplerInfo.wrap.u, samplerInfo.wrap.v);
	}

	texture::texture(std::shared_ptr<image2d> image, sampler_info samplerInfo)
		: m_image(image)
	{
		if(!m_sampler)
			m_sampler = get_sampler(samplerInfo.filter, samplerInfo.wrap.u, samplerInfo.wrap.v);
	}

	texture::~texture()
	{
		if (!m_image)
			return;

		if (auto id = m_image->get_id())
		{
			m_image.reset();

			const auto mapIterator = s_textures_atlas.find(id);
			if(mapIterator != s_textures_atlas.end())
			{
				if(mapIterator->second.use_count() == 0)
				{
					s_textures_atlas.erase(id);
            		LOG_ENGINE(trace, "Erasing texture from map with id 0x%llX", id);
				}
			}
		}

		if (!m_path.empty())
		{
        	LOG_ENGINE(trace, "destroyed texture with path %s", m_path.c_str());
		}
	}

	//--------------------------------------------------------------------------------------------------------------
	// TEXTURE CUBE
	//--------------------------------------------------------------------------------------------------------------

	texture_cube::texture_cube(const std::string& path, bool isFolder, bool flipOnLoad, sampler_info samplerInfo)
	{
		if(isFolder)
			create(path, flipOnLoad, samplerInfo);
		else
			create_single(path, flipOnLoad, samplerInfo);
	}
	texture_cube::~texture_cube()
	{
		m_image_cube.reset();

		if (m_sampler)
			vkDestroySampler(device::get_logical(), m_sampler, nullptr);
	}

	void texture_cube::create(const std::string& cubemapFolder, bool flipOnLoad, sampler_info samplerInfo)
	{
		int width, height, channels;
		size_t size = 0;
		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
		m_path = cubemapFolder;

		std::string paths[] =
		{
			std::string(cubemapFolder + "/right.jpg"),
			std::string(cubemapFolder + "/left.jpg"),
			std::string(cubemapFolder + "/top.jpg"),
			std::string(cubemapFolder + "/bottom.jpg"),
			std::string(cubemapFolder + "/front.jpg"),
			std::string(cubemapFolder + "/back.jpg")
		};

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VmaAllocation stagingbufferMemory = VK_NULL_HANDLE;
		void* bufferLocation = nullptr;
		size_t offset = 0;

		for (uint32_t i = 0; i < 6; i++)
		{
			byte* pixels = nullptr;

			if (stbi_is_hdr(paths[i].c_str()))
			{
				pixels = (byte*)stbi_loadf(paths[i].c_str(), &width, &height, &channels, 4);
				size = width * height * 4 * sizeof(float);
				format = VK_FORMAT_R32G32B32A32_SFLOAT;
				LOG_ENGINE(info, "HDR image");
			}
			else
			{
				pixels = stbi_load(paths[i].c_str(), &width, &height, &channels, 4);
				size = (size_t)width * (size_t)height * 4ull;
			}

			if (pixels == nullptr)
			{
				LOG_ENGINE(error, "failed to load image form path '%s'", paths[i].c_str());
				assert(false);
			}

			if (stagingBuffer == VK_NULL_HANDLE)
			{
				VkBufferCreateInfo stagingBufferCreateInfo{};
				stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				stagingBufferCreateInfo.size = size * 6ull;
				stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				stagingbufferMemory = memory_manager::create_buffer(stagingBufferCreateInfo, &stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
				memory_manager::map(&bufferLocation, stagingbufferMemory);
			}

			byte* dstData = (byte*)bufferLocation;
			dstData += offset;
			memcpy((void*)dstData, pixels, (size_t)size);

			stbi_image_free(pixels);

			offset += size;
		}

		m_image_cube = std::make_shared<image_cube>();
		m_image_cube->create(stagingBuffer, format, width, height);

		memory_manager::unmap(stagingbufferMemory);
		memory_manager::destroy_buffer(stagingBuffer, stagingbufferMemory);

		if(!m_sampler)
			m_sampler = texture::get_sampler(samplerInfo.filter, samplerInfo.wrap.u);

		LOG_ENGINE(trace, "Created Texture Cube");
	}

	void texture_cube::create_single(const std::string& path, bool flipOnLoad, sampler_info samplerInfo)
	{
		int width, height, channels;
		size_t size = 0;
		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
		m_path = path;

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VmaAllocation stagingbufferMemory = VK_NULL_HANDLE;
		void* bufferLocation = nullptr;

		size_t offset = 0;

		byte* pixels = nullptr;

		if (stbi_is_hdr(path.c_str()))
		{
			pixels = (byte*)stbi_loadf(path.c_str(), &width, &height, &channels, 4);
			size = width * height * 4 * sizeof(float);
			format = VK_FORMAT_R32G32B32A32_SFLOAT;
			LOG_ENGINE(info, "HDR image");
		}
		else
		{
			pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
			size = (size_t)width * (size_t)height * 4ull;
		}

		if (pixels == nullptr)
		{
			LOG_ENGINE(error, "failed to load image form path '%s'", path.c_str());
			return;
		}

		VkBufferCreateInfo stagingBufferCreateInfo{};
		stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferCreateInfo.size = size * 6ull;
		stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		stagingbufferMemory = memory_manager::create_buffer(stagingBufferCreateInfo, &stagingBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
		memory_manager::map(&bufferLocation, stagingbufferMemory);

		/* copy the same texture 6 times */
		for (uint32_t i = 0; i < 6; i++)
		{
			byte* dstData = (byte*)bufferLocation;
			dstData += offset;
			memcpy((void*)dstData, pixels, (size_t)size);

			offset += size;
		}

		stbi_image_free(pixels);

		m_image_cube = std::make_shared<image_cube>();
		m_image_cube->create(stagingBuffer, format, width, height);

		memory_manager::unmap(stagingbufferMemory);
		memory_manager::destroy_buffer(stagingBuffer, stagingbufferMemory);

		if(!m_sampler)
			m_sampler = texture::get_sampler(samplerInfo.filter, samplerInfo.wrap.u);

		LOG_ENGINE(trace, "Created Texture Cube");		
	}

	static inline VkSamplerAddressMode get_vulkan_sampler_mode(sampler_wrap wrap)
	{
		switch (wrap)
		{
		case sampler_wrap::repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case sampler_wrap::mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case sampler_wrap::clamp_edge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case sampler_wrap::clamp_border: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		}

		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}

	VkSampler texture::get_sampler(sampler_filter filter, sampler_wrap wrap)
	{
		return get_sampler(filter, wrap, wrap);
	}

	VkSampler texture::get_sampler(sampler_filter filter, sampler_wrap wrap_u, sampler_wrap wrap_v)
	{
		enum
		{
			linear_repeat = (uint32_t)sampler_filter::linear | (uint32_t)sampler_wrap::repeat,
			linear_mirror = (uint32_t)sampler_filter::linear | (uint32_t)sampler_wrap::mirror,
			linear_clamp_edge = (uint32_t)sampler_filter::linear | (uint32_t)sampler_wrap::clamp_edge,
			linear_clamp_border = (uint32_t)sampler_filter::linear | (uint32_t)sampler_wrap::clamp_border,

			nearest_repeat = (uint32_t)sampler_filter::nearest | (uint32_t)sampler_wrap::repeat,
			nearest_mirror = (uint32_t)sampler_filter::nearest | (uint32_t)sampler_wrap::mirror,
			nearest_clamp_edge = (uint32_t)sampler_filter::nearest | (uint32_t)sampler_wrap::clamp_edge,
			nearest_clamp_border = (uint32_t)sampler_filter::nearest | (uint32_t)sampler_wrap::clamp_border,
		};

		uint32_t samplerType = (uint32_t)filter | ((uint32_t)wrap_u | (uint32_t)wrap_v);

		const auto mapIterator = s_sampler_atlas.find(samplerType);
		if (mapIterator != s_sampler_atlas.end())
		{
			return mapIterator->second;
		}

		/*-----create-new-sampler-------------------------------------------*/
		static uint32_t count = 0;

		VkSampler outSampler = VK_NULL_HANDLE;

		VkFilter samplerFilter = VK_FILTER_LINEAR;
		switch (filter)
		{
		case sampler_filter::linear: samplerFilter = VK_FILTER_LINEAR; break;
		case sampler_filter::nearest: samplerFilter = VK_FILTER_NEAREST; break;
		case sampler_filter::cubic: samplerFilter = VK_FILTER_CUBIC_EXT; break;
		}

		VkSamplerAddressMode samplerMode_u = get_vulkan_sampler_mode(wrap_u);
		VkSamplerAddressMode samplerMode_v = get_vulkan_sampler_mode(wrap_v);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = samplerFilter;
		samplerInfo.minFilter = samplerFilter;
		samplerInfo.addressModeU = samplerMode_u;
		samplerInfo.addressModeV = samplerMode_v;
		samplerInfo.addressModeW = samplerMode_u;

		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = samplerFilter == VK_FILTER_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 13.0f; /* enough for 4k image */

		#if ENABLE_ANISOTROPY
		if (samplerFilter != VK_FILTER_CUBIC_EXT)
		{
			samplerInfo.maxAnisotropy = device::max_sampler_anisotropy();
			samplerInfo.anisotropyEnable = samplerInfo.maxAnisotropy > 1.0f ? VK_TRUE : VK_FALSE;
		}
		#else
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.anisotropyEnable = VK_FALSE;
		#endif

		VkResult samplerCreation = vkCreateSampler(device::get_logical(), &samplerInfo, nullptr, &outSampler);
		if (samplerCreation != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(samplerCreation, "Could not create texture sampler");

		count++;

		LOG_ENGINE(info, "creating sampler, total created == %u", count);

		s_sampler_atlas.emplace(samplerType, outSampler);

		return outSampler;
	}

	void texture::destroy_all_samplers()
	{
		for (auto& [type, sampler] : s_sampler_atlas)
		{
			if (sampler)
				vkDestroySampler(device::get_logical(), sampler, nullptr);

			sampler = VK_NULL_HANDLE;
		}

		s_sampler_atlas.clear();
	}
}
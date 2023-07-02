#include "renderer/descriptor_set.h"
#include "renderer/memory_manager.h"
#include "renderer/validation_layers.h"
#include "renderer/texture.h"

#include "core/log.h"
#include "core/engine_events.h"

#include "renderer/renderer.h"

namespace gs {

	descriptor_set::~descriptor_set()
	{
		if (m_layout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(device::get_logical(), m_layout, nullptr);
	}

	descriptor_set::descriptor_set(const descriptor_set& other)
		: m_layout_bindings(other.m_layout_bindings)
	{
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = (uint32_t)m_layout_bindings.size();
		layoutInfo.pBindings = m_layout_bindings.data();

		VkResult createDescriptorSetLayout = vkCreateDescriptorSetLayout(device::get_logical(), &layoutInfo, nullptr, &m_layout);
		if (createDescriptorSetLayout != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(createDescriptorSetLayout, "Could not create Descriptor set layout");

		LOG_ENGINE(trace, "copied descriptor set layout");

		m_descriptor_set = memory_manager::allocate_descriptor_set(m_layout);

		LOG_ENGINE(trace, "allocated descriptor set from copy");
	}

	descriptor_set::descriptor_set(descriptor_set&& other) noexcept
		: m_descriptor_set(other.m_descriptor_set), m_layout(other.m_layout), m_layout_bindings(std::move(other.m_layout_bindings))
	{
		other.m_descriptor_set = VK_NULL_HANDLE;
		other.m_layout = VK_NULL_HANDLE;

		/* probaly not necessary, since the vector has already been moved */
		other.m_layout_bindings.clear();
		other.m_layout_bindings.shrink_to_fit();
	}

	descriptor_set& descriptor_set::operator=(const descriptor_set& other)
	{
		m_layout_bindings = other.m_layout_bindings;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = (uint32_t)m_layout_bindings.size();
		layoutInfo.pBindings = m_layout_bindings.data();

		VkResult createDescriptorSetLayout = vkCreateDescriptorSetLayout(device::get_logical(), &layoutInfo, nullptr, &m_layout);
		if (createDescriptorSetLayout != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(createDescriptorSetLayout, "Could not create Descriptor set layout");

		LOG_ENGINE(trace, "copied descriptor set layout");

		m_descriptor_set = memory_manager::allocate_descriptor_set(m_layout);

		LOG_ENGINE(trace, "allocated descriptor set from copy");

		return *this;
	}

	descriptor_set& descriptor_set::operator=(descriptor_set&& other) noexcept
	{
		m_descriptor_set = other.m_descriptor_set;
		m_layout = other.m_layout;
		m_layout_bindings = std::move(other.m_layout_bindings);

		other.m_descriptor_set = VK_NULL_HANDLE;
		other.m_layout = VK_NULL_HANDLE;

		/* probaly not necessary, since the vector has already been moved */
		other.m_layout_bindings.clear();
		other.m_layout_bindings.shrink_to_fit();

		return *this;
	}

	void descriptor_set::create(VkDescriptorType type, VkShaderStageFlags stage, uint32_t count, const VkSampler* sampler)
	{
		create({ { 0, type, count, stage, sampler } });
	}

	void descriptor_set::create(std::initializer_list<VkDescriptorSetLayoutBinding> inLayoutBindings)
	{
		if (m_layout != VK_NULL_HANDLE)
		{
			LOG_ENGINE(warn, "overriding descriptor set");
			vkDestroyDescriptorSetLayout(device::get_logical(), m_layout, nullptr);
		}

		if(!m_layout_bindings.empty())
			m_layout_bindings.clear();

		m_layout_bindings.reserve(inLayoutBindings.size());

		for(const auto& binding : inLayoutBindings)
			m_layout_bindings.emplace_back(binding);

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = (uint32_t)inLayoutBindings.size();
		layoutInfo.pBindings = inLayoutBindings.begin();

		VkResult createDescriptorSetLayout = vkCreateDescriptorSetLayout(device::get_logical(), &layoutInfo, nullptr, &m_layout);
		if (createDescriptorSetLayout != VK_SUCCESS)
			engine_events::vulkan_result_error.broadcast(createDescriptorSetLayout, "Could not create descriptor set layout");

		LOG_ENGINE(trace, "created descriptor set layout");

		m_descriptor_set = memory_manager::allocate_descriptor_set(m_layout);

		LOG_ENGINE(trace, "Allocated descriptor set");	
	}

	////////////////////////////////////////////////
	////////////////////////////////////////////////

	#include "renderer/texture.h"

	std::shared_ptr<texture> texture_batch_descriptor::s_base_texture;

	texture_batch_descriptor::texture_batch_descriptor()
	{
		if(!s_base_texture)
		{
			s_base_texture = texture::create("engine_res/textures/white.gsasset");
			engine_events::terminate_renderer.subscribe(&texture_batch_descriptor::on_renderer_terminate);
		}

		m_descriptor_set.create(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, m_max_texture_slots);

		clear();
	}

	void texture_batch_descriptor::clear()
	{
		if (!m_bound_textures_map.empty())
			m_bound_textures_map.clear();

		VkDescriptorImageInfo info;
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = s_base_texture->get_image_view();
		info.sampler = s_base_texture->sampler();

		for (size_t i = 0; i < m_max_texture_slots; i++)
			m_descriptor_set.update(0, &info, 1, i);

		m_bound_textures_count = 1;
	}

	uint32_t texture_batch_descriptor::get_texture_id(std::shared_ptr<texture> inTexture)
	{
		if (!inTexture)
			return 0;

		uint32_t textureIndex = 0;
		const auto id = inTexture->get_image_id();

		const auto mapIterator = m_bound_textures_map.find(id);
		if (mapIterator != m_bound_textures_map.end())
		{
			textureIndex = mapIterator->second.index;
		}
		else
		{
			textureIndex = m_bound_textures_count++;
			m_bound_textures_map.emplace(id, data{ inTexture, textureIndex });

			renderer::submit_pre_render_cmd([this, inTexture, textureIndex] ()
			{
				VkDescriptorImageInfo imageInfo{ inTexture->sampler(), inTexture->get_image_view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
				m_descriptor_set.update(0, &imageInfo, 1, textureIndex);
			});
		}

		return textureIndex;
	}

}
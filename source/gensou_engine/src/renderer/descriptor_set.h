#pragma once

#include "core/core.h"
#include "core/uuid.h"
#include "renderer/device.h"

#include <vulkan/vulkan.h>

namespace gs {

	class descriptor_set
	{
	public:
		descriptor_set() = default;
		descriptor_set(VkDescriptorSet set, VkDescriptorSetLayout layout) : m_descriptor_set(set), m_layout(layout) {}
		~descriptor_set();

		descriptor_set(const descriptor_set&);
		descriptor_set& operator=(const descriptor_set&);

		descriptor_set(descriptor_set&&) noexcept;
		descriptor_set& operator=(descriptor_set&&) noexcept;

		uint32_t get_count() const { return (uint32_t)m_layout_bindings.size(); }

		void create(std::initializer_list<VkDescriptorSetLayoutBinding> layoutBindings);
		// for descriptors with a single binding
		void create(VkDescriptorType type, VkShaderStageFlags stage, uint32_t count = 1, const VkSampler* sampler = nullptr);

		FORCEINLINE void update(VkWriteDescriptorSet write)
		{
			assert(m_descriptor_set != VK_NULL_HANDLE);
			vkUpdateDescriptorSets(device::get_logical(), 1, &write, 0, nullptr);
		}

		FORCEINLINE void update(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t count, uint32_t dstArrayElement = 0)
		{
			assert(m_descriptor_set != VK_NULL_HANDLE);

			VkWriteDescriptorSet write{};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = m_descriptor_set;
			write.dstBinding = binding;
			write.dstArrayElement = dstArrayElement;
			write.descriptorType = m_layout_bindings[binding].descriptorType;
			write.descriptorCount = count;
			write.pImageInfo = nullptr;
			write.pBufferInfo = bufferInfo;

			vkUpdateDescriptorSets(device::get_logical(), 1, &write, 0, nullptr);
		}

		void update(uint32_t binding, VkDescriptorImageInfo* pInfo, uint32_t count, uint32_t dstArrayElement = 0)
		{
			VkWriteDescriptorSet write{};

			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = m_descriptor_set;
			write.dstBinding = binding;
			write.dstArrayElement = dstArrayElement;
			write.descriptorType = m_layout_bindings[binding].descriptorType;
			write.descriptorCount = count;
			write.pImageInfo = pInfo;
			write.pBufferInfo = nullptr;

			vkUpdateDescriptorSets(device::get_logical(), 1, &write, 0, nullptr);
		}

		VkDescriptorSet get() const { return m_descriptor_set; }
		VkDescriptorSetLayout get_layout() const { return m_layout; }

	private:
		VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;

		std::vector<VkDescriptorSetLayoutBinding> m_layout_bindings;
	};

	/////////////////////////////////////
	/////////////////////////////////////

	class texture_batch_descriptor
	{
	public:
		texture_batch_descriptor();

		uint32_t get_texture_id(std::shared_ptr<class texture> inTexture);
		static constexpr uint32_t get_white_texture_id() { return 0; }

		descriptor_set& get_descriptor() { return m_descriptor_set; }

		/* clear all resources except for the base white texture */
		void clear();

	private:
		/* since we bind images, we must keep a strong ref to the texture as well */
		struct data { std::shared_ptr<class texture> texture_ref; uint32_t index = 0; };

		descriptor_set m_descriptor_set;
		std::unordered_map<uuid, data> m_bound_textures_map;
		uint32_t m_bound_textures_count = 0;
		uint32_t m_max_texture_slots = 16;

		/* white texture for color, also serving as a place-holder for unused slots */
		static std::shared_ptr<class texture> s_base_texture;
		static void on_renderer_terminate() { s_base_texture.reset(); }
	};

}
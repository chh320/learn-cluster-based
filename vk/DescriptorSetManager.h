#pragma once
#include "VkConfig.h"

namespace Vk {
	class DescriptorSetManager final {
	public:
		DescriptorSetManager(VkDevice device);
		~DescriptorSetManager();

		void CreateBindlessLayout(VkDevice device, VkDescriptorType type, VkDescriptorSetLayout& layout);
		void CreateBindlessSet(VkDevice device, VkDescriptorType type, VkDescriptorPool& pool, VkDescriptorSetLayout& layout, VkDescriptorSet& descriptorSet);

		const VkDescriptorSet& GetBindlessBufferSet() const { return _bindlessBufferSet; }
		const VkDescriptorSet& GetBindlessImageSet() const { return _bindlessImageSet; }
		const VkDescriptorSetLayout& GetBindlessBufferLayout() const { return _bindlessBufferLayout; }
		const VkDescriptorSetLayout& GetBindlessImageLayout() const { return _bindlessImageLayout; }

	private:
		VkDevice _device;
		VkDescriptorPool _imageDescriptorPool;
		VkDescriptorPool _bufferDescriptorPool;

		VkDescriptorSet _bindlessBufferSet;
		VkDescriptorSet _bindlessImageSet;

		VkDescriptorSetLayout _bindlessBufferLayout;
		VkDescriptorSetLayout _bindlessImageLayout;
	};
}
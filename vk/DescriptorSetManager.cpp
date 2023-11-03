#include "DescriptorSetManager.h"
#include <iostream>

namespace Vk {
	DescriptorSetManager::DescriptorSetManager(VkDevice device) : _device(device){
		CreateBindlessLayout(device, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _bindlessBufferLayout);
		CreateBindlessLayout(device, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _bindlessImageLayout);
		CreateBindlessSet(device, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _bufferDescriptorPool, _bindlessBufferLayout, _bindlessBufferSet);
		CreateBindlessSet(device, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _imageDescriptorPool, _bindlessImageLayout, _bindlessImageSet);
	}

	DescriptorSetManager::~DescriptorSetManager() {
		vkDestroyDescriptorPool(_device, _imageDescriptorPool, nullptr);
		vkDestroyDescriptorPool(_device, _bufferDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(_device, _bindlessBufferLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _bindlessImageLayout, nullptr);
	}

	void DescriptorSetManager::CreateBindlessLayout(VkDevice device, VkDescriptorType type, VkDescriptorSetLayout& layout) {
		VkDescriptorSetLayoutBinding layoutBinding{};
		layoutBinding.binding = 0;
		layoutBinding.descriptorCount = (1 << 20);
		layoutBinding.descriptorType = type;
		layoutBinding.stageFlags = VK_SHADER_STAGE_ALL;

		VkDescriptorBindingFlags flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlag{};
		bindingFlag.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		bindingFlag.bindingCount = 1;
		bindingFlag.pBindingFlags = &flag;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &layoutBinding;
		layoutInfo.pNext = &bindingFlag;

		Check(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout), "create descriptor layout.");
	}

	void DescriptorSetManager::CreateBindlessSet(VkDevice device, VkDescriptorType type, VkDescriptorPool& pool, VkDescriptorSetLayout& layout, VkDescriptorSet& descriptorSet) {
		VkDescriptorPoolSize poolSize{};
		poolSize.type = type;
		poolSize.descriptorCount = (1 << 20);

		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.maxSets = 1;
		descriptorPoolInfo.poolSizeCount = 1;
		descriptorPoolInfo.pPoolSizes = &poolSize;

		Check(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &pool), "create descriptor pool.");

		uint32_t num = (1 << 20);
		VkDescriptorSetVariableDescriptorCountAllocateInfo descriptorSetExt{};
		descriptorSetExt.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
		descriptorSetExt.descriptorSetCount = 1;
		descriptorSetExt.pDescriptorCounts = &num;

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout;
		allocInfo.pNext = &descriptorSetExt;

		Check(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet),
			"allocate descriptor sets");
	}
}
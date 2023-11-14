#pragma once

#include "VkConfig.h"
#include <iostream>

namespace Vk {
	class Buffer final {
	public:
		Buffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage) :_allocator(allocator), _size(size){
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = bufferUsage;

			VmaAllocationCreateInfo allocationInfo{};
			allocationInfo.usage = memoryUsage;

			Check(vmaCreateBuffer(_allocator, &bufferInfo, &allocationInfo, &_buffer, &_allocation, nullptr), "create buffer");
		}

		~Buffer()
		{
			vmaDestroyBuffer(_allocator, _buffer, _allocation);
		}

		void Update(void* p, uint32_t size) {
			void* data;
			vmaMapMemory(_allocator, _allocation, &data);
			memcpy(data, p, size);
			vmaUnmapMemory(_allocator, _allocation);
		}

		static void UpdateDescriptorSets(const std::vector<Buffer*>& bufferData, VkDevice device, VkDescriptorSet descriptorSet, uint32_t dstArrayElement) {
			std::vector<VkDescriptorBufferInfo> bufferInfos(bufferData.size());
			uint32_t i = 0;
			for (auto& bufferInfo : bufferInfos) {
				bufferInfo.buffer = bufferData[i]->GetBuffer();
				bufferInfo.offset = 0;
				bufferInfo.range = bufferData[i]->GetSize();
				i++;
			}

			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = descriptorSet;
			writeDescriptorSet.dstBinding = 0;
			writeDescriptorSet.dstArrayElement = dstArrayElement;
			writeDescriptorSet.descriptorCount = bufferData.size();
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writeDescriptorSet.pBufferInfo = bufferInfos.data();
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}


		VkBuffer& GetBuffer() { return _buffer; }
		uint32_t GetSize() { return _size; }

	private:
		VmaAllocator _allocator;
		uint32_t _size;
		VmaAllocation _allocation;
		VkBuffer _buffer;
	};
}
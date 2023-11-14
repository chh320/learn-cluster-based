#pragma once
#include "VkConfig.h"
#include "Image.h"
#include "Buffer.h"
#include <vector>
#include <iostream>

namespace Vk {
	struct ImageBarrier {
		VkImage image;
		VkPipelineStageFlags2 srcStage;
		VkAccessFlags2 srcAccess;
		VkPipelineStageFlags2 dstStage;
		VkAccessFlags2 dstAccess;
		VkImageLayout oldLayout;
		VkImageLayout newLayout;
		VkImageAspectFlags aspectMask;
		uint32_t baseMipLevel;

		ImageBarrier(VkImage& image,
			VkPipelineStageFlags2 srcStage,
			VkAccessFlags2 srcAccess,
			VkPipelineStageFlags2 dstStage,
			VkAccessFlags2 dstAccess,
			VkImageLayout oldLayout,
			VkImageLayout newLayout,
			VkImageAspectFlags aspectMask,
			uint32_t baseMipLevel = 0
		) : image(image), srcStage(srcStage) , srcAccess(srcAccess) , dstStage(dstStage) , dstAccess(dstAccess), oldLayout(oldLayout), newLayout(newLayout), aspectMask(aspectMask), baseMipLevel(baseMipLevel){}
	};

	struct BufferBarrier {
		VkBuffer buffer;
		uint32_t size;
		VkPipelineStageFlags2 srcStage;
		VkAccessFlags2 srcAccess;
		VkPipelineStageFlags2 dstStage;
		VkAccessFlags2 dstAccess;

		BufferBarrier(
			VkBuffer buffer,
			uint32_t size,
			VkPipelineStageFlags2 srcStage,
			VkAccessFlags2 srcAccess,
			VkPipelineStageFlags2 dstStage,
			VkAccessFlags2 dstAccess
		) : buffer(buffer), size(size), srcStage(srcStage), srcAccess(srcAccess), dstStage(dstStage), dstAccess(dstAccess) {}
	};

	class Barrier final {
	public:
		static void PipelineBarrier(VkCommandBuffer cmd, const std::vector<ImageBarrier>& imageBarriers, const std::vector<BufferBarrier>& bufferBarriers) {
			std::vector<VkImageMemoryBarrier2> ImageMemoryBarriers;
			for (auto barrier : imageBarriers) {
				VkImageMemoryBarrier2 imageBarrier{};
				imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
				imageBarrier.srcStageMask = barrier.srcStage;
				imageBarrier.srcAccessMask = barrier.srcAccess;
				imageBarrier.dstStageMask = barrier.dstStage;
				imageBarrier.dstAccessMask = barrier.dstAccess;
				imageBarrier.oldLayout = barrier.oldLayout;
				imageBarrier.newLayout = barrier.newLayout;
				imageBarrier.image = barrier.image;
				imageBarrier.subresourceRange.aspectMask = barrier.aspectMask;
				imageBarrier.subresourceRange.levelCount = 1; 
				imageBarrier.subresourceRange.layerCount = 1;
				imageBarrier.subresourceRange.baseMipLevel = barrier.baseMipLevel;
				
				ImageMemoryBarriers.push_back(imageBarrier);
			}

			std::vector<VkBufferMemoryBarrier2> bufferMemoryBarriers;
			for (auto barrier : bufferBarriers) {
				VkBufferMemoryBarrier2 bufferBarrier{};
				bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
				bufferBarrier.srcStageMask = barrier.srcStage;
				bufferBarrier.srcAccessMask = barrier.srcAccess;
				bufferBarrier.dstStageMask = barrier.dstStage;
				bufferBarrier.dstAccessMask = barrier.dstAccess;
				bufferBarrier.buffer = barrier.buffer;
				bufferBarrier.offset = 0;
				bufferBarrier.size = barrier.size;

				bufferMemoryBarriers.push_back(bufferBarrier);
			}

			VkDependencyInfo dep;
			dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			dep.memoryBarrierCount = 0;
			dep.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferMemoryBarriers.size());
			dep.pBufferMemoryBarriers = bufferMemoryBarriers.data();
			dep.imageMemoryBarrierCount = static_cast<uint32_t>(ImageMemoryBarriers.size());
			dep.pImageMemoryBarriers = ImageMemoryBarriers.data();
			dep.pNext = nullptr;
			dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			vkCmdPipelineBarrier2(cmd, &dep);
		}
	};
}
#pragma once

#include "VkConfig.h"
#include "Device.h"

namespace Vk {
	class Image final{
    public:
		Image(const Device& device, uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, uint32_t usage, const VkImageAspectFlags aspectFlags) :_device(device.GetDevice()), _allocator(device.GetAllocator()) {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = mipLevels;
            imageInfo.arrayLayers = 1;
            imageInfo.format = format;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = usage;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.flags = 0;

            _mipLevel = 1;

            VmaAllocationCreateInfo allocationInfo{};
            allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            Check(vmaCreateImage(_allocator, &imageInfo, &allocationInfo, &_image, &_allocation, nullptr), "create image");
		    
            _imageViews.resize(mipLevels);
            for (auto i = 0; i < mipLevels; i++) {
                CreateImageView(device.GetDevice(), format, i, aspectFlags, 1, _image, _imageViews[i]);
            }
        }

        ~Image() {
            for (auto imageView : _imageViews) CleanUpImageView(_device, imageView);
            vmaDestroyImage(_allocator, _image, _allocation);
        }

        inline void CreateImageView(VkDevice device,VkFormat format, uint32_t baseMipLevel, const VkImageAspectFlags aspectFlags, uint32_t levelCount, VkImage& image, VkImageView& imageview) {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = image;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = aspectFlags;
            createInfo.subresourceRange.baseMipLevel = baseMipLevel;
            createInfo.subresourceRange.levelCount = levelCount;
            createInfo.subresourceRange.layerCount = 1;

            Check(vkCreateImageView(device, &createInfo, nullptr, &imageview), "create image view");
        }

        inline void CleanUpImageView(VkDevice device, VkImageView& imageview) {
            vkDestroyImageView(device, imageview, nullptr);
        }

        static void UpdateDescriptorSets(const std::vector<std::pair<VkImageView, VkSampler>>& imageData, VkDevice device, VkDescriptorSet descriptorSet, uint32_t dstArrayElement) {
            std::vector<VkDescriptorImageInfo> imageInfos(imageData.size());
            uint32_t i = 0;
            for (auto& imageInfo : imageInfos) {
                imageInfo.imageView     = imageData[i].first;
                imageInfo.sampler       = imageData[i].second;
                imageInfo.imageLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                i++;
            }

            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet = descriptorSet;
            writeDescriptorSet.dstBinding = 0;
            writeDescriptorSet.dstArrayElement = dstArrayElement;
            writeDescriptorSet.descriptorCount = imageData.size();
            writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorSet.pImageInfo = imageInfos.data();
            vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
        }

        VkImage& GetImage() { return _image; }
        VkImageView& GetImageView() { return _imageViews[0]; }
        VkImageView& GetImageView(uint32_t id) { return _imageViews[id]; }
        uint32_t GetMipLevel() { return _mipLevel; }

    private:
        VkDevice _device;
        VkImage _image;
        std::vector<VkImageView> _imageViews;
        VmaAllocator _allocator;
        VmaAllocation _allocation;
        uint32_t _mipLevel;
	};
}
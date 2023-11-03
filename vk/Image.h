#pragma once

#include "VkConfig.h"
#include "Device.h"

namespace Vk {
	class Image final{
    public:
		Image(const Device& device, uint32_t width, uint32_t height, VkFormat format, uint32_t usage, const VkImageAspectFlags aspectFlags) :_device(device.GetDevice()), _allocator(device.GetAllocator()) {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
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
		    
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = _image;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = aspectFlags;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.layerCount = 1;

            Check(vkCreateImageView(device.GetDevice(), &createInfo, nullptr, &_imageView),
                "create image view");
        }

        ~Image() {
            vkDestroyImageView(_device, _imageView, nullptr);
            vmaDestroyImage(_allocator, _image, _allocation);
        }

        VkImage GetImage() { return _image; }
        VkImageView& GetImageView() { return _imageView; }
        uint32_t GetMipLevel() { return _mipLevel; }

    private:
        VkDevice _device;
        VkImage _image;
        VkImageView _imageView;
        VmaAllocator _allocator;
        VmaAllocation _allocation;
        uint32_t _mipLevel;
	};
}
#pragma once
#include "VkConfig.h"

namespace Vk {
	class ImageSampler final {
	public:
		ImageSampler(VkDevice device, VkSamplerAddressMode addressMode, bool useUnnormalizedCoordinates) : _device(device){
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = addressMode;
            samplerInfo.addressModeV = addressMode;
            samplerInfo.addressModeW = addressMode;
            //samplerInfo.anisotropyEnable = VK_TRUE;

            //VkPhysicalDeviceProperties properties{};
            //vkGetPhysicalDeviceProperties(physicalDevice, &properties);
            //samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = useUnnormalizedCoordinates;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode = useUnnormalizedCoordinates ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = useUnnormalizedCoordinates ? 0 : VK_LOD_CLAMP_NONE;

            Check(vkCreateSampler(device, &samplerInfo, nullptr, &_sampler), "create image sampler");
		}

        ~ImageSampler() {
            vkDestroySampler(_device, _sampler, nullptr);
        }

        VkSampler GetSampler() { return _sampler; }
	private:
        VkDevice _device;
		VkSampler _sampler;
	};
}
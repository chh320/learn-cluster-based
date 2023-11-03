#include "SwapChain.h"
#include <algorithm>
#include <iostream>

namespace Vk {
	SwapChain::SwapChain(const Device& device, Window& window) : _device(device){
        const auto swapChainSupport = QuerySwapChainSupport(device.GetPhysicalDevice(), device.GetSurface());
        if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
        {
            throw std::runtime_error("empty swap chain support");
        }

        const auto& surface = device.GetSurface();

        _swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
        //VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities, window);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = _swapchainFormat;
        //createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        //createInfo.clipped = VK_TRUE;
        //createInfo.oldSwapchain = nullptr;

        const auto& queueFamilyIndices = device.GetQueueFamilyIndices();
        if (queueFamilyIndices.graphicsFamily.value() != queueFamilyIndices.presentFamily.value())
        {
            uint32_t graphicsQueue = static_cast<uint32_t>(queueFamilyIndices.graphicsFamily.value());
            uint32_t presentQueue = static_cast<uint32_t>(queueFamilyIndices.presentFamily.value());
            uint32_t queueFamilyIndices[] = { graphicsQueue, presentQueue };

            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        Check(vkCreateSwapchainKHR(device.GetDevice(), &createInfo, nullptr, &_swapchain), "create swap chain!");
	    
       
        _presentMode = presentMode;
        _minImageCount = imageCount;
        _extent = extent;

        vkGetSwapchainImagesKHR(device.GetDevice(), _swapchain, &imageCount, nullptr);
        _swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device.GetDevice(), _swapchain, &imageCount, _swapChainImages.data());
        
        _swapChainImageViews.resize(_swapChainImages.size());
        for (size_t i = 0; i < _swapChainImages.size(); i++) {
            _swapChainImageViews[i] = CreateImageView(device, _swapChainImages[i], _swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }

#ifdef _DEBUG
        std::cerr << "Min image count is: " << _minImageCount << "\n";
        std::cerr << "Extent is : " << _extent.width << " x " << _extent.height << "\n";
        std::cerr << "Present Mode is: " << presentMode <<  "\n";
        std::cerr << "Swapchain format is: " << _swapchainFormat << "\n";
#endif // _DEBUG

    }

    VkImageView SwapChain::CreateImageView(const Device& device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(device.GetDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
        return imageView;
    }

	SwapChain::SupportDetails SwapChain::QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
        SupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
	}

    SwapChain::~SwapChain() {
        for (size_t i = 0; i < _minImageCount; i++) {
            vkDestroyImageView(_device.GetDevice(), _swapChainImageViews[i], nullptr);
        }
        vkDestroySwapchainKHR(_device.GetDevice(), _swapchain, nullptr);
        _swapChainImages.clear();
        _swapChainImageViews.clear();
    }

    VkSurfaceFormatKHR SwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR SwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window& window) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            int width, height;
            glfwGetFramebufferSize(window.GetWindow(), &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }
}
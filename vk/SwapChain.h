#pragma once

#include "VkConfig.h"
#include "VkWindow.h"
#include "Device.h"
#include <vector>

namespace Vk {
	class SwapChain {
	public:
		SwapChain(const Device& device, Window& window);
		~SwapChain();
		
		const Device GetDevice() const { return _device; }
		const VkExtent2D GetExtent() const { return _extent; }
		const uint32_t GetImageCount() const { return _minImageCount; }
		const VkFormat GetImageFormat() const { return _swapchainFormat; }
		VkImage& GetImage(uint32_t id) { return _swapChainImages[id]; }
		VkImageView& GetImageView(uint32_t id) { return _swapChainImageViews[id]; }
		VkSwapchainKHR GetSwapChain() { return _swapchain; }
	private:
		struct SupportDetails {
			VkSurfaceCapabilitiesKHR capabilities{};
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

		SupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window& window);
		VkImageView CreateImageView(const Device& device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

		const Device& _device;
		VkSwapchainKHR _swapchain;
		VkFormat _swapchainFormat;
		VkPresentModeKHR _presentMode;
		uint32_t _minImageCount;
		VkExtent2D _extent;
		uint32_t renderingInfo;

		std::vector<VkImage> _swapChainImages;
		std::vector<VkImageView> _swapChainImageViews;
	};
}
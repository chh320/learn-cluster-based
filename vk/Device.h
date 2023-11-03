#pragma once

#include "VkConfig.h"
#include "VkWindow.h"
#include "Queue.h"

#include <vector>

namespace Vk {
	class Device {
	public:
		Device() {};
		Device(bool enableValidationLayers, Window& window);
		~Device();

		void InitVulkan(bool enableValidationLayers, Window& window);
		void CreateInstance();
		void CreateSurface(Window& window);
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void CreateAllocator();

		const VkPhysicalDevice GetPhysicalDevice() const { return _physicalDevice; }
		const VkDevice GetDevice() const { return _device; }
		const VkQueue GetQueue() const { return _queue; }
		const QueueFamilyIndices& GetQueueFamilyIndices() const { return _queueFamilyId; }
		const VkSurfaceKHR GetSurface() const { return _surface; }
		const VmaAllocator GetAllocator() const { return _allocator; }

	private:
		const std::vector<const char*> validationLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		const std::vector<const char*> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		VkInstance _instance;
		VkPhysicalDevice _physicalDevice;
		VkDevice _device;
		VkQueue _queue;
		QueueFamilyIndices _queueFamilyId;
		VkSurfaceKHR _surface;
		VmaAllocator _allocator;

		bool checkDeviceExtensionSupport(VkPhysicalDevice device);
		bool CheckValidationLayerSupport();
		std::vector<const char*> getRequiredExtensions();
	};
}
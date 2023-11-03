#include "Device.h"
#include <iostream>
#include <string>
#include <set>

namespace Vk {
	Device::Device(bool enableValidationLayers, Window& window) {
		InitVulkan(enableValidationLayers, window);
	}

	Device::~Device() {
		vmaDestroyAllocator(_allocator);
		vkDestroyDevice(_device, nullptr);
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyInstance(_instance, nullptr);
	}

	void Device::InitVulkan(bool enableValidationLayers, Window& window) {
		if (enableValidationLayers && !CheckValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		CreateInstance();
		CreateSurface(window);
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateAllocator();
	}

	void Device::CreateInstance() {
		auto extensions = getRequiredExtensions();

		VkApplicationInfo appInfo;
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "VulkanTesting";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;
		appInfo.pNext = nullptr;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		Check(vkCreateInstance(&createInfo, nullptr, &_instance), "create instance");
	}

	void Device::CreateSurface(Window& window) {
		Check(glfwCreateWindowSurface(_instance, window.GetWindow(), nullptr, &_surface), "create surface");
	}

	void Device::PickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);

		if (deviceCount == 0) {
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

		//select device
		const auto result = std::find_if(devices.begin(), devices.end(), [&](const VkPhysicalDevice& device) {
			/*VkPhysicalDeviceProperties2 prop{};
			prop.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			vkGetPhysicalDeviceProperties2(device, &prop);*/

			// We want a device with geometry shader support.
			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

			bool extensionsSupported = checkDeviceExtensionSupport(device);
			QueueFamilyIndices indices = QueueFamilyIndices::findQueueFamilies(device, _surface);

			return indices.isComplete() && extensionsSupported;
			});
		_physicalDevice = *result;

		if (_physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("failed to find a suitable GPU!");
		}

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(_physicalDevice, &properties);
		std::cerr << "Choose: " << properties.deviceName << std::endl;
	}

	void Device::CreateLogicalDevice() {
		_queueFamilyId = QueueFamilyIndices::findQueueFamilies(_physicalDevice, _surface);
		uint32_t graphicsQueue = _queueFamilyId.graphicsFamily.value();

		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = graphicsQueue;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		VkPhysicalDeviceFeatures deviceFeature{};
		deviceFeature.fillModeNonSolid = true;		// for wire frame

		VkPhysicalDeviceDynamicRenderingFeatures deviceRenderFeature{};
		deviceRenderFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
		deviceRenderFeature.dynamicRendering = VK_TRUE;

		VkPhysicalDeviceSynchronization2Features synchronization2Features{};
		synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
		synchronization2Features.pNext = &deviceRenderFeature;
		synchronization2Features.synchronization2 = VK_TRUE;

		VkPhysicalDeviceDescriptorIndexingFeatures indexingFeature{};
		indexingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		indexingFeature.pNext = &synchronization2Features;
		indexingFeature.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
		indexingFeature.descriptorBindingPartiallyBound = VK_TRUE;
		indexingFeature.descriptorBindingVariableDescriptorCount = VK_TRUE;
		indexingFeature.runtimeDescriptorArray = VK_TRUE;

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
		deviceCreateInfo.pEnabledFeatures = &deviceFeature;
		deviceCreateInfo.pNext = &indexingFeature;

		Check(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device), "create logical device");
		vkGetDeviceQueue(_device, graphicsQueue, 0, &_queue);
	}

	void Device::CreateAllocator() {
		VmaAllocatorCreateInfo allocatorInfo{};
		allocatorInfo.physicalDevice = _physicalDevice;
		allocatorInfo.device = _device;
		allocatorInfo.instance = _instance;

		Check(vmaCreateAllocator(&allocatorInfo, &_allocator), "create allocator");
	}

	bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	bool Device::CheckValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	std::vector<const char*> Device::getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		return std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
	}
}
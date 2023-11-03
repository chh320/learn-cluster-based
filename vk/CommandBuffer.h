#pragma once
#include "VkConfig.h"
#include "Device.h"
#include <vector>
#include <functional>

namespace Vk {
	class CommandPool final {
	public:
		CommandPool(const Device& device) :_device(device.GetDevice()) {
			VkCommandPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			poolInfo.queueFamilyIndex = device.GetQueueFamilyIndices().graphicsFamily.value();

			Check(vkCreateCommandPool(device.GetDevice(), &poolInfo, nullptr, &_commandPool),
				"create command pool");
		}

		~CommandPool() {
			vkDestroyCommandPool(_device, _commandPool, nullptr);
		}

		const VkCommandPool GetCommandPool()const { return _commandPool; }

	private:
		VkDevice _device;
		VkCommandPool _commandPool;
	};

	class CommandBuffers final {
	public:
		CommandBuffers(const Device& device, const CommandPool& commandPool, uint32_t size) {
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = commandPool.GetCommandPool();
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = size;

			_commandBuffers.resize(size);

			if (vkAllocateCommandBuffers(device.GetDevice(), &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate command buffers!");
			}
		}

		VkCommandBuffer Begin(const size_t i) {
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = nullptr; // Optional

			Check(vkBeginCommandBuffer(_commandBuffers[i], &beginInfo),
				"begin recording command buffer");

			return _commandBuffers[i];
		}

		void End(const size_t i)
		{
			Check(vkEndCommandBuffer(_commandBuffers[i]),
				"record command buffer");
		}
		VkCommandBuffer& operator[](const size_t i) { return _commandBuffers[i]; }
		std::vector<VkCommandBuffer>& GetCommandBuffers() { return _commandBuffers; }
		const uint32_t GetSize() const { return _commandBuffers.size(); }
		
	private:
		std::vector<VkCommandBuffer> _commandBuffers;
	};

	class SingleTimeCommands final {
	public:
		static void Submit(const Device& device, CommandPool& commandPool, const std::function<void(VkCommandBuffer)>& action)
		{
			CommandBuffers commandBuffers(device, commandPool, 1);

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(commandBuffers[0], &beginInfo);

			action(commandBuffers[0]);

			vkEndCommandBuffer(commandBuffers[0]);

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffers[0];

			const auto graphicsQueue = device.GetQueue();

			vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr);
			vkQueueWaitIdle(graphicsQueue);
		}
	};
}
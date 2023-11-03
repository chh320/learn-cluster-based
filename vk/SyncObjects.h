#pragma once

#include "VkConfig.h"
#include <vector>

namespace Vk {
	class SyncObjects final{
	public:
		SyncObjects(VkDevice device,uint32_t frameNum) :_device(device){
            _imageAvailableSemaphores.resize(frameNum);
            _renderFinishedSemaphores.resize(frameNum);
            _inFlightFences.resize(frameNum);

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (size_t i = 0; i < frameNum; i++) {
                Check(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]), "create semaphore.");
                Check(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]), "create semaphore.");
                Check(vkCreateFence(device, &fenceInfo, nullptr, &_inFlightFences[i]), "create fence.");
            }
		}

        ~SyncObjects() {
            for (size_t i = 0; i < _imageAvailableSemaphores.size(); i++) {
                vkDestroySemaphore(_device, _imageAvailableSemaphores[i], nullptr);
                vkDestroySemaphore(_device, _renderFinishedSemaphores[i], nullptr);
                vkDestroyFence(_device, _inFlightFences[i], nullptr);
            }
        }

        VkSemaphore& GetImageAvailableSemaphore(uint32_t id) { return _imageAvailableSemaphores[id]; }
        VkSemaphore& GetRenderFinishedSemaphore(uint32_t id) { return _renderFinishedSemaphores[id]; }
        VkFence& GetFence(uint32_t id) { return _inFlightFences[id]; }

	private:
        VkDevice _device;
		std::vector<VkSemaphore> _imageAvailableSemaphores;
		std::vector<VkSemaphore> _renderFinishedSemaphores;
		std::vector<VkFence> _inFlightFences;
	};
}
#pragma once

#include "VkConfig.h"
#include "SwapChain.h"
#include "DescriptorSetManager.h"
#include "Vertex.h"
#include "RenderPass.h"

#include <string>

namespace Vk {
	class GraphicsPipeline final {
	public:
		GraphicsPipeline(const Device& device, const DescriptorSetManager& descriptorSetManager, const RenderInfo& info, bool isWireFrame = false);
		~GraphicsPipeline();

		const VkPipeline GetPipeline() const { return _graphicsPipeline; }
		const VkPipelineLayout GetPipelineLayout() const { return _pipelineLayout; }


	private:
		VkDevice _device;
		bool _isWireFrame;
		VkPipeline _graphicsPipeline;
		VkPipelineLayout _pipelineLayout;
	};
}

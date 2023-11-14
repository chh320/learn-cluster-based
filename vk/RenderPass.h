#pragma once
#include "VkConfig.h"

#include <vector>
#include <string>

namespace Vk {
	struct RenderInfo {
		uint32_t viewWidth, viewHeight;
		uint32_t pushConstantSize;
		uint32_t compareOp;
		bool useInstance;
		std::pair<std::string, std::string> shaderName;

		std::vector<VkFormat> colorAttachmentFormats;
		VkFormat depthAttachmentFormat;
	};

	struct RenderPassInfo
	{
		VkImageView colorImageView;
		VkImageView depthImageView;
		VkExtent2D extent2D;
	};
}
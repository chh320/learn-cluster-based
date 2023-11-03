#pragma once

#include "VkConfig.h"
#include <shaderc/shaderc.hpp>
#include <string>
#include <vector>

namespace Vk {
	class ShaderModule final
	{
	public:

		ShaderModule(VkDevice device, const std::string& filename);
		ShaderModule(VkDevice device, const std::vector<char>& code);
		ShaderModule(VkDevice device, const std::vector<uint32_t>& code);
		~ShaderModule();

		const VkDevice& Device() const { return device_; }

		VkPipelineShaderStageCreateInfo CreateShaderStage(VkShaderStageFlagBits stage) const;
		static std::vector<uint32_t> ReadFile(const std::string& filename, shaderc_shader_kind kind, bool optimize = false);

	private:
		static std::vector<char> ReadFile(const std::string& filename);

		VkDevice device_;
		VkShaderModule shaderModule_;
	};
}
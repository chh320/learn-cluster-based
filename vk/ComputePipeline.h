#pragma once

#include "VkConfig.h"
#include "Device.h"
#include "DescriptorSetManager.h"
#include "ShaderModule.h"

namespace Vk {
	class ComputePipeline final {
	public:
		ComputePipeline(const Device& device, const DescriptorSetManager& descriptorSetManager, uint32_t pushConstantSize) : _device(device.GetDevice())
		{
			// Load shaders.
			auto comp_code = ShaderModule::ReadFile("shaders/shader.comp", shaderc_glsl_compute_shader);

			const ShaderModule compShader(_device, comp_code);

			VkPipelineShaderStageCreateInfo shaderStages = compShader.CreateShaderStage(VK_SHADER_STAGE_COMPUTE_BIT);

			// Pipeline layout
			VkPushConstantRange pushConstant{};
			pushConstant.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;
			pushConstant.offset = 0;
			pushConstant.size = pushConstantSize;

			VkDescriptorSetLayout layouts[2] = {
				descriptorSetManager.GetBindlessBufferLayout(),
				descriptorSetManager.GetBindlessImageLayout()
			};

			VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 2;
			pipelineLayoutInfo.pSetLayouts = layouts;
			pipelineLayoutInfo.pushConstantRangeCount = 1;
			pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
			Check(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout),
				"create pipeline layout.");

			VkComputePipelineCreateInfo pipelineInfo{};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipelineInfo.stage = shaderStages;
			pipelineInfo.layout = _pipelineLayout;

			Check(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline), "create compute pipeline");
		}

		~ComputePipeline() {
			vkDestroyPipeline(_device, _pipeline, nullptr);
			vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
		}

		const VkPipeline GetPipeline() const { return _pipeline; }
		const VkPipelineLayout GetPipelineLayout() const { return _pipelineLayout; }

	private:
		VkDevice _device;
		VkPipeline _pipeline;
		VkPipelineLayout _pipelineLayout;
	};
}
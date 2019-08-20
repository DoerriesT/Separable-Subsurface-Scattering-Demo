#include "SSSBlurPipeline.h"
#include "utility/Utility.h"
#include "ShaderModule.h"
#include <glm/vec2.hpp>

namespace
{
	using namespace glm;
	struct PushConsts
	{
		vec2 texelSize;
		vec2 dir;
		float sssWidth;
	};
}

std::pair<VkPipeline, VkPipelineLayout> sss::vulkan::SSSBlurPipeline::create(VkDevice device, uint32_t setLayoutCount, VkDescriptorSetLayout * setLayouts)
{
	VkPipelineLayout pipelineLayout;

	VkPushConstantRange pushConstantRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConsts) };

	VkPipelineLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutCreateInfo.setLayoutCount = setLayoutCount;
	layoutCreateInfo.pSetLayouts = setLayouts;
	layoutCreateInfo.pushConstantRangeCount = 1;
	layoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create PipelineLayout!", EXIT_FAILURE);
	}

	ShaderModule computeShaderModule(device, "resources/shaders/sssBlur_comp.spv");

	VkPipelineShaderStageCreateInfo shaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, computeShaderModule, "main" };

	VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	pipelineInfo.stage = shaderStage;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	VkPipeline pipeline;
	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create pipeline!", EXIT_FAILURE);
	}

	return { pipeline, pipelineLayout };
}

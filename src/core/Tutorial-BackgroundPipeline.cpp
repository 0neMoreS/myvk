#include "Tutorial.hpp"

#include "Helpers.hpp"
#include "refsol.hpp"
#include "VK.hpp"

static uint32_t vert_code[] = {
#include "../shaders/spv/background.vert.inl"
};

static uint32_t frag_code[] = {
#include "../shaders/spv/background.frag.inl"
};

void Tutorial::BackgroundPipeline::create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass) {
	VkShaderModule vert_module = rtg.helpers.create_shader_module(vert_code);
	VkShaderModule frag_module = rtg.helpers.create_shader_module(frag_code);

	{ //create pipeline layout:
		VkPushConstantRange range{
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0,
			.size = sizeof(Push),
		};

		VkPipelineLayoutCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 0,
			.pSetLayouts = nullptr,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &range,
		};

		VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout) );
	}
}

void Tutorial::BackgroundPipeline::destroy(RTG &rtg) {
	refsol::BackgroundPipeline_destroy(rtg, &layout, &handle);
}
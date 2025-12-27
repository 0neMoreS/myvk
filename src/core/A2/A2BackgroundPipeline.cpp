#include "A2BackgroundPipeline.hpp"

#include "Helpers.hpp"
#include "VK.hpp"

#include <array>
#include <stdexcept>
#include <vector>

static uint32_t vert_code[] = {
#include "../../shaders/spv/A2-background.vert.inl"
};

static uint32_t frag_code[] = {
#include "../../shaders/spv/A2-background.frag.inl"
};

void A2BackgroundPipeline::create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass) {
    vert_module = rtg.helpers.create_shader_module(vert_code);
    frag_module = rtg.helpers.create_shader_module(frag_code);

    assert(set0_PV != VK_NULL_HANDLE && "set0_PV must be set before creating A2BackgroundPipeline");
    assert(set1_CUBEMAP != VK_NULL_HANDLE && "set1_CUBEMAP must be set before creating A2BackgroundPipeline");

    { //create pipeline layout:
		std::array< VkDescriptorSetLayout, 2 > layouts{
			set0_PV,
			set1_CUBEMAP
		};

		VkPipelineLayoutCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = uint32_t(layouts.size()),
			.pSetLayouts = layouts.data(),
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr,
		};

		VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout) );
	}

	create_pipeline(rtg, render_pass, subpass, false);

	vkDestroyShaderModule(rtg.device, frag_module, nullptr);
	vkDestroyShaderModule(rtg.device, vert_module, nullptr);
	frag_module = VK_NULL_HANDLE;
	vert_module = VK_NULL_HANDLE;

    pipeline_name_to_index["A2BackgroundPipeline"] = 1;
}

void A2BackgroundPipeline::destroy(RTG &rtg) {
    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtg.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
}

A2BackgroundPipeline::~A2BackgroundPipeline() {
    //ensure resources have been destroyed:
    assert(layout == VK_NULL_HANDLE);
    assert(pipeline == VK_NULL_HANDLE);
	assert(vert_module == VK_NULL_HANDLE);
	assert(frag_module == VK_NULL_HANDLE);
}

#include "A2ReflectionPipeline.hpp"

static uint32_t vert_code[] = {
#include "../../shaders/spv/A2-reflection.vert.inl"
};

static uint32_t frag_code[] = {
#include "../../shaders/spv/A2-reflection.frag.inl"
};

A2ReflectionPipeline::~A2ReflectionPipeline(){
    assert(layout == VK_NULL_HANDLE); //should have been destroyed already
	assert(pipeline == VK_NULL_HANDLE);
	assert(vert_module == VK_NULL_HANDLE);
	assert(frag_module == VK_NULL_HANDLE);
    assert(set1_Transforms == VK_NULL_HANDLE);
}

void A2ReflectionPipeline::create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass) {
	vert_module = rtg.helpers.create_shader_module(vert_code);
	frag_module = rtg.helpers.create_shader_module(frag_code);

	assert(set0_PV != VK_NULL_HANDLE && "should have been set before creating object pipeline layout");
	assert(set2_CUBEMAP != VK_NULL_HANDLE && "should have been set before creating object pipeline layout");
	
	{ //the set1_Transforms layout holds an array of Transform structures in a storage buffer used in the vertex shader:
		std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
			VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
			},
		};
		
		VkDescriptorSetLayoutCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = uint32_t(bindings.size()),
			.pBindings = bindings.data(),
		};

		VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set1_Transforms) );
	}

	{ //create pipeline layout:
		std::array< VkDescriptorSetLayout, 3 > layouts{
			set0_PV,
			set1_Transforms,
            set2_CUBEMAP,
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

	create_pipeline(rtg, render_pass, subpass);
	
	vkDestroyShaderModule(rtg.device, frag_module, nullptr);
	vkDestroyShaderModule(rtg.device, vert_module, nullptr);
	frag_module = VK_NULL_HANDLE;
	vert_module = VK_NULL_HANDLE;

	block_descriptor_configs.push_back(BlockDescriptorConfig{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .layout = set1_Transforms}); //Transform

	pipeline_name_to_index["A2ReflectionPipeline"] = 2;
}

void A2ReflectionPipeline::destroy(RTG &rtg) {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtg.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }
    if (set1_Transforms != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
        set1_Transforms = VK_NULL_HANDLE;
    }
}
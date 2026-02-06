#include "A1LinesPipeline.hpp"

#include "Helpers.hpp"
#include "VK.hpp"

static uint32_t vert_code[] = {
#include "../../shaders/spv/lines.vert.inl"
};

static uint32_t frag_code[] = {
#include "../../shaders/spv/lines.frag.inl"
};

A1LinesPipeline::~A1LinesPipeline(){
	assert(layout == VK_NULL_HANDLE);
	assert(pipeline == VK_NULL_HANDLE);
	assert(set0_PV == VK_NULL_HANDLE);
}

void A1LinesPipeline::create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass, const TextureManager& texture_manager) {
    vert_module = rtg.helpers.create_shader_module(vert_code);
	frag_module = rtg.helpers.create_shader_module(frag_code);

	{ //the set0_PV layout holds PV info in a uniform buffer used in the vertex shader:
		std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
			VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
			},
		};
		
		VkDescriptorSetLayoutCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = uint32_t(bindings.size()),
			.pBindings = bindings.data(),
		};

		VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_PV) );
	}

	{ //create pipeline layout:
		std::array< VkDescriptorSetLayout, 1 > layouts{
			set0_PV, //we'd like to say "VK_NULL_HANDLE" here, but that's not valid without an extension
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

	create_pipeline(rtg, render_pass, subpass, true, false, true); //enable depth, disable cull, lines draw

	vkDestroyShaderModule(rtg.device, frag_module, nullptr);
    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
    frag_module = VK_NULL_HANDLE;
    vert_module = VK_NULL_HANDLE;

	block_descriptor_configs.push_back(
        BlockDescriptorConfig{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
        .layout = set0_PV, 
        .bindings_count = 1
    }); //Global
	
	block_descriptor_set_name_to_index = {
        {"PV", 0}
    };

    block_binding_name_to_index = {
        // PV
        {"PV", 0},
    };

	pipeline_name_to_index["A1LinesPipeline"] = 0;
}

void A1LinesPipeline::destroy(RTG &rtg) {
	if (layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(rtg.device, layout, nullptr);
		layout = VK_NULL_HANDLE;
	}

	if (pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(rtg.device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}

	if (set0_PV != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(rtg.device, set0_PV, nullptr);
		set0_PV = VK_NULL_HANDLE;
	}
}
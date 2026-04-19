#include "SSDOTiledLightingComputePipeline.hpp"
#include "Helpers.hpp"
#include "VK.hpp"

#include <vector>
#include <array>
#include <cassert>

static uint32_t comp_code[] = {
#include "../../shaders/spv/SSDO-tiled-lighting.comp.inl"
};

void SSDOTiledLightingComputePipeline::create(
		RTG &rtg, 
		VkRenderPass render_pass, 
		uint32_t subpass,
        const ManagerContext& context
	) {
    comp_module = rtg.helpers.create_shader_module(comp_code);

    { // set0_Global
        std::vector< VkDescriptorSetLayoutBinding > bindings;
        bindings.push_back(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        });

        for (uint32_t i = 1; i <= 14; ++i) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = i,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
            });
        }
        
        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_Global) );
    }

    { // pipeline layout
        VkPushConstantRange push_constant_range{
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(Push),
        };

        std::array< VkDescriptorSetLayout, 1 > layouts{
            set0_Global
        };

        VkPipelineLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = uint32_t(layouts.size()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range,
        };

        VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout) );
    }

    { // compute pipeline
        VkComputePipelineCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = comp_module,
                .pName = "main"
            },
            .layout = layout,
        };

        VK( vkCreateComputePipelines(rtg.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline) );
    }

    vkDestroyShaderModule(rtg.device, comp_module, nullptr);
    comp_module = VK_NULL_HANDLE;

	block_descriptor_configs.push_back(
		BlockDescriptorConfig{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // default type, but we override via binding_types
        .binding_types = {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // 0
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 1
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 2
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 3
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 4
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 5
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 6
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 7
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 8
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 9
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 10
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 11
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 12
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 13
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // 14
        },
		.layout = set0_Global, 
		.bindings_count = 15
	}); // Global

	block_descriptor_set_name_to_index = {
        {"Global", 0}
    };

	block_binding_name_to_index = {
        {"PV", 0},
        {"SunLights", 1},
        {"SphereLights", 2},
        {"SpotLights", 3},
        {"ShadowSunLights", 4},
        {"ShadowSphereLights", 5},
        {"ShadowSpotLights", 6},
        {"SphereTileData", 7},
        {"SphereLightIdx", 8},
        {"SpotTileData", 9},
        {"SpotLightIdx", 10},
        {"ShadowSphereTileData", 11},
        {"ShadowSphereLightIdx", 12},
        {"ShadowSpotTileData", 13},
        {"ShadowSpotLightIdx", 14},
    };

    pipeline_name_to_index["SSDOTiledLightingComputePipeline"] = 6;
}

void SSDOTiledLightingComputePipeline::destroy(RTG &rtg) {
    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtg.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }

	if(set0_Global != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(rtg.device, set0_Global, nullptr);
		set0_Global = VK_NULL_HANDLE;
	}
}

SSDOTiledLightingComputePipeline::~SSDOTiledLightingComputePipeline() {
    assert(layout == VK_NULL_HANDLE);
    assert(pipeline == VK_NULL_HANDLE);
	assert(comp_module == VK_NULL_HANDLE);
	assert(set0_Global == VK_NULL_HANDLE);
}
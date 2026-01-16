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

void A2BackgroundPipeline::create(
		RTG &rtg, 
		VkRenderPass render_pass, 
		uint32_t subpass,
		const TextureManager& texture_manager
	) {
    vert_module = rtg.helpers.create_shader_module(vert_code);
    frag_module = rtg.helpers.create_shader_module(frag_code);

    { // PV matrix layout
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

	{ // bind texture descriptors: cubemaps
		assert(texture_manager.raw_environment_cubemap_texture.size() > 0);

		{ // Cubemap sampler layout
			std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
				VkDescriptorSetLayoutBinding{
					.binding = 0,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
				},
			};
			
			VkDescriptorSetLayoutCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = uint32_t(bindings.size()),
				.pBindings = bindings.data(),
			};

			VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set1_CUBEMAP) );
		}

		{ // allocate texture descriptor and update data
			VkDescriptorSetAllocateInfo alloc_info{
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                        .descriptorPool = texture_manager.texture_descriptor_pool,
                        .descriptorSetCount = 1,
                        .pSetLayouts = &set1_CUBEMAP,
                    };

			VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &set1_CUBEMAP_instance));
		}

		{ // update cubemap descriptors
			std::array< VkDescriptorImageInfo, 1 > image_infos = {
				VkDescriptorImageInfo{
                    .sampler = texture_manager.raw_environment_cubemap_texture[0]->sampler,
                    .imageView = texture_manager.raw_environment_cubemap_texture[0]->image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                }
			};

			VkWriteDescriptorSet write_cubemap{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set1_CUBEMAP_instance,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = image_infos.data(),
                };

			vkUpdateDescriptorSets(rtg.device, 1, &write_cubemap, 0, nullptr);
		}
	}

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

	create_pipeline(rtg, render_pass, subpass, false, false); //disable depth and cull for background pipeline

	vkDestroyShaderModule(rtg.device, frag_module, nullptr);
	vkDestroyShaderModule(rtg.device, vert_module, nullptr);
	frag_module = VK_NULL_HANDLE;
	vert_module = VK_NULL_HANDLE;

	block_descriptor_configs.push_back(
		BlockDescriptorConfig{
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
		.layout = set0_PV, 
		.bindings_count = 1
	}); //PV

	block_descriptor_set_name_to_index = {
        {"PV", 0}
    };

	block_binding_name_to_index = {
        // PV
        {"PV", 0},
    };

    pipeline_name_to_index["A2BackgroundPipeline"] = 0;
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

	if(set0_PV != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(rtg.device, set0_PV, nullptr);
		set0_PV = VK_NULL_HANDLE;
	}

	if(set1_CUBEMAP != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(rtg.device, set1_CUBEMAP, nullptr);
		set1_CUBEMAP = VK_NULL_HANDLE;
	}

	if(set1_CUBEMAP_instance != VK_NULL_HANDLE) {
		set1_CUBEMAP_instance = VK_NULL_HANDLE;
	}
}

A2BackgroundPipeline::~A2BackgroundPipeline() {
    //ensure resources have been destroyed:
    assert(layout == VK_NULL_HANDLE);
    assert(pipeline == VK_NULL_HANDLE);
	assert(vert_module == VK_NULL_HANDLE);
	assert(frag_module == VK_NULL_HANDLE);
	assert(set0_PV == VK_NULL_HANDLE);
	assert(set1_CUBEMAP == VK_NULL_HANDLE);
	assert(set1_CUBEMAP_instance == VK_NULL_HANDLE);
}

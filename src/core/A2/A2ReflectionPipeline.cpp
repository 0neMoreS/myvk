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
	assert(set0_PV == VK_NULL_HANDLE);
    assert(set1_Transforms == VK_NULL_HANDLE);
	assert(set2_CUBEMAP == VK_NULL_HANDLE);
}

void A2ReflectionPipeline::create(
    class RTG &rtg, 
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

	{ // the set1_Transforms layout holds an array of Transform structures in a storage buffer used in the vertex shader:
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

			VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set2_CUBEMAP) );
		}

		{ // allocate texture descriptor and update data
			VkDescriptorSetAllocateInfo alloc_info{
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                        .descriptorPool = texture_manager.texture_descriptor_pool,
                        .descriptorSetCount = 1,
                        .pSetLayouts = &set2_CUBEMAP,
                    };

			VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &set2_CUBEMAP_instance));
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
                    .dstSet = set2_CUBEMAP_instance,
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

	create_pipeline(rtg, render_pass, subpass, true);
	
	vkDestroyShaderModule(rtg.device, frag_module, nullptr);
	vkDestroyShaderModule(rtg.device, vert_module, nullptr);
	frag_module = VK_NULL_HANDLE;
	vert_module = VK_NULL_HANDLE;

	block_descriptor_configs.push_back(
        BlockDescriptorConfig{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
        .layout = set0_PV, 
        .bindings_count = 2
    }); //Global
    block_descriptor_configs.push_back(
        BlockDescriptorConfig{
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
        .layout = set1_Transforms, 
        .bindings_count = 1
    }); //Transform

	block_descriptor_set_name_to_index = {
        {"PV", 0},
		{"Transforms", 1},
    };

	block_binding_name_to_index = {
        // PV
        {"PV", 0},
		// Transforms
		{"Transforms", 0},
    };

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

	if(set0_PV != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(rtg.device, set0_PV, nullptr);
		set0_PV = VK_NULL_HANDLE;
	}

    if (set1_Transforms != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
        set1_Transforms = VK_NULL_HANDLE;
    }

	if(set2_CUBEMAP != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(rtg.device, set2_CUBEMAP, nullptr);
		set2_CUBEMAP = VK_NULL_HANDLE;
	}

	if(set2_CUBEMAP_instance != VK_NULL_HANDLE) {
		//descriptor sets are automatically freed when the descriptor pool is destroyed, so no need to free individually
		set2_CUBEMAP_instance = VK_NULL_HANDLE;
	}
}
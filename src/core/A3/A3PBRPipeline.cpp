#include "A3PBRPipeline.hpp"
#include "ShadowMapManager.hpp"

static uint32_t vert_code[] = {
#include "../../shaders/spv/A3-pbr.vert.inl"
};

static uint32_t frag_code[] = {
#include "../../shaders/spv/A3-pbr.frag.inl"
};

A3PBRPipeline::~A3PBRPipeline(){
    assert(layout == VK_NULL_HANDLE); //should have been destroyed already
    assert(pipeline == VK_NULL_HANDLE);

    assert(vert_module == VK_NULL_HANDLE);
    assert(frag_module == VK_NULL_HANDLE);

    assert(set0_Global == VK_NULL_HANDLE);
    assert(set1_Transforms == VK_NULL_HANDLE);
    assert(set2_Textures == VK_NULL_HANDLE);
    assert(set2_Textures_instance == VK_NULL_HANDLE);
    assert(sun_shadow_array_view == VK_NULL_HANDLE);
}

void A3PBRPipeline::create(
    RTG &rtg, 
    VkRenderPass render_pass, 
    uint32_t subpass,
    const ManagerContext& context
){
    auto const &texture_manager = *context.texture_manager;
    auto const *shadow_map_manager = context.shadow_map_manager;
    vert_module = rtg.helpers.create_shader_module(vert_code);
    frag_module = rtg.helpers.create_shader_module(frag_code);

    { //the set0_Global layout holds PV(UBO) + light buffers(SSBO):
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(15);
        bindings.push_back(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1, // PV matrix
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        });

        for (uint32_t i = 1; i <= 14; ++i) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = i,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            });
        }
        
        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_Global) );
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

    { // bind texture descriptors: cubemaps and 2D textures
        uint32_t total_2d_descriptors = 1; // BRDF LUT
        uint32_t total_cubemap_descriptors = 2; // IrradianceMap + PrefilterMap
        const uint32_t sun_shadow_count = texture_manager.sun_shadow_descriptor_count;
        const uint32_t sphere_shadow_count = texture_manager.sphere_shadow_descriptor_count;
        const uint32_t spot_shadow_count = texture_manager.spot_shadow_descriptor_count;
        assert(texture_manager.raw_environment_cubemap_texture.size() > 1);

        for (const auto &material_slots : texture_manager.raw_2d_textures_by_material) {
            for (const auto &texture_opt : material_slots) {
                if (texture_opt) {
                    ++total_2d_descriptors;
                }
            }
        }

        { // the set2_Textures
            std::array< VkDescriptorSetLayoutBinding, 5 > bindings{
                VkDescriptorSetLayoutBinding{
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = total_cubemap_descriptors, // IrradianceMap + PrefilterMap
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                },
                VkDescriptorSetLayoutBinding{
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = total_2d_descriptors, // runtime-defined number of 2D textures
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                },
                VkDescriptorSetLayoutBinding{
                    .binding = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = sun_shadow_count, // SunShadowMap (sampler2DArray)
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                },
                VkDescriptorSetLayoutBinding{
                    .binding = 3,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = sphere_shadow_count, // SphereShadowMap (samplerCube)
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                },
                VkDescriptorSetLayoutBinding{
                    .binding = 4,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = spot_shadow_count, // SpotShadowMap (sampler2D)
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                }
            };

            std::array<VkDescriptorBindingFlags, 5> binding_flags{
                0,  // binding 0: fixed size
				0,  // binding 1: fixed size
				0,
				0,
				0,
			};

            VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = uint32_t(binding_flags.size()),
                .pBindingFlags = binding_flags.data(),
            };
            
            VkDescriptorSetLayoutCreateInfo create_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &binding_flags_info,
                .bindingCount = uint32_t(bindings.size()),
                .pBindings = bindings.data(),
            };

            VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set2_Textures) );
        }

        { // allocate texture descriptor and update data
            { // the set2_Textures_instance
                // uint32_t total_descriptors = total_2d_descriptors + total_cubemap_descriptors;
                VkDescriptorSetVariableDescriptorCountAllocateInfo var_count_alloc_info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                    .descriptorSetCount = 1,
                    .pDescriptorCounts = &total_2d_descriptors  // runtime-defined size
                };

                VkDescriptorSetAllocateInfo alloc_info{
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                        .pNext = &var_count_alloc_info,
                        .descriptorPool = texture_manager.texture_descriptor_pool,
                        .descriptorSetCount = 1,
                        .pSetLayouts = &set2_Textures,
                };

                VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &set2_Textures_instance));
            }

            { // update cubemap descriptors
                std::vector<VkDescriptorImageInfo> image_info(2);
                // IrradianceMap
                image_info[0] = VkDescriptorImageInfo{
                    .sampler = texture_manager.raw_environment_cubemap_texture[1]->sampler,
                    .imageView = texture_manager.raw_environment_cubemap_texture[1]->image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                // PrefilterMap
                image_info[1] = VkDescriptorImageInfo{
                    .sampler = texture_manager.raw_environment_cubemap_texture[2]->sampler,
                    .imageView = texture_manager.raw_environment_cubemap_texture[2]->image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };

                VkWriteDescriptorSet write_cubemap{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set2_Textures_instance,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = image_info.data(),
                };

                vkUpdateDescriptorSets(rtg.device, 1, &write_cubemap, 0, nullptr);
            }

            { //update the set2_Textures_instance descriptor set
                std::vector<VkDescriptorImageInfo> image_info(total_2d_descriptors);
                size_t index = 0;

                // PBRTLUT
                image_info[index] = VkDescriptorImageInfo{
                    .sampler = texture_manager.raw_brdf_LUT_texture->sampler,
                    .imageView = texture_manager.raw_brdf_LUT_texture->image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                ++index;

                for(const auto &material_slots : texture_manager.raw_2d_textures_by_material) {
                    for (const auto &texture_opt : material_slots) {
                        if (texture_opt) {
                            const auto &texture = texture_opt.value();
                            image_info[index] = VkDescriptorImageInfo{
                                .sampler = texture->sampler,
                                .imageView = texture->image_view,
                                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            };
                            ++index;
                        }
                    }
                }

                VkWriteDescriptorSet write_2d{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set2_Textures_instance,
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = total_2d_descriptors,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = image_info.data(),
                };

                vkUpdateDescriptorSets(rtg.device, 1, &write_2d, 0, nullptr); 
            }

            { // update shadow map descriptors (SunShadowMap, SphereShadowMap, SpotShadowMap)
                std::vector<VkDescriptorImageInfo> sun_shadow_infos(sun_shadow_count);
                if (shadow_map_manager && !shadow_map_manager->sun_shadow_targets.empty()) {
                    const size_t available = shadow_map_manager->sun_shadow_targets.size();
                    for (uint32_t i = 0; i < sun_shadow_count; ++i) {
                        const auto &target = shadow_map_manager->sun_shadow_targets[i % available];
                        sun_shadow_infos[i] = VkDescriptorImageInfo{
                            .sampler = shadow_map_manager->sun_shadow_sampler,
                            .imageView = target.depth_array_view,
                            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        };
                    }
                } else {
                    // Use dummy shadow texture as fallback (no shadow)
                    for (auto &info : sun_shadow_infos) {
                        info = VkDescriptorImageInfo{
                            .sampler = texture_manager.dummy_shadow_sampler_2d,
                            .imageView = texture_manager.dummy_shadow_2d_array_view,
                            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        };
                    }
                }

                std::vector<VkDescriptorImageInfo> sphere_shadow_infos(sphere_shadow_count);
                if (shadow_map_manager && !shadow_map_manager->sphere_shadow_targets.empty()) {
                    const size_t available = shadow_map_manager->sphere_shadow_targets.size();
                    for (uint32_t i = 0; i < sphere_shadow_count; ++i) {
                        const auto &target = shadow_map_manager->sphere_shadow_targets[i % available];
                        sphere_shadow_infos[i] = VkDescriptorImageInfo{
                            .sampler = shadow_map_manager->sphere_shadow_sampler,
                            .imageView = target.depth_cube_view,
                            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        };
                    }
                } else {
                    // Use dummy shadow texture as fallback (no shadow)
                    for (auto &info : sphere_shadow_infos) {
                        info = VkDescriptorImageInfo{
                            .sampler = texture_manager.dummy_shadow_sampler_cube,
                            .imageView = texture_manager.dummy_shadow_cube_view,
                            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        };
                    }
                }

                std::vector<VkDescriptorImageInfo> spot_shadow_infos(spot_shadow_count);
                if (shadow_map_manager && !shadow_map_manager->spot_shadow_targets.empty()) {
                    const size_t available = shadow_map_manager->spot_shadow_targets.size();
                    for (uint32_t i = 0; i < spot_shadow_count; ++i) {
                        const auto &target = shadow_map_manager->spot_shadow_targets[i % available];
                        spot_shadow_infos[i] = VkDescriptorImageInfo{
                            .sampler = shadow_map_manager->spot_shadow_sampler,
                            .imageView = target.depth_image_view,
                            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        };
                    }
                } else {
                    // Use dummy shadow texture as fallback (no shadow)
                    for (auto &info : spot_shadow_infos) {
                        info = VkDescriptorImageInfo{
                            .sampler = texture_manager.dummy_shadow_sampler_2d,
                            .imageView = texture_manager.dummy_shadow_2d_view,
                            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        };
                    }
                }

                VkWriteDescriptorSet write_sun_shadow{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set2_Textures_instance,
                    .dstBinding = 2,
                    .dstArrayElement = 0,
                    .descriptorCount = sun_shadow_count,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = sun_shadow_infos.data(),
                };

                VkWriteDescriptorSet write_sphere_shadow{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set2_Textures_instance,
                    .dstBinding = 3,
                    .dstArrayElement = 0,
                    .descriptorCount = sphere_shadow_count,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = sphere_shadow_infos.data(),
                };

                VkWriteDescriptorSet write_spot_shadow{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set2_Textures_instance,
                    .dstBinding = 4,
                    .dstArrayElement = 0,
                    .descriptorCount = spot_shadow_count,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = spot_shadow_infos.data(),
                };

                std::array<VkWriteDescriptorSet, 3> writes{
                    write_sun_shadow,
                    write_sphere_shadow,
                    write_spot_shadow,
                };
                vkUpdateDescriptorSets(rtg.device, uint32_t(writes.size()), writes.data(), 0, nullptr);
            }
        }
    }

    { //create pipeline layout:
		std::array< VkDescriptorSetLayout, 3 > layouts{
			set0_Global,
            set1_Transforms,
            set2_Textures
		};

        VkPushConstantRange range{
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0,
			.size = sizeof(Push),
		};

		VkPipelineLayoutCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = uint32_t(layouts.size()),
			.pSetLayouts = layouts.data(),
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &range,
		};

		VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout) );
	}

    create_pipeline(rtg, render_pass, subpass, true);

    vkDestroyShaderModule(rtg.device, frag_module, nullptr);
    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
    frag_module = VK_NULL_HANDLE;
    vert_module = VK_NULL_HANDLE;

    std::vector<VkDescriptorType> global_binding_types(15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    global_binding_types[0] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    block_descriptor_configs.push_back(
        BlockDescriptorConfig{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .binding_types = std::move(global_binding_types),
        .layout = set0_Global, 
        .bindings_count = 15
    }); //Global
    block_descriptor_configs.push_back(
        BlockDescriptorConfig{
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
        .layout = set1_Transforms, 
        .bindings_count = 1
    }); //Transform

    block_descriptor_set_name_to_index = {
        {"Global", 0},
        {"Transforms", 1}
    };

    block_binding_name_to_index = {
        // Global
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
        // Transforms
        {"Transforms", 0},
    };
    
    pipeline_name_to_index["A3PBRPipeline"] = 2;
}

void A3PBRPipeline::destroy(RTG &rtg) {
    if(pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtg.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }

    if(layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if(set0_Global != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set0_Global, nullptr);
        set0_Global = VK_NULL_HANDLE;
    }

    if(set1_Transforms != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
        set1_Transforms = VK_NULL_HANDLE;
    }

    if(set2_Textures != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set2_Textures, nullptr);
        set2_Textures = VK_NULL_HANDLE;
    }

    if(set2_Textures_instance != VK_NULL_HANDLE) {
        set2_Textures_instance = VK_NULL_HANDLE;
    }

    if(sun_shadow_array_view != VK_NULL_HANDLE) {
        vkDestroyImageView(rtg.device, sun_shadow_array_view, nullptr);
        sun_shadow_array_view = VK_NULL_HANDLE;
    }
}
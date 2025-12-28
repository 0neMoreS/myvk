#include "TextureManager.hpp"

#include <cassert>

TextureManager::~TextureManager(){
    if(descriptor_pool != VK_NULL_HANDLE) {
        std::cerr << "[TextureManager] Warning: descriptor pool not destroyed before TextureManager destruction" << std::endl;
    }

    for(size_t pipeline_index = 0; pipeline_index < texture_bindings_by_pipeline.size(); ++pipeline_index) {
        for(size_t material_index = 0; material_index < texture_bindings_by_pipeline[pipeline_index].size(); ++material_index) {
            for(size_t slot_index = 0; slot_index < texture_bindings_by_pipeline[pipeline_index][material_index].size(); ++slot_index) {
                auto &binding_opt = texture_bindings_by_pipeline[pipeline_index][material_index][slot_index];
                if(binding_opt) {
                    if(binding_opt->descriptor_set != VK_NULL_HANDLE) {
                        std::cerr << "[TextureManager] Warning: descriptor set not destroyed before TextureManager destruction (pipeline " << pipeline_index << ", material " << material_index << ", slot " << slot_index << ")" << std::endl;
                    }
                }
            }
        }
    }

    if(raw_textures_by_material.size() > 0) {
        std::cerr << "[TextureManager] Warning: raw textures not destroyed before TextureManager destruction" << std::endl;
    }

    if(environment_cubemap_binding.has_value()) {
        std::cerr << "[TextureManager] Warning: environment cubemap not destroyed before TextureManager destruction" << std::endl;
    }

    if(ibl_cubemap_bindings.size() > 0) {
        std::cerr << "[TextureManager] Warning: IBL cubemaps not destroyed before TextureManager destruction" << std::endl;
    }

    if(brdf_LUT.texture) {
        std::cerr << "[TextureManager] Warning: BRDF LUT texture not destroyed before TextureManager destruction" << std::endl;
    }
}

void TextureManager::destroy(RTG &rtg) {
    if (descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(rtg.device, descriptor_pool, nullptr);
        descriptor_pool = VK_NULL_HANDLE;
    }

    for(size_t pipeline_index = 0; pipeline_index < texture_bindings_by_pipeline.size(); ++pipeline_index) {
        for(size_t material_index = 0; material_index < texture_bindings_by_pipeline[pipeline_index].size(); ++material_index) {
            for(size_t slot_index = 0; slot_index < texture_bindings_by_pipeline[pipeline_index][material_index].size(); ++slot_index) {
                auto &binding_opt = texture_bindings_by_pipeline[pipeline_index][material_index][slot_index];
                if(binding_opt) {
                    if(binding_opt->descriptor_set != VK_NULL_HANDLE) {
                        // Descriptor sets are freed when the descriptor pool is destroyed
                        binding_opt->descriptor_set = VK_NULL_HANDLE;
                    }
                }
            }
        }
    }

    for (auto &material_slots : raw_textures_by_material) {
        for (auto &texture_opt : material_slots) {
            if (texture_opt) {
                Texture2DLoader::destroy(*texture_opt, rtg);
            }
            texture_opt.reset();
        }
    }

    raw_textures_by_material.clear();

    // Clean up environment cubemaps
    if(environment_cubemap_binding.has_value()){
        TextureCubeLoader::destroy(environment_cubemap_binding->first, rtg);

        environment_cubemap_binding->first.reset();
        environment_cubemap_binding->second = VK_NULL_HANDLE;
    }

    if(ibl_cubemap_bindings.size()){
        for(auto &ibl_binding : ibl_cubemap_bindings){
            TextureCubeLoader::destroy(ibl_binding.first, rtg);
            ibl_binding.first.reset();
            ibl_binding.second = VK_NULL_HANDLE;
        }
        ibl_cubemap_bindings.clear();
    }


    Texture2DLoader::destroy(brdf_LUT.texture, rtg);
    brdf_LUT.texture.reset();
    brdf_LUT.descriptor_set = VK_NULL_HANDLE; 
}

void TextureManager::create(
    RTG &rtg,
    std::shared_ptr<S72Loader::Document> &doc,
    const std::vector<std::vector<Pipeline::TextureDescriptorConfig>> &&texture_descriptor_configs_by_pipeline,
    const VkDescriptorSetLayout cubemap_descriptor_layout
) {
    // Clean previous data
    destroy(rtg);

    { // Load raw textures from document
        raw_textures_by_material.resize(doc->materials.size());

        auto push_texture = [&](size_t material_index, TextureSlot slot, const std::optional<S72Loader::Texture> &texture_opt, const glm::vec3 &fallback_color) {
            auto &texture_element = raw_textures_by_material[material_index][slot];
            if (texture_opt.has_value()) {
                const auto &texture = texture_opt.value();
                std::string texture_path = s72_dir + texture.src;
                texture_element = Texture2DLoader::load_image(rtg.helpers, texture_path, VK_FILTER_LINEAR);
            } else {
                texture_element = Texture2DLoader::create_rgb_texture(rtg.helpers, fallback_color);
            }
        };

        for (const auto &material : doc->materials) {
            // diffuse / albedo
            std::optional<S72Loader::Texture> albedo_texture;
            glm::vec3 albedo_value{1.0f, 1.0f, 1.0f};
            size_t material_index = &material - &doc->materials[0];

            if (material.pbr && material.pbr->albedo_texture) {
                albedo_texture = material.pbr->albedo_texture;
            } else if (material.lambertian && material.lambertian->albedo_texture) {
                albedo_texture = material.lambertian->albedo_texture;
            }

            if (material.pbr && material.pbr->albedo_value) {
                albedo_value = *material.pbr->albedo_value;
            } else if (material.lambertian && material.lambertian->albedo_value) {
                albedo_value = *material.lambertian->albedo_value;
            }

            push_texture(material_index, TextureSlot::Albedo, albedo_texture, albedo_value);

            // normal
            push_texture(material_index, TextureSlot::Normal, material.normal_map, glm::vec3{0.0f, 0.0f, 0.0f});

            // displacement
            push_texture(material_index, TextureSlot::Displacement, material.displacement_map, glm::vec3{0.0f, 0.0f, 0.0f});

            // roughness
            std::optional<S72Loader::Texture> roughness_texture;
            float roughness_value = 1.0f;
            if (material.pbr) {
                if (material.pbr->roughness_texture) roughness_texture = material.pbr->roughness_texture;
                if (material.pbr->roughness_value) roughness_value = *material.pbr->roughness_value;
            }
            push_texture(material_index, TextureSlot::Roughness, roughness_texture, glm::vec3(roughness_value));

            // metallic
            std::optional<S72Loader::Texture> metallic_texture;
            float metallic_value = 0.0f;
            if (material.pbr) {
                if (material.pbr->metalness_texture) metallic_texture = material.pbr->metalness_texture;
                if (material.pbr->metalness_value) metallic_value = *material.pbr->metalness_value;
            }
            push_texture(material_index, TextureSlot::Metallic, metallic_texture, glm::vec3(metallic_value));
        }
    }

    { // Load environment and IBL cubemaps
        if(doc->environments.size()){
            const auto &env = doc->environments[0];
            const auto &radiance = env.radiance;
            std::string texture_path = s72_dir + radiance.src;
            environment_cubemap_binding.emplace(
                TextureCubeLoader::load_from_png_atlas(rtg.helpers, texture_path, VK_FILTER_LINEAR),
                VK_NULL_HANDLE
            );

            std::string ibl_base_path = texture_path.substr(0, texture_path.find_last_of('.'));

            ibl_cubemap_bindings.emplace_back(
                TextureCubeLoader::load_from_png_atlas(rtg.helpers, ibl_base_path + ".lambertian.png", VK_FILTER_LINEAR),
                VK_NULL_HANDLE
            );

            // std::vector<std::shared_ptr<TextureCubeLoader::Texture>> ibl_levels;
            // for(int level = 0; level < 5; ++level){
            //     std::string ibl_path = ibl_base_path + "." + std::to_string(level) + ".png";
            //     ibl_levels.push_back(
            //         TextureCubeLoader::load_from_png_atlas(rtg.helpers, ibl_path, VK_FILTER_LINEAR)
            //     );
            // }

            // For now, ignore mipmap...

            ibl_cubemap_bindings.emplace_back(
                TextureCubeLoader::load_from_png_atlas(rtg.helpers, ibl_base_path + ".1.png", VK_FILTER_LINEAR),
                VK_NULL_HANDLE
            );
        }
    }

    { // Load BRDF LUT texture
        std::string brdf_lut_path = s72_dir + "brdf_LUT.png";
        brdf_LUT.texture = Texture2DLoader::load_image(rtg.helpers, brdf_lut_path, VK_FILTER_LINEAR);
        brdf_LUT.descriptor_set = VK_NULL_HANDLE;

    }

    { // Create the descriptor pool based on actual textures loaded
        uint32_t total_descriptors = 0;
        for (const auto &material_slots : raw_textures_by_material) {
            for (const auto &texture_opt : material_slots) {
                if (texture_opt) {
                    ++total_descriptors;
                }
            }
        }
        
        // Add environment cubemaps to descriptor count
        if(environment_cubemap_binding.has_value())  ++total_descriptors;

        if (total_descriptors == 0) return; // No textures to allocate

        std::array<VkDescriptorPoolSize, 1> pool_sizes{
            VkDescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = total_descriptors,
            },
        };

        VkDescriptorPoolCreateInfo pool_create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = 0,
            .maxSets = total_descriptors,
            .poolSizeCount = uint32_t(pool_sizes.size()),
            .pPoolSizes = pool_sizes.data(),
        };

        VK( vkCreateDescriptorPool(rtg.device, &pool_create_info, nullptr, &descriptor_pool) );
    }

    { // Build texture bindings for each pipeline configuration
        if (texture_descriptor_configs_by_pipeline.empty()) return;
        if (descriptor_pool == VK_NULL_HANDLE) return;

        texture_bindings_by_pipeline.resize(texture_descriptor_configs_by_pipeline.size());

        // each pipeline
        for (size_t pipeline_idx = 0; pipeline_idx < texture_descriptor_configs_by_pipeline.size(); ++pipeline_idx) {
            const auto &pipeline_configs = texture_descriptor_configs_by_pipeline[pipeline_idx];
            auto &pipeline_bindings = texture_bindings_by_pipeline[pipeline_idx];
            pipeline_bindings.resize(raw_textures_by_material.size());

            // each descriptor config
            for (size_t config_idx = 0; config_idx < pipeline_configs.size(); ++config_idx) {
                const auto &config = pipeline_configs[config_idx];

                std::vector<TextureBinding*> bindings;
                bindings.reserve(pipeline_bindings.size());

                { // gather all textures for this slot across materials
                    const auto slot_index = config.slot;
                    for (size_t material_idx = 0; material_idx < raw_textures_by_material.size(); ++material_idx) {
                        auto &texture_opt = raw_textures_by_material[material_idx][slot_index];
                        if (!texture_opt) continue;

                        TextureBinding binding{
                            .texture = *texture_opt,
                            .descriptor_set = VK_NULL_HANDLE,
                        };
                        pipeline_bindings[material_idx][slot_index] = binding;
                        bindings.push_back(&(pipeline_bindings[material_idx][slot_index].value()));
                    }
                }

                if (bindings.empty()) continue;

                VkDescriptorSetAllocateInfo alloc_info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = descriptor_pool,
                    .descriptorSetCount = 1,
                    .pSetLayouts = &config.layout,
                };

                std::vector<VkDescriptorImageInfo> infos(bindings.size());
                std::vector<VkWriteDescriptorSet> writes(bindings.size());

                // allocate and write each descriptor set
                for (size_t j = 0; j < bindings.size(); ++j) {
                    auto *item = bindings[j];

                    VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &item->descriptor_set));

                    infos[j] = VkDescriptorImageInfo{
                        .sampler = item->texture->sampler,
                        .imageView = item->texture->image_view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    };
                    writes[j] = VkWriteDescriptorSet{
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = item->descriptor_set,
                        .dstBinding = 0,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .pImageInfo = &infos[j],
                    };
                }

                vkUpdateDescriptorSets(rtg.device, uint32_t(writes.size()), writes.data(), 0, nullptr);
            }
        }
    }

    { // Allocate descriptor set for environment cubemap
        if(environment_cubemap_binding.has_value() && environment_cubemap_binding->first && cubemap_descriptor_layout != VK_NULL_HANDLE) {
            VkDescriptorSetAllocateInfo alloc_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = descriptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &cubemap_descriptor_layout,
            };

            VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &environment_cubemap_binding->second));

            VkDescriptorImageInfo image_info{
                .sampler = environment_cubemap_binding->first->sampler,
                .imageView = environment_cubemap_binding->first->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };

            VkWriteDescriptorSet write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = environment_cubemap_binding->second,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &image_info,
            };

            vkUpdateDescriptorSets(rtg.device, 1, &write, 0, nullptr);
        }

    }

    { // Allocate descriptor set for IBL cubemap
        if(ibl_cubemap_bindings.size() > 0) {
            for(auto &ibl_binding : ibl_cubemap_bindings){
                VkDescriptorSetAllocateInfo alloc_info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = descriptor_pool,
                    .descriptorSetCount = 1,
                    .pSetLayouts = &cubemap_descriptor_layout,
                };

                VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &ibl_binding.second));

                VkDescriptorImageInfo image_info{
                    .sampler = ibl_binding.first->sampler,
                    .imageView = ibl_binding.first->image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };

                VkWriteDescriptorSet write{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = ibl_binding.second,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &image_info,
                };

                vkUpdateDescriptorSets(rtg.device, 1, &write, 0, nullptr);
            }
        }
    }

    { // Allocate descriptor set for brdf_LUT
        VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &cubemap_descriptor_layout,
        };

        VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &brdf_LUT.descriptor_set));

        VkDescriptorImageInfo image_info{
            .sampler = brdf_LUT.texture->sampler,
            .imageView = brdf_LUT.texture->image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = brdf_LUT.descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_info,
        };

        vkUpdateDescriptorSets(rtg.device, 1, &write, 0, nullptr);
    }
}
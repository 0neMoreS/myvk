#include "TextureManager.hpp"

#include <cassert>

TextureManager::~TextureManager(){
    if(descriptor_pool != VK_NULL_HANDLE) {
        std::cerr << "[TextureManager] Warning: descriptor pool not destroyed before TextureManager destruction" << std::endl;
    }
}

void TextureManager::destroy(RTG &rtg) {
    if (descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(rtg.device, descriptor_pool, nullptr);
        descriptor_pool = VK_NULL_HANDLE;
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
}

void TextureManager::create(
    RTG &rtg,
    std::shared_ptr<S72Loader::Document> &doc,
    const std::vector<std::vector<Pipeline::TextureDescriptorConfig>> &&texture_descriptor_configs_by_pipeline
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
                texture_element = Texture2DLoader::load_png(rtg.helpers, texture_path, VK_FILTER_LINEAR);
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

            push_texture(material_index, TextureSlot::Diffuse, albedo_texture, albedo_value);

            // normal
            push_texture(material_index, TextureSlot::Normal, material.normal_map, glm::vec3{0.0f, 0.0f, 0.0f});

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

    { // Create the descriptor pool based on actual textures loaded
        uint32_t total_descriptors = 0;
        for (const auto &material_slots : raw_textures_by_material) {
            for (const auto &texture_opt : material_slots) {
                if (texture_opt) {
                    ++total_descriptors;
                }
            }
        }

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
                    const auto slot_index = static_cast<size_t>(config.slot);
                    for (size_t material_idx = 0; material_idx < raw_textures_by_material.size(); ++material_idx) {
                        auto &texture_opt = raw_textures_by_material[material_idx][slot_index];
                        if (!texture_opt) continue;

                        TextureBinding binding{
                            .texture = *texture_opt,
                            .descriptor_set = VK_NULL_HANDLE,
                        };
                        pipeline_bindings[material_idx][slot_index] = binding;
                        bindings.push_back(&(*pipeline_bindings[material_idx][slot_index]));
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
}
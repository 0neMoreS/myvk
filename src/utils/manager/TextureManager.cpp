#include "TextureManager.hpp"

#include <cassert>

void TextureManager::destroy(RTG &rtg) {
    for (auto &material_slots : raw_2d_textures_by_material) {
        for (auto &texture_opt : material_slots) {
            if (texture_opt) {
                Texture2DLoader::destroy(*texture_opt, rtg);
            }
            texture_opt.reset();
        }
    }

    raw_2d_textures_by_material.clear();

    for (auto &cubemap_texture : raw_environment_cubemap_texture) {
        TextureCubeLoader::destroy(cubemap_texture, rtg);
    }
    raw_environment_cubemap_texture.clear();

    Texture2DLoader::destroy(raw_brdf_LUT_texture, rtg);
    raw_brdf_LUT_texture = nullptr;

    if(texture_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(rtg.device, texture_descriptor_pool, nullptr);
        texture_descriptor_pool = VK_NULL_HANDLE;
    }
}

void TextureManager::create(
    RTG &rtg,
    std::shared_ptr<S72Loader::Document> &doc,
    uint32_t pipeline_count
) {
    // Clean previous data
    destroy(rtg);

    { // Load raw textures from document
        raw_2d_textures_by_material.resize(doc->materials.size());

        auto push_texture = [&](size_t material_index, TextureSlot slot, const std::optional<S72Loader::Texture> &texture_opt, const glm::vec3 &fallback_color) {
            auto &texture_element = raw_2d_textures_by_material[material_index][slot];
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

            // normal
            push_texture(material_index, TextureSlot::Normal, material.normal_map, glm::vec3{0.5f, 0.5f, 1.0f});

            // displacement
            push_texture(material_index, TextureSlot::Displacement, material.displacement_map, glm::vec3{0.0f, 0.0f, 0.0f});

            // albedo
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

    {
        bool has_cubemap = doc->environments.size() > 0;

        { // Load environment and IBL cubemaps
            if(has_cubemap){
                const auto &env = doc->environments[0];
                const auto &radiance = env.radiance;
                std::string texture_path = s72_dir + radiance.src;
                raw_environment_cubemap_texture.resize(3);
                
                raw_environment_cubemap_texture[0] = TextureCubeLoader::load_from_png_atlas(rtg.helpers, texture_path, VK_FILTER_LINEAR, 1);
                raw_environment_cubemap_texture[1] = TextureCubeLoader::load_from_png_atlas(rtg.helpers, texture_path, VK_FILTER_LINEAR, 1);
                raw_environment_cubemap_texture[2] = TextureCubeLoader::load_from_png_atlas(rtg.helpers, texture_path, VK_FILTER_LINEAR, 5);
            }
        }

        { // Load BRDF LUT texture
            std::string brdf_lut_path = s72_dir + "brdf_LUT.png";
            raw_brdf_LUT_texture = Texture2DLoader::load_image(rtg.helpers, brdf_lut_path, VK_FILTER_LINEAR);
        }

        { // the descriptor pool for texture descriptors
            uint32_t total_2d_descriptors = has_cubemap ? 1 : 0; // BRDF LUT
            uint32_t total_cubemap_descriptors = has_cubemap ? 2 : 0; // IrradianceMap + PrefilterMap

            for (const auto &material_slots : raw_2d_textures_by_material) {
                for (const auto &texture_opt : material_slots) {
                    if (texture_opt) {
                        ++total_2d_descriptors;
                    }
                }
            }

            std::array<VkDescriptorPoolSize, 1> pool_sizes{
                VkDescriptorPoolSize{
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = total_2d_descriptors + total_cubemap_descriptors,
                },
            };

            VkDescriptorPoolCreateInfo pool_create_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = pipeline_count,
                .poolSizeCount = uint32_t(pool_sizes.size()),
                .pPoolSizes = pool_sizes.data(),
            };

            VK( vkCreateDescriptorPool(rtg.device, &pool_create_info, nullptr, &texture_descriptor_pool) );
        }
    }
}

TextureManager::~TextureManager() {
    assert(texture_descriptor_pool == VK_NULL_HANDLE);
}
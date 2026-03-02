#pragma once

#include "RTG.hpp"

#include "S72Loader.hpp"
#include "Texture2DLoader.hpp"
#include "TextureCubeLoader.hpp"
#include "VK.hpp"

#include <optional>
#include <cassert>

class TextureManager {
    public:
        VkDescriptorPool texture_descriptor_pool = VK_NULL_HANDLE;
        uint32_t shadow_sun_light_count = 0;
        uint32_t shadow_sphere_light_count = 0;
        uint32_t shadow_spot_light_count = 0;
        uint32_t sun_shadow_descriptor_count = 1;
        uint32_t sphere_shadow_descriptor_count = 1;
        uint32_t spot_shadow_descriptor_count = 1;
        // Raw textures from document: textures_by_material[material_index][texture_slot]
        std::vector< std::array< std::optional<std::unique_ptr<Texture2DLoader::Texture>>, 5 > > raw_2d_textures_by_material;

        // 0: cubemaps, 1: irradiance map, 2 : prefilter map(with mipmaps)
        std::vector<std::unique_ptr<TextureCubeLoader::Texture>> raw_environment_cubemap_texture;

        // BRDF LUT texture
        std::unique_ptr<Texture2DLoader::Texture> raw_brdf_LUT_texture;
            
        void create(
            RTG & rtg,
            std::shared_ptr<S72Loader::Document> &doc,
            uint32_t pipeline_count,
            uint32_t shadow_sun_count = 0,
            uint32_t shadow_sphere_count = 0,
            uint32_t shadow_spot_count = 0
        );
        void destroy(RTG &rtg);

        TextureManager() = default;
        ~TextureManager();
};
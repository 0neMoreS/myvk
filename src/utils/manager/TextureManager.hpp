#pragma once

#include "RTG.hpp"

#include "S72Loader.hpp"
#include "Texture2DLoader.hpp"
#include "TextureCubeLoader.hpp"
#include "VK.hpp"
#include "Pipeline.hpp"

#include <optional>

class TextureManager {
    public:
        // Raw textures from document: textures_by_material[material_index][texture_slot]
        std::vector< std::array< std::optional<std::shared_ptr<Texture2DLoader::Texture>>, 5 > > raw_2d_textures_by_material;

        // 0: cubemaps, 1: irradiance map, 2 : prefilter map(with mipmaps)
        std::vector<std::shared_ptr<TextureCubeLoader::Texture>> raw_environment_cubemap_texture;

        // BRDF LUT texture
        std::shared_ptr<Texture2DLoader::Texture> raw_brdf_LUT_texture;
            
        void create(
            RTG & rtg,
            std::shared_ptr<S72Loader::Document> &doc
        );
        void destroy(RTG &rtg);

        TextureManager() = default;
        ~TextureManager() = default;
};
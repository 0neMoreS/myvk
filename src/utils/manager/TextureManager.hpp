#pragma once

#include "RTG.hpp"

#include "S72Loader.hpp"
#include "Texture2DLoader.hpp"
#include "VK.hpp"
#include "Pipeline.hpp"

#include <optional>

class TextureManager {
    public:
        struct TextureBinding {
            std::shared_ptr<Texture2DLoader::Texture> texture;
            VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        };

        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

        // Raw textures from document: textures_by_material[material_index][texture_slot]
        std::vector< std::array< std::optional<std::shared_ptr<Texture2DLoader::Texture>>, 4 > > raw_textures_by_material;
        
        void create(RTG & rtg, std::shared_ptr<S72Loader::Document> &doc);
        void destroy(RTG &rtg);
        
        // Build texture bindings for a specific pipeline configuration
        std::vector< std::array< std::optional< TextureBinding >, 4 > > build_texture_bindings(
            RTG & rtg,
            const std::vector<Pipeline::TextureDescriptorConfig> &texture_descriptor_configs
        );

        TextureManager() = default;
        ~TextureManager();
};
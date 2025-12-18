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
        // Texture bindings by pipeline: texture_bindings_by_pipeline[pipeline_index][material_index][texture_slot]
        std::vector< std::vector< std::array< std::optional< TextureBinding >, 4 > > > texture_bindings_by_pipeline;

        // Raw textures from document: textures_by_material[material_index][texture_slot]
        std::vector< std::array< std::optional<std::shared_ptr<Texture2DLoader::Texture>>, 4 > > raw_textures_by_material;
        
        void create(RTG & rtg,
            std::shared_ptr<S72Loader::Document> &doc,
            const std::vector<std::vector<Pipeline::TextureDescriptorConfig>> &&texture_descriptor_configs_by_pipeline);
        void destroy(RTG &rtg);

        TextureManager() = default;
        ~TextureManager();
};
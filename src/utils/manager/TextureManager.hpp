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
        struct TextureBinding {
            std::shared_ptr<Texture2DLoader::Texture> texture;
            VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        };

        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        // Texture bindings by pipeline: texture_bindings_by_pipeline[pipeline_index][material_index][texture_slot]
        std::vector< std::vector< std::array< std::optional< TextureBinding >, 5 > > > texture_bindings_by_pipeline;

        // Raw textures from document: textures_by_material[material_index][texture_slot]
        std::vector< std::array< std::optional<std::shared_ptr<Texture2DLoader::Texture>>, 5 > > raw_textures_by_material;
        
        // Environment cubemaps: only one per doc
        std::optional<std::pair<std::shared_ptr<TextureCubeLoader::Texture>, VkDescriptorSet>> environment_cubemap_binding;
        
        void create(RTG & rtg,
            std::shared_ptr<S72Loader::Document> &doc,
            const std::vector<std::vector<Pipeline::TextureDescriptorConfig>> &&texture_descriptor_configs_by_pipeline,
            const VkDescriptorSetLayout cubemap_descriptor_layout = VK_NULL_HANDLE);
        void destroy(RTG &rtg);

        TextureManager() = default;
        ~TextureManager();
};
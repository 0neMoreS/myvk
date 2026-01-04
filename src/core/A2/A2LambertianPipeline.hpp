#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"
#include "TextureManager.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

struct A2LambertianPipeline : Pipeline {
    // Global PV matrix, light, update per-frame
    VkDescriptorSetLayout set0_Global = VK_NULL_HANDLE;

    // Per-instance transforms matrix, update per-draw
    VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;

    // Global IBL and 2D (including all 2d textures, an instance will use the material_index to get the corresponding descriptor for it) texture descriptor sets, no update
    VkDescriptorSetLayout set2_Textures = VK_NULL_HANDLE;
    VkDescriptorSet set2_Textures_instance = VK_NULL_HANDLE;
    /*
        IrradianceMap
        PrefilterMap
        PBRTLUT
        {
            NormalTexture
            DisplacementTexture
            AlbedoTexture
            RoughnessTexture
            MetalnessTexture
        }
    */

    struct Push{
        uint32_t MATERIAL_INDEX;
    };

    //no push constants
    void create(
		class RTG &, 
		VkRenderPass render_pass, 
		uint32_t subpass,
		const TextureManager& texture_manager
	) override;
    void destroy(RTG &rtg) override;

    A2LambertianPipeline() = default;
    ~A2LambertianPipeline();
};
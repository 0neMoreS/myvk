#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"
#include "TextureManager.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

struct A2PBRPipeline : Pipeline {
    // Global PV matrix, light, update per-frame
    VkDescriptorSetLayout set0_Global = VK_NULL_HANDLE;

    // Per-instance transforms matrix, update per-draw
    VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;

    // Global IBL and 2D (including all 2d textures, an instance will use the material_index to get the corresponding descriptor for it) texture descriptor sets, no update
    VkDescriptorSetLayout set2_Textures = VK_NULL_HANDLE;
    VkDescriptorSet set2_Textures_instance = VK_NULL_HANDLE;
    VkDescriptorPool set2_Textures_instance_pool = VK_NULL_HANDLE;
    /*
        PBRTLUT
        IrradianceMap
        PrefilterMap
        {
            NormalTexture
            DisplacementTexture
            AlbedoTexture
            RoughnessTexture
            MetalnessTexture
        }
    */

    struct Push{
        uint32_t material_index;
    };

    //types for descriptors:
    struct Light {
        struct { float x, y, z, padding_; } LIGHT_POSITION;
        struct { float r, g, b, padding_; } LIGHT_ENERGY;
    };
    static_assert(sizeof(Light) == 4*4 + 4*4, "Light is the expected size.");

    //types for descriptors:
    struct Transform {
        glm::mat4 MODEL;
        glm::mat4 MODEL_NORMAL;
    };
    static_assert(sizeof(Transform) == 16*4 + 16*4, "Transform is the expected size.");

    //no push constants
    void create(
		class RTG &, 
		VkRenderPass render_pass, 
		uint32_t subpass,
		const TextureManager& texture_manager
	) override;
    void destroy(RTG &rtg) override;

    A2PBRPipeline() = default;
    ~A2PBRPipeline();
};
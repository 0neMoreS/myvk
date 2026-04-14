#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"
#include "TextureManager.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

struct SSAOPBRPipeline : Pipeline {
    // Global PV matrix, light, update pointer once, write buffer per frame
    VkDescriptorSetLayout set0_Global = VK_NULL_HANDLE;

    // Per-instance transforms matrix, update pointer and write buffer per frame
    VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;

    // Global IBL and 2D texture descriptor sets, update pointer once, no buffer writing
    // Shadow map, update pointer once, write buffer per frame
    // G-buffer update pointer per frame, write buffer per frame
    VkDescriptorSetLayout set2_Textures = VK_NULL_HANDLE;
    VkDescriptorSet set2_Textures_instance = VK_NULL_HANDLE;
    VkImageView sun_shadow_array_view = VK_NULL_HANDLE;
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
        SunShadowMap
        SphereShadowMap 
        SpotShadowMap
        write_gbuffer_position_depth
        write_gbuffer_normal
        write_gbuffer_albedo
        write_gbuffer_pbr
    */

    struct Push{
        uint32_t MATERIAL_INDEX;
    };

    //no push constants
    void create(
		RTG &, 
		VkRenderPass render_pass, 
		uint32_t subpass,
        const ManagerContext& context
	) override;

    void update_gbuffer_descriptors(
        VkDevice device,
        VkSampler sampler,
        VkImageView position_depth,
        VkImageView normal,
        VkImageView albedo,
        VkImageView pbr
    );

    void destroy(RTG &rtg) override;

    SSAOPBRPipeline() = default;
    ~SSAOPBRPipeline();
};
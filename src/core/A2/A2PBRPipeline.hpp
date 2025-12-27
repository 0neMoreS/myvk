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
    // Matrix descriptor sets
    VkDescriptorSetLayout set0_PV = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;
    VkDescriptorSetLayout set2_Light = VK_NULL_HANDLE;
    // Texture descriptor sets
    VkDescriptorSetLayout set3_NormalTexture = VK_NULL_HANDLE;
    VkDescriptorSetLayout set4_DisplacementTexture = VK_NULL_HANDLE;
    VkDescriptorSetLayout set5_AlbedoTexture = VK_NULL_HANDLE;
    VkDescriptorSetLayout set6_RoughnessTexture = VK_NULL_HANDLE;
    VkDescriptorSetLayout set7_MetalnessTexture = VK_NULL_HANDLE;
    // IBL descriptor sets
    VkDescriptorSetLayout set8_IrradianceMap = VK_NULL_HANDLE;
    VkDescriptorSetLayout set9_PrefilterMap = VK_NULL_HANDLE;
    VkDescriptorSetLayout set10_BRDFLUT = VK_NULL_HANDLE;

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
    void create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass) override;
    void destroy(RTG &rtg) override;

    A2PBRPipeline() = default;
    ~A2PBRPipeline();
};
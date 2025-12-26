#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"
#include "TextureManager.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

struct A2EnvironmentPipeline : Pipeline {
    // type definitions
    VkDescriptorSetLayout set0_World = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;
    VkDescriptorSetLayout set2_TEXTURE = VK_NULL_HANDLE;
    VkDescriptorSetLayout set3_CUBEMAP = VK_NULL_HANDLE;

    //types for descriptors:
    struct World {
        struct { float x, y, z, padding_; } SKY_DIRECTION;
        struct { float r, g, b, padding_; } SKY_ENERGY;
        struct { float x, y, z, padding_; } SUN_DIRECTION;
        struct { float r, g, b, padding_; } SUN_ENERGY;
    };
    static_assert(sizeof(World) == 4*4 + 4*4 + 4*4 + 4*4, "World is the expected size.");

    //types for descriptors:
    struct Transform {
        glm::mat4 PERSPECTIVE;
        glm::mat4 VIEW;
        glm::mat4 MODEL;
        glm::mat4 MODEL_NORMAL;
    };
    static_assert(sizeof(Transform) == 16*4 + 16*4 + 16*4 + 16*4, "Transform is the expected size.");

    //no push constants
    void create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass) override;
    void destroy(RTG &rtg) override;

    A2EnvironmentPipeline() = default;
    ~A2EnvironmentPipeline();
    
    // Cubemap data structure
    struct Cubemap {
        glm::vec4 reserved; // For future use, maintains alignment
    };
    static_assert(sizeof(Cubemap) == 16, "Cubemap is the expected size.");
};
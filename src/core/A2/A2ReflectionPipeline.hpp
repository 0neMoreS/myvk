#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"
#include "TextureManager.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <cassert>

struct A2ReflectionPipeline : Pipeline {
    // type definitions
    VkDescriptorSetLayout set0_PV = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;
    VkDescriptorSetLayout set2_CUBEMAP = VK_NULL_HANDLE;
    VkDescriptorSet set2_CUBEMAP_instance = VK_NULL_HANDLE;

    //no push constants
    void create(
        class RTG &rtg, 
        VkRenderPass render_pass, 
        uint32_t subpass,
        const TextureManager& texture_manager
    ) override;
    void destroy(RTG &rtg) override;

    A2ReflectionPipeline() = default;
    ~A2ReflectionPipeline();
};
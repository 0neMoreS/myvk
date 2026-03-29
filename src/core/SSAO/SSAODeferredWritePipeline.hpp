#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"
#include "TextureManager.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

struct SSAODeferredWritePipeline : Pipeline {
    VkDescriptorSetLayout set0_PV = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;
    VkDescriptorSetLayout set2_Textures = VK_NULL_HANDLE;
    VkDescriptorSet set2_Textures_instance = VK_NULL_HANDLE;

    struct Push {
        uint32_t MATERIAL_INDEX;
    };

    void create(
        RTG &,
        VkRenderPass render_pass,
        uint32_t subpass,
        const ManagerContext& context
    ) override;
    void destroy(RTG &rtg) override;

    SSAODeferredWritePipeline() = default;
    ~SSAODeferredWritePipeline();
};

#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"

#include <glm/glm.hpp>

struct SSAOToneMappingPipeline : Pipeline {
    // HDR texture descriptor set layout
    VkDescriptorSetLayout set0_HDRTexture = VK_NULL_HANDLE;
    VkDescriptorSet set0_HDRTexture_instance = VK_NULL_HANDLE;

    void create(
        RTG &,
        VkRenderPass render_pass,
        uint32_t subpass,
        const ManagerContext& context
    ) override;

    struct Push{
        float EXPOSURE;
        uint32_t METHOD; // 0: linear, 1: aces
    };

    void destroy(RTG &rtg) override;

    SSAOToneMappingPipeline() = default;
    ~SSAOToneMappingPipeline();
};

#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"

#include <glm/glm.hpp>

struct A2ToneMappingPipeline : Pipeline {
    // HDR texture descriptor set layout
    VkDescriptorSetLayout set0_HDRTexture = VK_NULL_HANDLE;
    VkDescriptorSet set0_HDRTexture_instance = VK_NULL_HANDLE;

    void create(
        RTG &,
        VkRenderPass render_pass,
        uint32_t subpass,
        const TextureManager& texture_manager
    ) override;

    struct Push{
        float EXPOSURE;
        uint32_t METHOD; // 0: linear, 1: aces
    };

    void destroy(RTG &rtg) override;

    A2ToneMappingPipeline() = default;
    ~A2ToneMappingPipeline();
};

#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"

struct SSAOAmbientOcclusionPipeline : Pipeline {
    VkDescriptorSetLayout set0_PV = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_GBuffer = VK_NULL_HANDLE;
    VkDescriptorSet set1_GBuffer_instance = VK_NULL_HANDLE;
    VkDescriptorSetLayout set2_Noise = VK_NULL_HANDLE;
    VkDescriptorSet set2_Noise_instance = VK_NULL_HANDLE;

    void create(
        RTG &,
        VkRenderPass render_pass,
        uint32_t subpass,
        const ManagerContext& context
    ) override;

    void destroy(RTG &rtg) override;

    SSAOAmbientOcclusionPipeline() = default;
    ~SSAOAmbientOcclusionPipeline();
};

#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"

struct SSAOAOBlurPipeline : Pipeline {
    VkDescriptorSetLayout set0_AOInput = VK_NULL_HANDLE;
    VkDescriptorSet set0_AOInput_instance = VK_NULL_HANDLE;

    void create(
        RTG &,
        VkRenderPass render_pass,
        uint32_t subpass,
        const ManagerContext &context
    ) override;

    void destroy(RTG &rtg) override;

    SSAOAOBlurPipeline() = default;
    ~SSAOAOBlurPipeline();
};

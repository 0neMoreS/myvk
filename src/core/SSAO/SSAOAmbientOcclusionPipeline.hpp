#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"

struct SSAOAmbientOcclusionPipeline : Pipeline {
    VkDescriptorSetLayout set0_GBuffer = VK_NULL_HANDLE;
    VkDescriptorSet set0_GBuffer_instance = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_Noise = VK_NULL_HANDLE;
    VkDescriptorSet set1_Noise_instance = VK_NULL_HANDLE;

    Helpers::AllocatedImage noise_image;
    VkImageView noise_image_view = VK_NULL_HANDLE;
    VkSampler noise_sampler = VK_NULL_HANDLE;

    struct Push {
        float RADIUS_PIXELS;
        float DEPTH_BIAS;
        float POWER;
    };

    VkDescriptorImageInfo get_noise_descriptor_image_info() const;

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

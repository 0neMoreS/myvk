#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"

#include <vector>

struct SSAOAmbientOcclusionPipeline : Pipeline {
    VkDescriptorPool pv_descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout set0_PV = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> set0_PV_instances;
    VkDescriptorSetLayout set1_GBuffer = VK_NULL_HANDLE;
    VkDescriptorSet set1_GBuffer_instance = VK_NULL_HANDLE;
    VkDescriptorSetLayout set2_Noise = VK_NULL_HANDLE;
    VkDescriptorSet set2_Noise_instance = VK_NULL_HANDLE;

    Helpers::AllocatedImage noise_image;
    VkImageView noise_image_view = VK_NULL_HANDLE;
    VkSampler noise_sampler = VK_NULL_HANDLE;

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

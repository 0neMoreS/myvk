#pragma once

#include "RTG.hpp"
#include "VK.hpp"

#include "RenderPassManager.hpp"

#include <vulkan/vulkan.h>

struct GBufferManager {
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    VkSampler sampler = VK_NULL_HANDLE;
    VkImageView depth_view = VK_NULL_HANDLE;
    VkImageView albedo_view = VK_NULL_HANDLE;
    VkImageView normal_view = VK_NULL_HANDLE;
    VkImageView pbr_view = VK_NULL_HANDLE;

    Helpers::AllocatedImage depth_image;
    Helpers::AllocatedImage albedo_image;
    Helpers::AllocatedImage normal_image;
    Helpers::AllocatedImage pbr_image;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    void create(RTG &rtg, VkDescriptorPool descriptor_pool, VkDescriptorSetLayout descriptor_set_layout);

    void set_views_and_sampler(
        VkSampler in_sampler,
        VkImageView depth,
        VkImageView albedo,
        VkImageView normal,
        VkImageView pbr
    );

    void create_targets(
        RTG &rtg,
        VkExtent2D extent,
        VkRenderPass gbuffer_render_pass
    );

    void destroy_targets(RTG &rtg);

    void update(VkDevice device);
    void destroy(RTG &rtg);
};
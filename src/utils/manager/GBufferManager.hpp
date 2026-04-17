#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"

#include <vulkan/vulkan.h>
#include <array>

struct GBufferManager {
    VkImageView depth_view = VK_NULL_HANDLE;
    VkImageView albedo_view = VK_NULL_HANDLE;
    VkImageView normal_view = VK_NULL_HANDLE;
    VkImageView pbr_view = VK_NULL_HANDLE;

    Helpers::AllocatedImage depth_image;
    Helpers::AllocatedImage albedo_image;
    Helpers::AllocatedImage normal_image;
    Helpers::AllocatedImage pbr_image;

    VkSampler gbuffer_sampler = VK_NULL_HANDLE;
    VkFramebuffer gbuffer_framebuffer = VK_NULL_HANDLE;

    void create(RTG &rtg, RenderPassManager &render_pass_manager, VkExtent2D const &extent);
    void destroy(RTG &rtg);

    std::array<VkDescriptorImageInfo, 4> get_descriptor_image_infos() const;

    GBufferManager() = default;
    ~GBufferManager();
};
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
    VkImageView ao_view = VK_NULL_HANDLE;
    VkImageView ao_blur_view = VK_NULL_HANDLE;

    Helpers::AllocatedImage depth_image;
    Helpers::AllocatedImage albedo_image;
    Helpers::AllocatedImage normal_image;
    Helpers::AllocatedImage ao_image;
    Helpers::AllocatedImage ao_blur_image;

    VkSampler gbuffer_sampler = VK_NULL_HANDLE;
    VkFramebuffer gbuffer_framebuffer = VK_NULL_HANDLE;
    VkFramebuffer ao_framebuffer = VK_NULL_HANDLE;
    VkFramebuffer ao_blur_framebuffer = VK_NULL_HANDLE;

    void create(RTG &rtg, RenderPassManager &render_pass_manager, VkExtent2D const &extent);
    void destroy(RTG &rtg);

    std::array<VkDescriptorImageInfo, 3> get_descriptor_image_infos() const;
    VkDescriptorImageInfo get_ao_descriptor_image_info() const;

    GBufferManager() = default;
    ~GBufferManager();
};
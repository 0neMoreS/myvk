#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"
#include "RenderTarget.hpp"

#include <array>

struct GBufferManager {
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    VkFormat albedo_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat normal_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat ao_format = VK_FORMAT_R8_UNORM;

    std::array<VkClearValue, 3> gbuffer_clears;
    std::array<VkClearValue, 1> ao_clears;

    BufferRenderTarget::Target2D depth_target;
    BufferRenderTarget::Target2D albedo_target;
    BufferRenderTarget::Target2D normal_target;
    BufferRenderTarget::Target2D ao_target;
    BufferRenderTarget::Target2D ao_blur_target;

    VkSampler gbuffer_sampler = VK_NULL_HANDLE;
    VkFramebuffer gbuffer_framebuffer = VK_NULL_HANDLE;

    void create(RTG &rtg, RenderPassManager &render_pass_manager);
    void on_swapchain(RTG &rtg, RenderPassManager &render_pass_manager, VkExtent2D const &extent);
    void destroy(RTG &rtg);

    std::array<VkDescriptorImageInfo, 3> get_descriptor_image_infos() const;
    VkDescriptorImageInfo get_ao_descriptor_image_info() const;

    GBufferManager() = default;
    ~GBufferManager();
};

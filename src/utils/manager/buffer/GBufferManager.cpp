#include "GBufferManager.hpp"

#include "RenderTarget.hpp"

#include <array>
#include <cassert>
#include <iostream>

void GBufferManager::create(RTG &rtg, RenderPassManager &) {
    if (depth_format == VK_FORMAT_UNDEFINED) {
        depth_format = rtg.helpers.find_image_format(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32 },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    gbuffer_clears = {
        VkClearValue{ .color{ .float32{0.0f, 0.0f, 0.0f, 0.0f} } },
        VkClearValue{ .color{ .float32{0.0f, 0.0f, 1.0f, 1.0f} } },
        VkClearValue{ .depthStencil{ .depth = rtg.configuration.reverse_z ? 0.0f : 1.0f, .stencil = 0 } },
    };

    ao_clears = {
        VkClearValue{ .color{ .float32{0.0f, 0.0f, 0.0f, 1.0f} } },
    };

    if (gbuffer_sampler == VK_NULL_HANDLE) {
        VkSamplerCreateInfo sampler_info{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1.0f,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };
        VK(vkCreateSampler(rtg.device, &sampler_info, nullptr, &gbuffer_sampler));
    }
}

void GBufferManager::on_swapchain(RTG &rtg, RenderPassManager &render_pass_manager, VkExtent2D const &extent) {
    assert(extent.width > 0 && extent.height > 0);
    assert(render_pass_manager.gbuffer_render_pass != VK_NULL_HANDLE);
    assert(render_pass_manager.ao_render_pass != VK_NULL_HANDLE);
    assert(depth_format != VK_FORMAT_UNDEFINED);

    if (gbuffer_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rtg.device, gbuffer_framebuffer, nullptr);
        gbuffer_framebuffer = VK_NULL_HANDLE;
    }

    BufferRenderTarget::destroy_target_2d(rtg, depth_target);
    BufferRenderTarget::destroy_target_2d(rtg, albedo_target);
    BufferRenderTarget::destroy_target_2d(rtg, normal_target);
    BufferRenderTarget::destroy_target_2d(rtg, ao_target);
    BufferRenderTarget::destroy_target_2d(rtg, ao_blur_target);

    depth_target = BufferRenderTarget::create_target_2d(
        rtg,
        extent,
        depth_format,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_NULL_HANDLE
    );

    albedo_target = BufferRenderTarget::create_target_2d(
        rtg,
        extent,
        albedo_format,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_NULL_HANDLE
    );

    normal_target = BufferRenderTarget::create_target_2d(
        rtg,
        extent,
        normal_format,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_NULL_HANDLE
    );

    ao_target = BufferRenderTarget::create_target_2d(
        rtg,
        extent,
        ao_format,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        render_pass_manager.ao_render_pass
    );

    ao_blur_target = BufferRenderTarget::create_target_2d(
        rtg,
        extent,
        ao_format,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        render_pass_manager.ao_render_pass
    );

    std::vector<VkImageView> gbuffer_attachments{
        albedo_target.view,
        normal_target.view,
        depth_target.view,
    };
    gbuffer_framebuffer = BufferRenderTarget::create_framebuffer(
        rtg,
        render_pass_manager.gbuffer_render_pass,
        extent,
        gbuffer_attachments
    );
}

void GBufferManager::destroy(RTG &rtg) {
    if (gbuffer_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rtg.device, gbuffer_framebuffer, nullptr);
        gbuffer_framebuffer = VK_NULL_HANDLE;
    }

    BufferRenderTarget::destroy_target_2d(rtg, depth_target);
    BufferRenderTarget::destroy_target_2d(rtg, albedo_target);
    BufferRenderTarget::destroy_target_2d(rtg, normal_target);
    BufferRenderTarget::destroy_target_2d(rtg, ao_target);
    BufferRenderTarget::destroy_target_2d(rtg, ao_blur_target);

    if (gbuffer_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.device, gbuffer_sampler, nullptr);
        gbuffer_sampler = VK_NULL_HANDLE;
    }
}

std::array<VkDescriptorImageInfo, 3> GBufferManager::get_descriptor_image_infos() const {
    return {
        VkDescriptorImageInfo{
            .sampler = gbuffer_sampler,
            .imageView = depth_target.view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = gbuffer_sampler,
            .imageView = albedo_target.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = gbuffer_sampler,
            .imageView = normal_target.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };
}

VkDescriptorImageInfo GBufferManager::get_ao_descriptor_image_info() const {
    return VkDescriptorImageInfo{
        .sampler = gbuffer_sampler,
        .imageView = ao_target.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
}

GBufferManager::~GBufferManager() {
    if (gbuffer_framebuffer != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: gbuffer_framebuffer not destroyed" << std::endl;
    }
    if (depth_target.view != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: depth_view not destroyed" << std::endl;
    }
    if (albedo_target.view != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: albedo_view not destroyed" << std::endl;
    }
    if (normal_target.view != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: normal_view not destroyed" << std::endl;
    }
    if (ao_target.view != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: ao_view not destroyed" << std::endl;
    }
    if (ao_blur_target.view != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: ao_blur_view not destroyed" << std::endl;
    }
    if (depth_target.image.handle != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: depth_image not destroyed" << std::endl;
    }
    if (albedo_target.image.handle != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: albedo_image not destroyed" << std::endl;
    }
    if (normal_target.image.handle != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: normal_image not destroyed" << std::endl;
    }
    if (ao_target.image.handle != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: ao_image not destroyed" << std::endl;
    }
    if (ao_blur_target.image.handle != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: ao_blur_image not destroyed" << std::endl;
    }
    if (gbuffer_sampler != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: gbuffer_sampler not destroyed" << std::endl;
    }
}

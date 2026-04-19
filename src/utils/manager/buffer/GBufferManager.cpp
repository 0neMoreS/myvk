#include "GBufferManager.hpp"

#include "RenderTarget.hpp"

#include <array>
#include <cassert>
#include <iostream>

void GBufferManager::create(RTG &rtg, RenderPassManager &) {
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
    assert(render_pass_manager.depth_format != VK_FORMAT_UNDEFINED);

    if (ao_blur_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rtg.device, ao_blur_framebuffer, nullptr);
        ao_blur_framebuffer = VK_NULL_HANDLE;
    }
    if (ao_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rtg.device, ao_framebuffer, nullptr);
        ao_framebuffer = VK_NULL_HANDLE;
    }
    if (gbuffer_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rtg.device, gbuffer_framebuffer, nullptr);
        gbuffer_framebuffer = VK_NULL_HANDLE;
    }

    BufferRenderTarget::Target2D depth_target{
        .image = std::move(depth_image),
        .view = depth_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, depth_target);
    depth_image = std::move(depth_target.image);
    depth_view = depth_target.view;

    BufferRenderTarget::Target2D albedo_target{
        .image = std::move(albedo_image),
        .view = albedo_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, albedo_target);
    albedo_image = std::move(albedo_target.image);
    albedo_view = albedo_target.view;

    BufferRenderTarget::Target2D normal_target{
        .image = std::move(normal_image),
        .view = normal_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, normal_target);
    normal_image = std::move(normal_target.image);
    normal_view = normal_target.view;

    BufferRenderTarget::Target2D ao_target{
        .image = std::move(ao_image),
        .view = ao_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, ao_target);
    ao_image = std::move(ao_target.image);
    ao_view = ao_target.view;

    BufferRenderTarget::Target2D ao_blur_target{
        .image = std::move(ao_blur_image),
        .view = ao_blur_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, ao_blur_target);
    ao_blur_image = std::move(ao_blur_target.image);
    ao_blur_view = ao_blur_target.view;

    auto new_depth = BufferRenderTarget::create_target_2d(
        rtg,
        extent,
        render_pass_manager.depth_format,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_NULL_HANDLE
    );
    depth_image = std::move(new_depth.image);
    depth_view = new_depth.view;

    auto new_albedo = BufferRenderTarget::create_target_2d(
        rtg,
        extent,
        render_pass_manager.albedo_format,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_NULL_HANDLE
    );
    albedo_image = std::move(new_albedo.image);
    albedo_view = new_albedo.view;

    auto new_normal = BufferRenderTarget::create_target_2d(
        rtg,
        extent,
        render_pass_manager.normal_format,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_NULL_HANDLE
    );
    normal_image = std::move(new_normal.image);
    normal_view = new_normal.view;

    auto new_ao = BufferRenderTarget::create_target_2d(
        rtg,
        extent,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        render_pass_manager.ao_render_pass
    );
    ao_image = std::move(new_ao.image);
    ao_view = new_ao.view;
    ao_framebuffer = new_ao.framebuffer;

    auto new_ao_blur = BufferRenderTarget::create_target_2d(
        rtg,
        extent,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        render_pass_manager.ao_render_pass
    );
    ao_blur_image = std::move(new_ao_blur.image);
    ao_blur_view = new_ao_blur.view;
    ao_blur_framebuffer = new_ao_blur.framebuffer;

    std::vector<VkImageView> gbuffer_attachments{
        albedo_view,
        normal_view,
        depth_view,
    };
    gbuffer_framebuffer = BufferRenderTarget::create_framebuffer(
        rtg,
        render_pass_manager.gbuffer_render_pass,
        extent,
        gbuffer_attachments
    );
}

void GBufferManager::destroy(RTG &rtg) {
    if (ao_blur_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rtg.device, ao_blur_framebuffer, nullptr);
        ao_blur_framebuffer = VK_NULL_HANDLE;
    }
    if (ao_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rtg.device, ao_framebuffer, nullptr);
        ao_framebuffer = VK_NULL_HANDLE;
    }
    if (gbuffer_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rtg.device, gbuffer_framebuffer, nullptr);
        gbuffer_framebuffer = VK_NULL_HANDLE;
    }

    BufferRenderTarget::Target2D depth_target{
        .image = std::move(depth_image),
        .view = depth_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, depth_target);
    depth_image = std::move(depth_target.image);
    depth_view = depth_target.view;

    BufferRenderTarget::Target2D albedo_target{
        .image = std::move(albedo_image),
        .view = albedo_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, albedo_target);
    albedo_image = std::move(albedo_target.image);
    albedo_view = albedo_target.view;

    BufferRenderTarget::Target2D normal_target{
        .image = std::move(normal_image),
        .view = normal_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, normal_target);
    normal_image = std::move(normal_target.image);
    normal_view = normal_target.view;

    BufferRenderTarget::Target2D ao_target{
        .image = std::move(ao_image),
        .view = ao_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, ao_target);
    ao_image = std::move(ao_target.image);
    ao_view = ao_target.view;

    BufferRenderTarget::Target2D ao_blur_target{
        .image = std::move(ao_blur_image),
        .view = ao_blur_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, ao_blur_target);
    ao_blur_image = std::move(ao_blur_target.image);
    ao_blur_view = ao_blur_target.view;

    if (gbuffer_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.device, gbuffer_sampler, nullptr);
        gbuffer_sampler = VK_NULL_HANDLE;
    }
}

std::array<VkDescriptorImageInfo, 3> GBufferManager::get_descriptor_image_infos() const {
    return {
        VkDescriptorImageInfo{
            .sampler = gbuffer_sampler,
            .imageView = depth_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = gbuffer_sampler,
            .imageView = albedo_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = gbuffer_sampler,
            .imageView = normal_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };
}

VkDescriptorImageInfo GBufferManager::get_ao_descriptor_image_info() const {
    return VkDescriptorImageInfo{
        .sampler = gbuffer_sampler,
        .imageView = ao_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
}

GBufferManager::~GBufferManager() {
    if (ao_blur_framebuffer != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: ao_blur_framebuffer not destroyed" << std::endl;
    }
    if (ao_framebuffer != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: ao_framebuffer not destroyed" << std::endl;
    }
    if (gbuffer_framebuffer != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: gbuffer_framebuffer not destroyed" << std::endl;
    }
    if (depth_view != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: depth_view not destroyed" << std::endl;
    }
    if (albedo_view != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: albedo_view not destroyed" << std::endl;
    }
    if (normal_view != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: normal_view not destroyed" << std::endl;
    }
    if (ao_view != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: ao_view not destroyed" << std::endl;
    }
    if (ao_blur_view != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: ao_blur_view not destroyed" << std::endl;
    }
    if (depth_image.handle != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: depth_image not destroyed" << std::endl;
    }
    if (albedo_image.handle != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: albedo_image not destroyed" << std::endl;
    }
    if (normal_image.handle != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: normal_image not destroyed" << std::endl;
    }
    if (ao_image.handle != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: ao_image not destroyed" << std::endl;
    }
    if (ao_blur_image.handle != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: ao_blur_image not destroyed" << std::endl;
    }
    if (gbuffer_sampler != VK_NULL_HANDLE) {
        std::cerr << "GBufferManager: gbuffer_sampler not destroyed" << std::endl;
    }
}

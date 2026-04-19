#include "RenderTarget.hpp"

#include <cassert>

namespace BufferRenderTarget {

namespace {

VkImageView create_image_view(
    RTG &rtg,
    VkImage image,
    VkImageViewType view_type,
    VkFormat format,
    VkImageAspectFlags aspect,
    uint32_t base_layer,
    uint32_t layer_count
) {
    VkImageView image_view = VK_NULL_HANDLE;

    VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = view_type,
        .format = format,
        .subresourceRange{
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = base_layer,
            .layerCount = layer_count,
        },
    };

    VK(vkCreateImageView(rtg.device, &create_info, nullptr, &image_view));
    return image_view;
}

} // namespace

Target2D create_target_2d(
    RTG &rtg,
    VkExtent2D const &extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect,
    VkRenderPass render_pass,
    VkImageCreateFlags image_create_flags
) {
    Target2D target{};

    target.image = rtg.helpers.create_image(
        extent,
        format,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        image_create_flags,
        1,
        1
    );

    target.view = create_image_view(
        rtg,
        target.image.handle,
        VK_IMAGE_VIEW_TYPE_2D,
        format,
        aspect,
        0,
        1
    );

    if (render_pass != VK_NULL_HANDLE) {
        std::vector<VkImageView> attachments{ target.view };
        target.framebuffer = create_framebuffer(rtg, render_pass, extent, attachments);
    }

    return target;
}

TargetArray create_target_array(
    RTG &rtg,
    VkExtent2D const &extent,
    uint32_t layer_count,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect,
    VkRenderPass render_pass,
    VkImageCreateFlags image_create_flags
) {
    assert(layer_count > 0);

    TargetArray target{};

    target.image = rtg.helpers.create_image(
        extent,
        format,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        image_create_flags,
        1,
        layer_count
    );

    target.array_view = create_image_view(
        rtg,
        target.image.handle,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        format,
        aspect,
        0,
        layer_count
    );

    target.layer_views.resize(layer_count, VK_NULL_HANDLE);
    target.layer_framebuffers.resize(layer_count, VK_NULL_HANDLE);

    for (uint32_t layer = 0; layer < layer_count; ++layer) {
        target.layer_views[layer] = create_image_view(
            rtg,
            target.image.handle,
            VK_IMAGE_VIEW_TYPE_2D,
            format,
            aspect,
            layer,
            1
        );

        if (render_pass != VK_NULL_HANDLE) {
            std::vector<VkImageView> attachments{ target.layer_views[layer] };
            target.layer_framebuffers[layer] = create_framebuffer(rtg, render_pass, extent, attachments);
        }
    }

    return target;
}

TargetCube create_target_cube(
    RTG &rtg,
    VkExtent2D const &extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect,
    VkRenderPass render_pass
) {
    TargetCube target{};

    target.image = rtg.helpers.create_image(
        extent,
        format,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        1,
        6
    );

    target.cube_view = create_image_view(
        rtg,
        target.image.handle,
        VK_IMAGE_VIEW_TYPE_CUBE,
        format,
        aspect,
        0,
        6
    );

    for (uint32_t face = 0; face < 6; ++face) {
        target.face_views[face] = create_image_view(
            rtg,
            target.image.handle,
            VK_IMAGE_VIEW_TYPE_2D,
            format,
            aspect,
            face,
            1
        );

        if (render_pass != VK_NULL_HANDLE) {
            std::vector<VkImageView> attachments{ target.face_views[face] };
            target.face_framebuffers[face] = create_framebuffer(rtg, render_pass, extent, attachments);
        }
    }

    return target;
}

void destroy_target_2d(RTG &rtg, Target2D &target) {
    if (target.framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rtg.device, target.framebuffer, nullptr);
        target.framebuffer = VK_NULL_HANDLE;
    }

    if (target.view != VK_NULL_HANDLE) {
        vkDestroyImageView(rtg.device, target.view, nullptr);
        target.view = VK_NULL_HANDLE;
    }

    if (target.image.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_image(std::move(target.image));
    }
}

void destroy_target_array(RTG &rtg, TargetArray &target) {
    for (VkFramebuffer &framebuffer : target.layer_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
    target.layer_framebuffers.clear();

    for (VkImageView &image_view : target.layer_views) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(rtg.device, image_view, nullptr);
            image_view = VK_NULL_HANDLE;
        }
    }
    target.layer_views.clear();

    if (target.array_view != VK_NULL_HANDLE) {
        vkDestroyImageView(rtg.device, target.array_view, nullptr);
        target.array_view = VK_NULL_HANDLE;
    }

    if (target.image.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_image(std::move(target.image));
    }
}

void destroy_target_cube(RTG &rtg, TargetCube &target) {
    for (VkFramebuffer &framebuffer : target.face_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }

    for (VkImageView &image_view : target.face_views) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(rtg.device, image_view, nullptr);
            image_view = VK_NULL_HANDLE;
        }
    }

    if (target.cube_view != VK_NULL_HANDLE) {
        vkDestroyImageView(rtg.device, target.cube_view, nullptr);
        target.cube_view = VK_NULL_HANDLE;
    }

    if (target.image.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_image(std::move(target.image));
    }
}

VkFramebuffer create_framebuffer(
    RTG &rtg,
    VkRenderPass render_pass,
    VkExtent2D const &extent,
    std::vector<VkImageView> const &attachments
) {
    assert(render_pass != VK_NULL_HANDLE);
    assert(!attachments.empty());

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkFramebufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = extent.width,
        .height = extent.height,
        .layers = 1,
    };

    VK(vkCreateFramebuffer(rtg.device, &create_info, nullptr, &framebuffer));
    return framebuffer;
}

} // namespace BufferRenderTarget

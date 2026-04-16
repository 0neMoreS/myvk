#include "GBufferManager.hpp"

#include <array>

void GBufferManager::create(RTG &rtg, VkDescriptorPool descriptor_pool, VkDescriptorSetLayout descriptor_set_layout) {
    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_set_layout,
    };

    VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &descriptor_set));
}

void GBufferManager::set_views_and_sampler(
    VkSampler in_sampler,
    VkImageView depth,
    VkImageView albedo,
    VkImageView normal,
    VkImageView pbr
) {
    sampler = in_sampler;
    depth_view = depth;
    albedo_view = albedo;
    normal_view = normal;
    pbr_view = pbr;
}

void GBufferManager::create_targets(
    RTG &rtg,
    VkExtent2D extent,
    VkRenderPass gbuffer_render_pass
) {
    destroy_targets(rtg);

    depth_image = rtg.helpers.create_image(
        extent,
        depth_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        0,
        1,
        1
    );

    albedo_image = rtg.helpers.create_image(
        extent,
        albedo_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        0,
        1,
        1
    );

    normal_image = rtg.helpers.create_image(
        extent,
        normal_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        0,
        1,
        1
    );

    pbr_image = rtg.helpers.create_image(
        extent,
        pbr_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        0,
        1,
        1
    );

    { // depth view
        VkImageViewCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = depth_image.handle,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = depth_format,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
        };

        VK(vkCreateImageView(rtg.device, &create_info, nullptr, &depth_view));
    }

    auto create_color_view = [&](VkImage image, VkFormat format, VkImageView &out_view) {
        VkImageViewCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
        };

        VK(vkCreateImageView(rtg.device, &create_info, nullptr, &out_view));
    };

    create_color_view(albedo_image.handle, albedo_format, albedo_view);
    create_color_view(normal_image.handle, normal_format, normal_view);
    create_color_view(pbr_image.handle, pbr_format, pbr_view);

    { // gbuffer framebuffer
        std::array<VkImageView, 4> attachments{
            albedo_view,
            normal_view,
            pbr_view,
            depth_view,
        };

        VkFramebufferCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = gbuffer_render_pass,
            .attachmentCount = uint32_t(attachments.size()),
            .pAttachments = attachments.data(),
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        VK(vkCreateFramebuffer(rtg.device, &create_info, nullptr, &framebuffer));
    }

    { // sampler
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
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };

        VK(vkCreateSampler(rtg.device, &sampler_info, nullptr, &sampler));
    }
}

void GBufferManager::destroy_targets(RTG &rtg) {
    if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
    }

    if (depth_view != VK_NULL_HANDLE) {
        vkDestroyImageView(rtg.device, depth_view, nullptr);
        depth_view = VK_NULL_HANDLE;
    }
    if (albedo_view != VK_NULL_HANDLE) {
        vkDestroyImageView(rtg.device, albedo_view, nullptr);
        albedo_view = VK_NULL_HANDLE;
    }
    if (normal_view != VK_NULL_HANDLE) {
        vkDestroyImageView(rtg.device, normal_view, nullptr);
        normal_view = VK_NULL_HANDLE;
    }
    if (pbr_view != VK_NULL_HANDLE) {
        vkDestroyImageView(rtg.device, pbr_view, nullptr);
        pbr_view = VK_NULL_HANDLE;
    }

    if (depth_image.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_image(std::move(depth_image));
    }
    if (albedo_image.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_image(std::move(albedo_image));
    }
    if (normal_image.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_image(std::move(normal_image));
    }
    if (pbr_image.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_image(std::move(pbr_image));
    }

    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
}

void GBufferManager::update(VkDevice device) {
    if (descriptor_set == VK_NULL_HANDLE) {
        return;
    }

    if (sampler == VK_NULL_HANDLE ||
        depth_view == VK_NULL_HANDLE ||
        albedo_view == VK_NULL_HANDLE ||
        normal_view == VK_NULL_HANDLE ||
        pbr_view == VK_NULL_HANDLE) {
        return;
    }

    std::array<VkDescriptorImageInfo, 4> image_infos{
        VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = depth_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = albedo_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = normal_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = pbr_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };

    std::array<VkWriteDescriptorSet, 4> writes{
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_infos[0],
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_infos[1],
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_infos[2],
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 3,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_infos[3],
        },
    };

    vkUpdateDescriptorSets(device, uint32_t(writes.size()), writes.data(), 0, nullptr);
}

void GBufferManager::destroy(RTG &rtg) {
    destroy_targets(rtg);

    descriptor_set = VK_NULL_HANDLE;

    depth_format = VK_FORMAT_UNDEFINED;
}
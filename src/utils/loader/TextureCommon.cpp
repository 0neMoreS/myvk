#include "TextureCommon.hpp"

#include "VK.hpp"

#include <stdexcept>
#include <string>

namespace TextureCommon {

VkFormat channel_count_to_format(int channels) {
    switch (channels) {
        case 1: return VK_FORMAT_R8_UNORM;
        case 2: return VK_FORMAT_R8G8_UNORM;
        case 3: return VK_FORMAT_R8G8B8_UNORM;
        case 4: return VK_FORMAT_R8G8B8A8_UNORM;
        default:
            throw std::runtime_error("Unsupported number of channels: " + std::to_string(channels));
    }
}

VkSampler create_sampler(
    VkDevice device,
    VkFilter filter,
    VkSamplerAddressMode address_mode_u,
    VkSamplerAddressMode address_mode_v,
    VkSamplerAddressMode address_mode_w
) {
    VkSamplerCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = filter,
        .minFilter = filter,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = address_mode_u,
        .addressModeV = address_mode_v,
        .addressModeW = address_mode_w,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkSampler sampler;
    VK( vkCreateSampler(device, &create_info, nullptr, &sampler) );
    return sampler;
}

VkImageView create_image_view(
    VkDevice device,
    VkImage image,
    VkFormat format,
    bool cube
) {
    VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = cube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = cube ? 6u : 1u,
        },
    };

    VkImageView image_view;
    VK( vkCreateImageView(device, &create_info, nullptr, &image_view) );
    return image_view;
}

} // namespace TextureCommon

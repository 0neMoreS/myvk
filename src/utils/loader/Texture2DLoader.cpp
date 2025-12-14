#include "Texture2DLoader.hpp"

#include "VK.hpp"
#include "RTG.hpp"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image.h>
#include <iostream>
#include <stdexcept>
#include <memory>

namespace Texture2DLoader {

namespace {

VkFormat channel_count_to_format(int channels) {
	switch (channels) {
		case 1:
			return VK_FORMAT_R8_UNORM;
		case 2:
			return VK_FORMAT_R8G8_UNORM;
		case 3:
			return VK_FORMAT_R8G8B8_UNORM;
		case 4:
			return VK_FORMAT_R8G8B8A8_UNORM;
		default:
			throw std::runtime_error("Unsupported number of channels: " + std::to_string(channels));
	}
}

VkImageView create_image_view(
	VkDevice device,
	VkImage image,
	VkFormat format
) {
	VkImageViewCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .flags = 0,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
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
			.layerCount = 1,
		},
	};

	VkImageView image_view;
	VK( vkCreateImageView(device, &create_info, nullptr, &image_view) );
	return image_view;
}

VkSampler create_sampler(VkDevice device, VkFilter filter) {
	VkSamplerCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = filter,
		.minFilter = filter,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
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

} // namespace

std::shared_ptr<Texture> load_image(
	Helpers &helpers,
	const std::string &filepath,
	int force_channels,
	VkFilter filter
) {
	// Load image file using stb_image
	int width, height, channels;
	
	stbi_set_flip_vertically_on_load(true);
	// stbi_load will automatically convert to the desired number of channels
	unsigned char *pixel_data = stbi_load(
		filepath.c_str(),
		&width,
		&height,
		&channels,
		force_channels
	);

	if (!pixel_data) {
		std::string error_msg = std::string("Failed to load image: ") + filepath;
		if (stbi_failure_reason()) {
			error_msg += std::string(" - ") + stbi_failure_reason();
		}
		throw std::runtime_error(error_msg);
	}

	// If force_channels is set, use that; otherwise use what stb_image detected
	if (force_channels > 0) {
		channels = force_channels;
	}

	try {
		// Determine format from channel count
		VkFormat format = channel_count_to_format(channels);

		// Create GPU texture resource
		auto texture = std::make_shared<Texture>();

		// Calculate image size in bytes
		size_t image_size = width * height * channels;

		// Create GPU image with transfer destination flag
		texture->image = helpers.create_image(
			VkExtent2D{.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)},
			format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            Helpers::Unmapped
		);

		helpers.transfer_to_image(pixel_data, image_size, texture->image);
		texture->image_view = create_image_view(helpers.rtg.device, texture->image.handle, format);
		texture->sampler = create_sampler(helpers.rtg.device, filter);

		// Free CPU-side pixel data
		stbi_image_free(pixel_data);

		return texture;

	} catch (const std::exception &e) {
		stbi_image_free(pixel_data);
		throw;
	}
}

std::shared_ptr<Texture> load_png(
	Helpers &helpers,
	const std::string &filepath,
	VkFilter filter
) {
	// Use load_image with force_channels = 4 for RGBA
	return load_image(helpers, filepath, 4, filter);
}

std::shared_ptr<Texture> create_rgb_texture(
    Helpers &helpers,
    const glm::vec3 &color,
    VkFilter filter
) {
    uint8_t pixel_data[4] = {
        static_cast<uint8_t>(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f),
        255, // alpha = 1.0
    };

    auto texture = std::make_shared<Texture>();
    
    texture->image = helpers.create_image(
        VkExtent2D{.width = 1, .height = 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped
    );

    helpers.transfer_to_image(pixel_data, 1 * 1 * 4, texture->image);
    texture->image_view = create_image_view(helpers.rtg.device, texture->image.handle, VK_FORMAT_R8G8B8A8_UNORM);
    texture->sampler = create_sampler(helpers.rtg.device, filter);

    return texture;
}

void destroy(const std::shared_ptr<Texture> &texture, RTG& rtg) {
	if (!texture) return;

	if (texture->sampler != VK_NULL_HANDLE) {
		vkDestroySampler(rtg.helpers.rtg.device, texture->sampler, nullptr);
		texture->sampler = VK_NULL_HANDLE;
	}

	if (texture->image_view != VK_NULL_HANDLE) {
		vkDestroyImageView(rtg.helpers.rtg.device, texture->image_view, nullptr);
		texture->image_view = VK_NULL_HANDLE;
	}

	if (texture->image.handle != VK_NULL_HANDLE) {
		rtg.helpers.destroy_image(std::move(texture->image));
	}
}

} // namespace Texture2DLoader

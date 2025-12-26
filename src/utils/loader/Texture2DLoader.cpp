#include "Texture2DLoader.hpp"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image.h>
#include <iostream>
#include <stdexcept>
#include <memory>

namespace Texture2DLoader {

namespace {
// no-internal helpers: kept empty to avoid anonymous namespace removal warnings
} // namespace

std::shared_ptr<Texture> load_image(
	Helpers &helpers,
	const std::string &filepath,
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
		4
	);

	if (!pixel_data) {
		std::string error_msg = std::string("Failed to load image: ") + filepath;
		if (stbi_failure_reason()) {
			error_msg += std::string(" - ") + stbi_failure_reason();
		}
		throw std::runtime_error(error_msg);
	}

	std::vector<float> rgba_data(width * height * 4);

	for(size_t i = 0; i < static_cast<size_t>(width * height); ++i) {
		TextureCommon::decode_rgbe(
			pixel_data + i * 4,
			rgba_data.data() + i * 4
		);
	}

	// Create GPU texture resource
	auto texture = std::make_shared<Texture>();

	// Create GPU image with transfer destination flag
	texture->image = helpers.create_image(
		VkExtent2D{.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)},
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped
	);

	helpers.transfer_to_image(rgba_data.data(), rgba_data.size() * sizeof(float), texture->image);
	
	texture->image_view = TextureCommon::create_image_view(helpers.rtg.device, texture->image.handle, VK_FORMAT_R32G32B32A32_SFLOAT, false);
	texture->sampler = TextureCommon::create_sampler(
		helpers.rtg.device,
		filter,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT
	);

	// Free CPU-side pixel data
	stbi_image_free(pixel_data);

	return texture;
}

std::shared_ptr<Texture> load_png(
	Helpers &helpers,
	const std::string &filepath,
	VkFilter filter
) {
	return load_image(helpers, filepath, filter);
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
	texture->image_view = TextureCommon::create_image_view(helpers.rtg.device, texture->image.handle, VK_FORMAT_R8G8B8A8_UNORM, false);
	texture->sampler = TextureCommon::create_sampler(
		helpers.rtg.device,
		filter,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT
	);

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

Texture::~Texture() {
	if (sampler != VK_NULL_HANDLE || image_view != VK_NULL_HANDLE) {
		std::cerr << "[Texture2DLoader] Texture destructor called without destroy() being called" << std::endl;
	}
}

} // namespace Texture2DLoader

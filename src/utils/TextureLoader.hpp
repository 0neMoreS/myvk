#pragma once

#include "Helpers.hpp"

#include <string>
#include <memory>

namespace TextureLoader {

struct Texture {
	Helpers::AllocatedImage image;
	VkImageView image_view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
};

std::shared_ptr<Texture> load_png(
	Helpers &helpers,
	const std::string &filepath,
	VkFilter filter = VK_FILTER_LINEAR
);

std::shared_ptr<Texture> load_image(
	Helpers &helpers,
	const std::string &filepath,
	int force_channels = 4,
	VkFilter filter = VK_FILTER_LINEAR
);

void destroy_texture(const std::shared_ptr<Texture> &texture, VkDevice device);

} // namespace TextureLoader

#pragma once

#include "Helpers.hpp"

#include <string>
#include <memory>
#include <glm/glm.hpp>

namespace Texture2DLoader {

struct Texture {
	Helpers::AllocatedImage image;
	VkImageView image_view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;

	Texture() = default;
	~Texture();
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

std::shared_ptr<Texture> create_rgb_texture(
    Helpers &helpers,
    const glm::vec3 &color,
    VkFilter filter = VK_FILTER_LINEAR
);

void destroy(const std::shared_ptr<Texture> &texture, RTG& rtg);

} // namespace Texture2DLoader

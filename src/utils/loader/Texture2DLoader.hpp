#pragma once

#include "Helpers.hpp"
#include "VK.hpp"
#include "RTG.hpp"
#include "TextureCommon.hpp"

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

std::unique_ptr<Texture> load_image(
	Helpers &helpers,
	const std::string &filepath,
	VkFilter filter = VK_FILTER_LINEAR,
	bool srgb = false
);

std::unique_ptr<Texture> create_rgb_texture(
    Helpers &helpers,
    const glm::vec3 &color,
    VkFilter filter = VK_FILTER_LINEAR,
	bool srgb = false
);

void destroy(std::unique_ptr<Texture> texture, RTG& rtg);

} // namespace Texture2DLoader

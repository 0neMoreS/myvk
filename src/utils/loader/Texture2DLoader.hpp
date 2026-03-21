#pragma once

#include "Helpers.hpp"
#include "VK.hpp"
#include "RTG.hpp"
#include "TextureCommon.hpp"

#include <string>
#include <memory>
#include <glm/glm.hpp>

namespace Texture2DLoader {

std::unique_ptr<TextureCommon::Texture> load_image(
	Helpers &helpers,
	const std::string &filepath,
	VkFilter filter = VK_FILTER_LINEAR,
	bool srgb = false,
	bool generate_mipmaps = false
);

std::unique_ptr<TextureCommon::Texture> create_rgb_texture(
    Helpers &helpers,
    const glm::vec3 &color,
    VkFilter filter = VK_FILTER_LINEAR
);

} // namespace Texture2DLoader

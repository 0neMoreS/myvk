#pragma once

#include <vulkan/vk_enum_string_helper.h>

#include <stdexcept>

#define VK( FN ) \
	if (VkResult result = FN; result != VK_SUCCESS) { \
		throw std::runtime_error("Call '" #FN "' returned " + std::to_string(result) + " [" + std::string(string_VkResult(result)) + "]." ); \
	}

// Coordinate system conversion matrix: Blender → Vulkan
static const glm::mat3 BLENDER_TO_VULKAN_3 = glm::mat3(
    0.0f,  0.0f,  -1.0f,
    1.0f,  0.0f,  0.0f,
    0.0f, -1.0f,  0.0f
);

static const glm::mat4 BLENDER_TO_VULKAN_4 = glm::mat4(
    0.0f,  0.0f,  -1.0f, 0.0f,
    1.0f,  0.0f,  0.0f, 0.0f,
    0.0f, -1.0f,  0.0f, 0.0f,
    0.0f,  0.0f,  0.0f, 1.0f
);

// Coordinate system conversion matrix: Vulkan → Blender
static const glm::mat3 VULKAN_TO_BLENDER_3 = glm::mat3(
    0.0f,  1.0f,  0.0f,
    0.0f,  0.0f, -1.0f,
   -1.0f,  0.0f,  0.0f
);

static const glm::mat4 VULKAN_TO_BLENDER_4 = glm::mat4(
    0.0f,  1.0f,  0.0f, 0.0f,
    0.0f,  0.0f, -1.0f, 0.0f,
   -1.0f,  0.0f,  0.0f, 0.0f,
    0.0f,  0.0f,  0.0f, 1.0f
);


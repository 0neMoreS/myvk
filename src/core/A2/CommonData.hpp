#pragma once

#include <vulkan/vulkan.h>
#include "VK.hpp"
#include "glm/glm.hpp"

namespace CommonData
{
    struct PV{
        glm::mat4 PERSPECTIVE;
        glm::mat4 VIEW;
        glm::vec4 LIGHT_POSITION;
        glm::vec4 CAMERA_POSITION;
    };

    static_assert(sizeof(PV) == 16*4 + 16*4 + 4*4 + 4*4, "PV is the expected size.");

    //types for descriptors:
    struct Light {
        glm::vec4 LIGHT_POSITION;
        glm::vec4 LIGHT_ENERGY;
        glm::vec4 CAMERA_POSITION;
    };
    static_assert(sizeof(Light) == 4*4 + 4*4 + 4*4, "Light is the expected size.");

    //types for descriptors:
    struct Transform {
        glm::mat4 MODEL;
        glm::mat4 MODEL_NORMAL;
    };
    static_assert(sizeof(Transform) == 16*4 + 16*4, "Transform is the expected size.");
} // namespace CommonData

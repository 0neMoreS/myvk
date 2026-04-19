#pragma once

#include <vulkan/vulkan.h>
#include "VK.hpp"
#include "glm/glm.hpp"
#include <cstdint>

namespace SSDOCommonData
{
    struct PV{
        glm::mat4 PERSPECTIVE;
        glm::mat4 INV_PERSPECTIVE;
        glm::mat4 VIEW;
        glm::mat4 INV_PV;
        glm::vec4 CAMERA_POSITION;
    };

    static_assert(sizeof(PV) == 16*4 + 16*4 + 16*4 + 16*4 + 4*4, "PV is the expected size.");

    //types for descriptors:
    struct Transform {
        glm::mat4 MODEL;
        glm::mat4 MODEL_NORMAL;
    };
    static_assert(sizeof(Transform) == 16*4 + 16*4, "Transform is the expected size.");
} // namespace SSDOCommonData

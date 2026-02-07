#pragma once

#include <vulkan/vulkan.h>
#include "VK.hpp"
#include <glm/glm.hpp>

namespace A1CommonData
{
    struct PV {
        glm::mat4 PERSPECTIVE;
        glm::mat4 VIEW;
    };
    static_assert(sizeof(PV) == 16*4 + 16*4, "PV is the expected size.");

    struct World {
        struct { float x, y, z, padding_; } SKY_DIRECTION;
        struct { float r, g, b, padding_; } SKY_ENERGY;
        struct { float x, y, z, padding_; } SUN_DIRECTION;
        struct { float r, g, b, padding_; } SUN_ENERGY;
    };
    static_assert(sizeof(World) == 4*4 + 4*4 + 4*4 + 4*4, "World is the expected size.");
} // namespace A1CommonData
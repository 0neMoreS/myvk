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
} // namespace A1CommonData
#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

struct PosVertex {
	struct { float x,y,z; } Position;

    //a pipeline vertex input state that works with a buffer holding a PosVertex[] array:
	static const VkPipelineVertexInputStateCreateInfo array_input_state;
};

static_assert(sizeof(PosVertex) == 3*4, "PosVertex is packed.");
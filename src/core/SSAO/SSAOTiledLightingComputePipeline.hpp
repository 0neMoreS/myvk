#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"

struct SSAOTiledLightingComputePipeline : Pipeline {
    VkDescriptorSetLayout set0_Global = VK_NULL_HANDLE;

	VkShaderModule comp_module = VK_NULL_HANDLE;

    struct Push {
        uint32_t render_width;
        uint32_t render_height;
        uint32_t tiles_x;
        uint32_t tiles_y;
    };

    void create(
		RTG &rtg, 
		VkRenderPass render_pass, 
		uint32_t subpass,
        const ManagerContext& context
	) override;
    void destroy(RTG &rtg) override;

    SSAOTiledLightingComputePipeline() = default;
    ~SSAOTiledLightingComputePipeline();
};

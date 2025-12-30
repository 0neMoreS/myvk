#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"

#include <glm/glm.hpp>

struct A2BackgroundPipeline : Pipeline {
    // External cubemap descriptor set layout (allocated elsewhere, not owned)
    VkDescriptorSetLayout set0_PV = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_CUBEMAP = VK_NULL_HANDLE;

    void create(
		class RTG &, 
		VkRenderPass render_pass, 
		uint32_t subpass,
		const TextureManager& texture_manager
	) override;
    void destroy(RTG &rtg) override;

    A2BackgroundPipeline() = default;
    ~A2BackgroundPipeline();
};

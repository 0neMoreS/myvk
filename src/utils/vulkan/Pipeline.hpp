#pragma once

#include <vector>
#include <vulkan/vulkan.h>

struct DescriptorConfig
{
    VkDescriptorType type;
    VkDescriptorSetLayout set_layout;
    VkDeviceSize size;
};


struct Pipeline
{
    std::vector<DescriptorConfig> descriptor_configs{};
    VkPipelineLayout layout = VK_NULL_HANDLE;	
    VkPipeline pipeline = VK_NULL_HANDLE;

    virtual void create(class RTG &, VkRenderPass render_pass, uint32_t subpass) = 0;
    virtual void destroy(class RTG &) = 0;
};

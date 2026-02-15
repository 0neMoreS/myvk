#include "QueryPoolManager.hpp"

void QueryPoolManager::create(RTG &rtg, uint32_t workspace_count_) {
    workspace_count = workspace_count_;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(rtg.physical_device, &properties);
    timestamp_period = properties.limits.timestampPeriod;
    timing_enabled = properties.limits.timestampComputeAndGraphics == VK_TRUE;

    if (!timing_enabled || workspace_count == 0) {
        return;
    }

    VkQueryPoolCreateInfo query_pool_info{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = static_cast<uint32_t>(workspace_count * 2)
    };

    VK(vkCreateQueryPool(rtg.device, &query_pool_info, nullptr, &query_pool));
}

void QueryPoolManager::destroy(RTG &rtg) {
    if (query_pool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(rtg.device, query_pool, nullptr);
        query_pool = VK_NULL_HANDLE;
    }
    timing_enabled = false;
    timestamp_period = 0.0f;
    workspace_count = 0;
}

void QueryPoolManager::begin_frame(VkCommandBuffer command_buffer, uint32_t workspace_index) {
    if (!timing_enabled || query_pool == VK_NULL_HANDLE) {
        return;
    }

    uint32_t query_base = workspace_index * 2;
    vkCmdResetQueryPool(command_buffer, query_pool, query_base, 2);
    vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, query_base);
}

void QueryPoolManager::end_frame(VkCommandBuffer command_buffer, uint32_t workspace_index) {
    if (!timing_enabled || query_pool == VK_NULL_HANDLE) {
        return;
    }

    uint32_t query_base = workspace_index * 2;
    vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool, query_base + 1);
}

bool QueryPoolManager::fetch_frame_ms(RTG &rtg, uint32_t workspace_index, double &out_ms) const {
    if (!timing_enabled || query_pool == VK_NULL_HANDLE) {
        return false;
    }

    uint64_t results[4] = {0, 0, 0, 0};
    uint32_t query_base = workspace_index * 2;
    VkResult query_result = vkGetQueryPoolResults(
        rtg.device,
        query_pool,
        query_base,
        2,
        sizeof(results),
        results,
        sizeof(uint64_t) * 2,
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT | VK_QUERY_RESULT_WAIT_BIT
    );

    if (query_result != VK_SUCCESS || results[1] == 0 || results[3] == 0) {
        return false;
    }

    uint64_t delta = results[2] - results[0];
    out_ms = (static_cast<double>(delta) * timestamp_period) / 1000000.0;
    return true;
}
